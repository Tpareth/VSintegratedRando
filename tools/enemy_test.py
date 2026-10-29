"""Check the ZND enemy hook in the running game: at each zone load, confirm the
zone id the payload uses matches the data being loaded, and that the game's
parsed enemy templates carry randomized stats."""
import argparse
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import psxdisc  # noqa: E402
import vstext  # noqa: E402
from build import read_symbols  # noqa: E402
from emu import PAD, emulator  # noqa: E402

ZONE_CONTEXT = 0x800F1880
ROOM_REQUEST = 0x800F1AB0
TEMPLATE = 0x464


def vanilla_zones(disc):
    zones = {}
    for e in disc.files():
        if e.path.startswith('MAP/ZONE') and e.path.endswith('.ZND'):
            znd = disc.read_lba(e.lba, e.size)
            off, ln = struct.unpack_from('<II', znd, 8)
            if ln:
                zones[int(e.path[8:11])] = znd[off:off + ln]
    return zones


def stats(t):
    hp, mp = struct.unpack_from('<hh', t, 0x1C)
    return hp, mp, t[0x20], t[0x21], t[0x22]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cue', default='build/VSIntegratedRando.cue')
    ap.add_argument('--symbols', default='build/payload.sym')
    ap.add_argument('--vanilla', default='reference/Vagrant Story (USA).bin')
    ap.add_argument('--config-home', required=True)
    ap.add_argument('--zones', type=int, default=3)
    args = ap.parse_args()
    syms = read_symbols(args.symbols)
    zones = vanilla_zones(psxdisc.Disc(args.vanilla))
    hook = syms['rando_onZndEnemies']
    buttons = syms['rando_debugButtons']

    with emulator(args.cue, args.config_home) as g:
        g.cmd('?')
        g.break_at(hook)
        start = time.time()
        seen = 0
        step = 0
        while seen < args.zones and time.time() - start < 240:
            press = ['start', None, 'circle', None][step % 4]
            step += 1
            g.write(buttons, struct.pack('<I', PAD[press] if press else 0))
            if g.cont(timeout=0.4) is None:
                continue
            regs = g.regs()
            zone = g.read(ROOM_REQUEST, 1)[0]
            count = g.read32(regs['a0'])
            vanilla = zones.get(zone)
            match = vanilla is not None and struct.unpack_from('<I', vanilla, 0)[0] == count
            print(f'[{time.time() - start:5.1f}s] ZND load: zone {zone}, {count} enemies, '
                  f'matches disc: {match}')
            # Run to the end of _loadZndEnemies, then read the game's parsed copy.
            g.unbreak(hook)
            g.break_at(regs['ra'])
            g.cont(timeout=5)
            g.unbreak(regs['ra'])
            g.break_at(hook)
            parsed = g.read32(ZONE_CONTEXT + 0x0C)
            for i in range(min(count, 4)):
                t = g.read(parsed + i * TEMPLATE, TEMPLATE)
                v = vanilla[4 + count * 8 + i * TEMPLATE:][:TEMPLATE] if match else None
                name = vstext.decode(t[4:28]).replace('|>6', ' ')
                print(f'    {name:<20} vanilla {stats(v) if v else "?"} -> now {stats(t)}')
            seen += 1


if __name__ == '__main__':
    main()
