"""Assemble the randomizer disc image and PPF from a vanilla Vagrant Story (USA) BIN.

1. Append the resident payload to SLUS_010.40 and point the exe entry at it.
   The boot menu is a separate binary in free sectors, loaded by the payload.
2. Move SLUS_010.40 into unused sectors so it can grow (it is boxed in by DBGFNT.TIM).
3. Redirect hooked calls in INITBTL.PRG, BATTLE.PRG and MENU0.PRG into the payload
   (TITLE.PRG's is installed at runtime), and apply single-instruction patches
   (ENDING.PRG, MENU0.PRG).
4. Write the patched BIN and a PPF 3.0 diff against the vanilla image.
"""
import argparse
import hashlib
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import ppf  # noqa: E402
import psxdisc  # noqa: E402

# Images the patch can be built against, by SHA-1 (the PPF then applies to that image):
# vanilla, Vagrant Story Zenith 1.94, and Zenith 1.94 + the Rood Reverse QoL mod
# v3.1 for Zenith (the hashes listed in the QoL mod's MOD.md).
BASES = {
    '38c63db6c49a06b91d87cd5a4e31b1ec68fe212c': 'vanilla',
    '02b764a6a2015af31dc75bfe1330c5e161f48dd1': 'zenith',
    '78ba22188a68c3e9f7f8f3469cc3688e678becc6': 'zenith-qol',
}
EDITION_BASES = {'vanilla': {'vanilla'}, 'zenith': {'zenith', 'zenith-qol'}}
BASE_NAMES = {'vanilla': 'Vagrant Story (USA)', 'zenith': 'Vagrant Story Zenith 1.94',
              'zenith-qol': 'Vagrant Story Zenith 1.94 + QoL mod v3.1'}

EXE_LOAD = 0x80010000
EXE_ORIG_SIZE = 0x52000
GAME_ENTRY = 0x8001F544
RESIDENT_END = 0x80068800
SLUS_NEW_LBA = 193  # start of an empty 907-sector gap after DBGFNT.TIM
MENU_LBA = 400  # boot menu binary, later in the same gap
MENU_LOAD = 0x80068800  # overlay slot 0, before TITLE.PRG is loaded
OVERLAY_LBA = 420  # battle overlay binary, after the menu in the same gap
OVERLAY_LOAD = 0x800F6000  # free RAM after BATTLE.PRG, clear of the QoL/Zenith mods' code there (see src/battle/link.ld)
OVERLAY_MAX_SECTORS = 7  # must end before overlay slot 1 at 0x800F9800
FILE_MAGIC = 0x454C4946  # rando_file_t

# (call site address, original jal target, payload symbol)
EXE_HOOKS = [
    (0x80042BD0, 0x80041F34, 'rando_beforeTitle'),  # vs_main_execTitle: _loadTitlePrg()
    (0x80043960, 0x800430F4, 'rando_onPadRead'),
]

# TITLE.PRG's New Game hook (0x800715A4 -> rando_onNewGame) is installed in RAM
# by src/payload/boot.c, so TITLE.PRG stays vanilla on disc and the QoL mod's
# edits to it keep valid sector checksums.

INITBTL_LOAD = 0x800F9800
INITBTL_HOOKS = [
    (0x800F9E2C, 0x80076F24, 'rando_onAshleyInit'),  # create Ashley from D_800FAB18
]

MENU0_LOAD = 0x80102800
MENU0_HOOKS = [
    (0x80104168, 0x801029B8, 'rando_teleportShowCost'),  # _teleportMenu: _setMPCostDirect(cost, mp < cost)
]

# Single-instruction patches: (file, load address, address, original word, new word).
# A new word given as a payload symbol name is a jal to it. MENU0.PRG patches are
# for free teleport and are left out of the Zenith edition (Zenith has its own).
WORD_PATCHES = [
    # The ending clears state flags 16..1087 before the clear save is written;
    # stop at 0x430 so the randomizer config (the last 16 bytes) survives into
    # New Game+. `slti v0, v1, 1088` -> `slti v0, v1, 0x430`.
    ('ENDING/ENDING.PRG', 0x80068800, 0x8006B814, 0x28620440, 0x28620430),
    # _teleportMenu caches the MP it greys save points out by:
    #   lh a0, 28(a0); lui v1, 0x8010; j 0x80104224; sw a0, 0x6A0C(v1)
    # -> jal rando_teleportMenuMp(a0 = stats); nop; j 0x80104224; move v0, zero
    ('MENU/MENU0.PRG', MENU0_LOAD, 0x80103E84, 0x8484001C, 'rando_teleportMenuMp'),
    ('MENU/MENU0.PRG', MENU0_LOAD, 0x80103E88, 0x3C038010, 0x00000000),
    ('MENU/MENU0.PRG', MENU0_LOAD, 0x80103E90, 0xAC646A0C, 0x00001021),
]

BATTLE_LOAD = 0x80068800
BATTLE_HOOKS = [
    (0x80088FA8, 0x80088D40, 'rando_onZndEnemies'),
    (0x80089E80, 0x8008A908, 'rando_onMpdLoaded'),
    (0x800C9840, 0x800FAAC8, 'rando_onMainMenu'),  # vs_mainMenu_exec, every frame while open
]

def jal(target):
    return 0x0C000000 | ((target >> 2) & 0x03FFFFFF)


def read_symbols(path):
    syms = {}
    for line in Path(path).read_text().splitlines():
        parts = line.split()
        if len(parts) == 3:
            syms[parts[2]] = int(parts[0], 16)
    return syms


def install_hooks(image, load_addr, hooks, syms):
    """Redirect `jal orig` at each call site to the payload symbol."""
    for addr, orig, sym in hooks:
        off = addr - load_addr
        word = struct.unpack_from('<I', image, off)[0]
        assert word == jal(orig), f'{addr:#x}: expected jal {orig:#x}, found {word:#010x}'
        struct.pack_into('<I', image, off, jal(syms[sym]))


def patch_exe(exe, payload, syms):
    hdr = bytearray(exe[:0x800])
    assert hdr[:8] == b'PS-X EXE'
    pc0, _, t_addr, t_size = struct.unpack_from('<4I', hdr, 0x10)
    assert (pc0, t_addr, t_size) == (GAME_ENTRY, EXE_LOAD, EXE_ORIG_SIZE), 'unexpected SLUS header'
    bss_end = syms['__bss_end']
    assert bss_end <= RESIDENT_END, f'payload overflows resident area by {bss_end - RESIDENT_END:#x} bytes'
    body = bytearray(exe[0x800:0x800 + EXE_ORIG_SIZE])
    install_hooks(body, EXE_LOAD, EXE_HOOKS, syms)
    body += payload
    body += bytes(-len(body) % 0x800)
    struct.pack_into('<I', hdr, 0x10, syms['rando_start'])
    struct.pack_into('<I', hdr, 0x1C, len(body))
    return bytes(hdr + body)


def patch_words(disc, syms, edition):
    for path, load, addr, old, new in WORD_PATCHES:
        if edition == 'zenith' and path == 'MENU/MENU0.PRG':
            continue
        if isinstance(new, str):
            new = jal(syms[new])
        data = bytearray(disc.read_file(path))
        off = addr - load
        word = struct.unpack_from('<I', data, off)[0]
        assert word == old, f'{path} {addr:#x}: expected {old:#010x}, found {word:#010x}'
        struct.pack_into('<I', data, off, new)
        disc.write_file(path, data)


def set_file(exe, syms, name, lba, size):
    """Fill in a rando_file_t in the exe image."""
    off = 0x800 + syms[name] - EXE_LOAD
    assert struct.unpack_from('<I', exe, off)[0] == FILE_MAGIC, f'{name} not found'
    exe = bytearray(exe)
    struct.pack_into('<II', exe, off + 4, lba, size)
    return bytes(exe)


def patch_overlay(prg, load_addr, hooks, syms):
    prg = bytearray(prg)
    install_hooks(prg, load_addr, hooks, syms)
    return bytes(prg)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--base', '--vanilla', dest='base', required=True,
                    help='image to build against: vanilla, or for --edition zenith a Zenith (+ QoL) image')
    ap.add_argument('--edition', choices=sorted(EDITION_BASES), default='vanilla')
    ap.add_argument('--payload', required=True)
    ap.add_argument('--symbols', required=True)
    ap.add_argument('--menu', required=True)
    ap.add_argument('--menu-symbols', required=True)
    ap.add_argument('--overlay', required=True)
    ap.add_argument('--out-bin', required=True)
    ap.add_argument('--out-ppf', required=True)
    ap.add_argument('--version', default='dev')
    args = ap.parse_args()

    vanilla = Path(args.base).read_bytes()  # the base image the PPF applies to
    base = BASES.get(hashlib.sha1(vanilla).hexdigest())
    if base not in EDITION_BASES[args.edition]:
        wanted = ' or '.join(BASE_NAMES[b] for b in sorted(EDITION_BASES[args.edition]))
        sys.exit(f'{args.base}: not a {wanted} image (hash mismatch)')
    syms = read_symbols(args.symbols)
    payload = Path(args.payload).read_bytes()

    disc = psxdisc.Disc(args.base)

    menu = Path(args.menu).read_bytes()
    menu_syms = read_symbols(args.menu_symbols)
    assert menu_syms['rando_menuMain'] == MENU_LOAD, 'menu entry must be first in the binary'
    menu_sectors = (len(menu) + psxdisc.USER - 1) // psxdisc.USER
    assert disc.is_free(MENU_LBA, menu_sectors), 'menu target sectors are not free'
    assert MENU_LBA + menu_sectors <= OVERLAY_LBA, 'menu would overlap the battle overlay'
    disc.write_form1(MENU_LBA, menu)

    overlay = Path(args.overlay).read_bytes()
    overlay_sectors = (len(overlay) + psxdisc.USER - 1) // psxdisc.USER
    assert overlay_sectors <= OVERLAY_MAX_SECTORS, f'battle overlay is {overlay_sectors} sectors'
    assert struct.unpack_from('<I', overlay, 0)[0] == 0x4C56524F, 'battle overlay header missing'
    assert disc.is_free(OVERLAY_LBA, overlay_sectors), 'battle overlay target sectors are not free'
    disc.write_form1(OVERLAY_LBA, overlay)

    exe = patch_exe(disc.read_file('SLUS_010.40'), payload, syms)
    exe = set_file(exe, syms, 'rando_menuFile', MENU_LBA, menu_sectors * psxdisc.USER)
    exe = set_file(exe, syms, 'rando_overlayFile', OVERLAY_LBA, overlay_sectors * psxdisc.USER)
    sectors = (len(exe) + psxdisc.USER - 1) // psxdisc.USER
    assert SLUS_NEW_LBA + sectors <= MENU_LBA, 'exe would overlap the menu'
    assert disc.is_free(SLUS_NEW_LBA, sectors), 'relocation target is not free'
    disc.relocate('SLUS_010.40', SLUS_NEW_LBA, exe)

    overlays = [('BATTLE/INITBTL.PRG', INITBTL_LOAD, INITBTL_HOOKS), ('BATTLE/BATTLE.PRG', BATTLE_LOAD, BATTLE_HOOKS)]
    if args.edition == 'vanilla':
        overlays.append(('MENU/MENU0.PRG', MENU0_LOAD, MENU0_HOOKS))
    for path, load, hooks in overlays:
        disc.write_file(path, patch_overlay(disc.read_file(path), load, hooks, syms))
    patch_words(disc, syms, args.edition)

    out = bytes(disc.img)
    Path(args.out_bin).write_bytes(out)
    Path(args.out_bin).with_suffix('.cue').write_text(
        f'FILE "{Path(args.out_bin).name}" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n')
    title = f'VS Integrated Randomizer v{args.version}' + {'vanilla': '', 'zenith': ' for Zenith 1.94',
                                                            'zenith-qol': ' for Zenith 1.94+QoL'}[base]
    ppf.create(vanilla, out, title[:50], args.out_ppf)

    # Verify: the patch reproduces the image and the relocated exe reads back.
    check = ppf.apply(bytearray(vanilla), Path(args.out_ppf).read_bytes())
    assert check == out, 'PPF round trip mismatch'
    assert psxdisc.Disc(args.out_bin).read_file('SLUS_010.40') == exe

    used = syms['__bss_end'] - 0x80062000
    print(f'payload: {len(payload)} bytes file, {used} of {RESIDENT_END - 0x80062000} bytes resident used')
    print(f'SLUS_010.40: {len(exe)} bytes at LBA {SLUS_NEW_LBA}; entry {syms["rando_start"]:#x}')
    print(f'boot menu: {len(menu)} bytes at LBA {MENU_LBA}')
    print(f'battle overlay: {len(overlay)} of {OVERLAY_MAX_SECTORS * psxdisc.USER} bytes at LBA {OVERLAY_LBA}')
    print(f'wrote {args.out_bin} and {args.out_ppf} ({Path(args.out_ppf).stat().st_size} bytes), for {BASE_NAMES[base]}')


if __name__ == '__main__':
    main()
