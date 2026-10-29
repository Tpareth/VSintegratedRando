"""Play into the Wine Cellar, open the main menu (Triangle) and screenshot it,
to check the seed display."""
import argparse
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from build import read_symbols  # noqa: E402
from emu import PAD, emulator, screenshot  # noqa: E402

ROOM_REQUEST = 0x800F1AB0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cue', default='build/VSIntegratedRando.cue')
    ap.add_argument('--symbols', default='build/payload.sym')
    ap.add_argument('--config-home', required=True)
    ap.add_argument('--shot', required=True)
    args = ap.parse_args()
    syms = read_symbols(args.symbols)
    buttons = syms['rando_debugButtons']
    mpd_hook = syms['rando_onMpdLoaded']
    menu_hook = syms['rando_onMainMenu']

    def hold(g, mask, secs):
        g.write(buttons, struct.pack('<I', mask))
        g.cont(timeout=secs)

    with emulator(args.cue, args.config_home) as g:
        g.cmd('?')
        g.break_at(mpd_hook)
        start = time.time()
        step = 0
        while time.time() - start < 200:
            press = ['start', None, 'circle', None][step % 4]
            step += 1
            g.write(buttons, struct.pack('<I', PAD[press] if press else 0))
            if g.cont(timeout=0.4) is not None and g.read(ROOM_REQUEST, 1)[0] == 9:
                break
        else:
            sys.exit('never reached the Wine Cellar')
        g.unbreak(mpd_hook)
        print(f'[{time.time() - start:5.1f}s] in the Wine Cellar')
        hold(g, 0, 8)
        g.break_at(menu_hook)
        for _ in range(10):
            hold(g, PAD['triangle'], 0.2)
            if g.cont(timeout=0.5) is not None:
                break
        else:
            sys.exit('main menu never opened')
        print(f'main menu hook: menu={g.regs()["a0"]:#x}')
        g.unbreak(menu_hook)
        hold(g, 0, 2)
        screenshot(args.shot)


if __name__ == '__main__':
    main()
