"""Start a randomized game in DuckStation and report Ashley as the game sees
him: his template at the INITBTL hook, and the starting inventory."""
import argparse
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import psxdisc  # noqa: E402
import vstext  # noqa: E402
from build import read_symbols  # noqa: E402
from emu import PAD, emulator, screenshot  # noqa: E402

INVENTORY = 0x80060168
TEMPLATE = 0x464


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cue', default='build/VSIntegratedRando.cue')
    ap.add_argument('--symbols', default='build/payload.sym')
    ap.add_argument('--vanilla', default='reference/Vagrant Story (USA).bin')
    ap.add_argument('--config-home', required=True)
    ap.add_argument('--shot', default=None)
    ap.add_argument('--menu-symbols', default='build/menu.sym')
    ap.add_argument('--menu-steps', default='',
                    help='buttons to press in the boot menu first, e.g. "up*8 right*8"')
    args = ap.parse_args()
    syms = read_symbols(args.symbols)
    names = psxdisc.Disc(args.vanilla).read_file('MENU/ITEMNAME.BIN')

    def name(i):
        return vstext.decode(names[i * 24:i * 24 + 24]).replace('|>6', ' ')

    hook = syms['rando_onAshleyInit']
    buttons = syms['rando_debugButtons']
    with emulator(args.cue, args.config_home) as g:
        g.cmd('?')
        if args.menu_steps:
            menu = read_symbols(args.menu_symbols)['rando_menuMain']
            g.break_at(menu)
            g.cont(timeout=60)
            g.unbreak(menu)
            g.cont(timeout=1)
            for step in args.menu_steps.split():
                button, _, times = step.partition('*')
                for _ in range(int(times or 1)):
                    g.write(buttons, struct.pack('<I', PAD[button]))
                    g.cont(timeout=0.12)
                    g.write(buttons, struct.pack('<I', 0))
                    g.cont(timeout=0.12)
        g.break_at(hook)
        start = time.time()
        step = 0
        while time.time() - start < 240:
            press = ['start', None, 'circle', None][step % 4]
            step += 1
            g.write(buttons, struct.pack('<I', PAD[press] if press else 0))
            if g.cont(timeout=0.4) is not None:
                break
        else:
            sys.exit('never reached the Ashley init hook')
        g.write(buttons, struct.pack('<I', 0))
        regs = g.regs()
        init = regs['a1']
        before = g.read(init, TEMPLATE)
        g.unbreak(hook)
        g.break_at(regs['ra'])
        g.cont(timeout=5)
        g.unbreak(regs['ra'])
        after = g.read(init, TEMPLATE)

        def stats(t):
            hp, mp = struct.unpack_from('<hh', t, 0x1C)
            return f'HP {hp} MP {mp} STR {t[0x20]} INT {t[0x21]} AGL {t[0x22]} walk {t[0x25]} run {t[0x27]}'

        print(f'[{time.time() - start:5.1f}s] {vstext.decode(after[4:28]).replace("|>6", " ")} at {init:#x}')
        print(f'    template before: {stats(before)}')
        print(f'    template after:  {stats(after)}')

        inv = g.read(INVENTORY, 0xF00)
        w = inv[0:0x20]
        blade = inv[0x280 + (w[1] - 1) * 0x2C:][:0x2C]
        grip = inv[0x540 + (w[2] - 1) * 0x10:][:0x10]
        print(f'    weapon "{vstext.decode(w[8:32])}": blade {name(blade[0])} (category {blade[3]}, '
              f'material {struct.unpack_from("<H", blade, 0x28)[0]}, DP {struct.unpack_from("<H", blade, 4)[0]}), '
              f'grip {name(struct.unpack_from("<H", grip)[0])} (category {grip[3]})')
        armor = [inv[0x640 + i * 0x28:][:0x28] for i in range(16)]
        print('    armor: ' + ', '.join(f'{name(a[0])} (limb {a[0x26]})' for a in armor if a[0]))
        misc = [struct.unpack_from('<HBB', inv, 0xE00 + i * 4) for i in range(64)]
        print('    items: ' + ', '.join(f'{name(m[0])} x{m[1]}' for m in misc if m[0]))

        g.cont(timeout=25)
        if args.shot:
            screenshot(args.shot)


if __name__ == '__main__':
    main()
