"""Play to a room with a chest (by walking out of the first Wine Cellar room)
and show its treasure before and after the payload randomizes it."""
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
WALK = ['up', 'left', 'down', 'right', 'up+left', 'down+right', 'up+right', 'down+left']


def describe(t, name):
    out = []
    if t[0]:
        out.append(f'weapon "{vstext.decode(t[0x94:0xAC])}" = {name(t[4])} + {name(struct.unpack_from("<H", t, 0x30)[0])}')
    if t[0xAC]:
        out.append(f'blade {name(t[0xB0])}')
    if t[0xDC]:
        out.append(f'grip {name(struct.unpack_from("<H", t, 0xE0)[0])}')
    if t[0xF0]:
        out.append(f'shield {name(t[0xF4])}')
    for flag in (0x170, 0x19C, 0x1C8):
        if t[flag]:
            out.append(f'armor {name(t[flag + 4])}')
    if t[0x1F4]:
        out.append(f'gem {name(struct.unpack_from("<H", t, 0x1F8)[0])}')
    for i in range(3):
        iid, flags, count = struct.unpack_from('<HBB', t, 0x214 + i * 4)
        if flags:
            out.append(f'{name(iid)} x{count}')
    return ', '.join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cue', default='build/VSIntegratedRando.cue')
    ap.add_argument('--symbols', default='build/payload.sym')
    ap.add_argument('--vanilla', default='reference/Vagrant Story (USA).bin')
    ap.add_argument('--config-home', required=True)
    args = ap.parse_args()
    syms = read_symbols(args.symbols)
    names = psxdisc.Disc(args.vanilla).read_file('MENU/ITEMNAME.BIN')

    def name(i):
        return vstext.decode(names[i * 24:i * 24 + 24]).replace('|>6', ' ')

    hook, buttons = syms['rando_onMpdLoaded'], syms['rando_debugButtons']
    with emulator(args.cue, args.config_home) as g:
        g.cmd('?')
        g.break_at(hook)
        start = time.time()
        step = 0
        walking = None
        while time.time() - start < 360:
            if walking is None:
                press = ['start', None, 'circle', None][step % 4]
                mask = PAD[press] if press else 0
            else:
                mask = 0
                for b in WALK[int((time.time() - walking) // 5) % len(WALK)].split('+'):
                    mask |= PAD[b]
            step += 1
            g.write(buttons, struct.pack('<I', mask))
            if g.cont(timeout=0.4) is None:
                continue
            zone, room = g.read(ROOM_REQUEST, 2)
            mpd = g.read32(ZONE_CONTEXT + 0x5C)
            hdr = struct.unpack('<12I', g.read(mpd, 48))
            print(f'[{time.time() - start:5.1f}s] zone {zone} room {room}', flush=True)
            if zone == 9 and walking is None:
                walking = time.time()
            if hdr[11] == 0x220:
                before = g.read(mpd + hdr[10], 0x220)
                regs = g.regs()
                g.unbreak(hook)
                g.break_at(regs['ra'])
                g.cont(timeout=5)
                g.unbreak(regs['ra'])
                after = g.read(mpd + hdr[10], 0x220)
                print(f'    chest before: {describe(before, name)}')
                print(f'    chest after:  {describe(after, name)}')
                return
        sys.exit('never reached a room with a chest')


if __name__ == '__main__':
    main()
