"""Boot the patched image, mash through the title screen, and stop at the first
MPD load hook. Prints the room and its treasure section before/after randomization."""
import argparse
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import psxdisc  # noqa: E402
from build import read_symbols  # noqa: E402
from emu import PAD, emulator, screenshot  # noqa: E402

ZONE_CONTEXT = 0x800F1880
ROOM_REQUEST = 0x800F1AB0
CONFIG_ADDR = 0x80061598 + 0x440 - 12


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cue', default='build/VSIntegratedRando.cue')
    ap.add_argument('--symbols', default='build/payload.sym')
    ap.add_argument('--config-home', required=True)
    ap.add_argument('--shots', default=None, help='directory for progress screenshots')
    ap.add_argument('--timeout', type=float, default=240)
    ap.add_argument('--loads', type=int, default=1, help='number of MPD loads to report')
    ap.add_argument('--vanilla', default='reference/Vagrant Story (USA).bin')
    args = ap.parse_args()
    syms = read_symbols(args.symbols)
    disc = psxdisc.Disc(args.vanilla)
    mpd_by_lba = {e.lba: e for e in disc.files() if e.path.endswith('.MPD')}

    def mpd_for(zone, room):
        """The MPD at index `room` of the zone's list: what roomKey assumes."""
        znd = disc.read_file(f'MAP/ZONE{zone:03d}.ZND')
        off, ln = struct.unpack_from('<II', znd, 0)
        if room >= ln // 8:
            return None
        e = mpd_by_lba.get(struct.unpack_from('<I', znd, off + room * 8)[0])
        return e and disc.read_lba(e.lba, e.size)
    buttons = syms['rando_debugButtons']
    hook = syms['rando_onMpdLoaded']
    shots = Path(args.shots) if args.shots else None
    if shots:
        shots.mkdir(parents=True, exist_ok=True)

    with emulator(args.cue, args.config_home) as g:
        g.cmd('?')
        g.break_at(hook)
        start = time.time()
        seq = ['start', None, 'circle', None]
        step = 0
        loads = 0
        while time.time() - start < args.timeout:
            press = seq[step % len(seq)]
            g.write(buttons, struct.pack('<I', PAD[press] if press else 0))
            step += 1
            stop = g.cont(timeout=0.4)
            if shots and step % 25 == 0:
                screenshot(shots / f'{int(time.time() - start):04d}.png')
            if stop is None:
                continue
            regs = g.regs()
            if regs['pc'] != hook:
                print('unexpected stop', stop, f"pc={regs['pc']:#x}")
                break
            zone, room = g.read(ROOM_REQUEST, 2)
            mpd = struct.unpack('<I', g.read(ZONE_CONTEXT + 0x5C, 4))[0]
            hdr = struct.unpack('<12I', g.read(mpd, 48))
            print(f'[{time.time() - start:6.1f}s] MPD load: zone {zone} room {room} mpdData={mpd:#x} '
                  f'treasure len={hdr[11]}')
            expected = mpd_for(zone, room)
            print(f'    roomKey matches the disc: {expected is not None and expected[:48] == g.read(mpd, 48)}')
            magic, _, seed, options = struct.unpack('<HHII', g.read(CONFIG_ADDR, 12))
            print(f'    config magic={magic:#x} seed={seed} options={options:#x}')
            if hdr[11] == 0x220:
                before = g.read(mpd + hdr[10] + 0x214, 12)
                g.unbreak(hook)
                g.break_at(regs['ra'])
                g.cont(timeout=5)
                g.unbreak(regs['ra'])
                g.break_at(hook)
                after = g.read(mpd + hdr[10] + 0x214, 12)
                for i in range(3):
                    b = struct.unpack_from('<HBB', before, i * 4)
                    a = struct.unpack_from('<HBB', after, i * 4)
                    print(f'    misc[{i}] id {b[0]:#05x} x{b[2]} flags {b[1]} -> id {a[0]:#05x} x{a[2]}')
            loads += 1
            if loads >= args.loads:
                break
        else:
            print('timed out without reaching the hook')
        if shots:
            screenshot(shots / 'final.png')


if __name__ == '__main__':
    main()
