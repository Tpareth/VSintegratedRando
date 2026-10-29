"""Drive the boot options menu in DuckStation and capture screenshots.

Steps are given as BUTTON[:seconds] (hold for that long, then release for the
same time); 'wait:S' just runs. A screenshot is taken after every step."""
import argparse
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from build import read_symbols  # noqa: E402
from emu import PAD, emulator, screenshot  # noqa: E402

CONFIG_ADDR = 0x80061598 + 0x440 - 12


def press(g, buttons_addr, mask, secs):
    g.write(buttons_addr, struct.pack('<I', mask))
    g.cont(timeout=secs)
    g.write(buttons_addr, struct.pack('<I', 0))
    g.cont(timeout=secs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cue', default='build/VSIntegratedRando.cue')
    ap.add_argument('--symbols', default='build/payload.sym')
    ap.add_argument('--menu-symbols', default='build/menu.sym')
    ap.add_argument('--config-home', required=True)
    ap.add_argument('--shots', required=True)
    ap.add_argument('steps', nargs='*')
    args = ap.parse_args()
    syms = read_symbols(args.symbols)
    buttons = syms['rando_debugButtons']
    menu = read_symbols(args.menu_symbols)['rando_menuMain']
    shots = Path(args.shots)
    shots.mkdir(parents=True, exist_ok=True)

    with emulator(args.cue, args.config_home) as g:
        g.cmd('?')
        g.break_at(menu)
        start = time.time()
        alternate = ['start', 'circle']
        n = 0
        while True:
            if time.time() - start > 180:
                sys.exit('never reached the boot menu')
            g.write(buttons, struct.pack('<I', PAD[alternate[n % 2]]))
            n += 1
            if g.cont(timeout=0.3) is not None:
                break
            g.write(buttons, struct.pack('<I', 0))
            if g.cont(timeout=0.3) is not None:
                break
        print(f'menu entered after {time.time() - start:.1f}s')
        g.unbreak(menu)
        g.write(buttons, struct.pack('<I', 0))
        g.cont(timeout=1.0)
        screenshot(shots / '00-menu.png')
        for i, step in enumerate(args.steps, 1):
            name, _, secs = step.partition(':')
            secs = float(secs or 0.15)
            if name == 'wait':
                g.cont(timeout=secs)
            else:
                mask = 0
                for b in name.split('+'):
                    mask |= PAD[b]
                press(g, buttons, mask, secs)
            g.cont(timeout=0.3)
            screenshot(shots / f'{i:02d}-{name}.png')
        seed, options = struct.unpack('<II', g.read(syms['rando_pending'], 8))
        print(f'pending: seed={seed} options={options:#x}')
        magic, check, seed, options = struct.unpack('<HHII', g.read(CONFIG_ADDR, 12))
        print(f'config: magic={magic:#x} seed={seed} options={options:#x} check={check:#x}')


if __name__ == '__main__':
    main()
