"""Launch DuckStation on an image with its GDB server enabled, using an isolated
config directory (so the user's own DuckStation settings are untouched).

The config dir must contain duckstation/settings.ini with [Debug]
EnableGDBServer = true and a BIOS search directory holding an NTSC-U BIOS."""
import os
import signal
import subprocess
import time
from contextlib import contextmanager
from pathlib import Path

from gdbrsp import Gdb

DUCKSTATION = '/opt/duckstation-qt/AppRun'

# Button bits as returned by vs_main_updatePadState (1 = held).
PAD = {
    'L2': 0x1, 'R2': 0x2, 'L1': 0x4, 'R1': 0x8,
    'triangle': 0x10, 'circle': 0x20, 'cross': 0x40, 'square': 0x80,
    'select': 0x100, 'start': 0x800,
    'up': 0x1000, 'right': 0x2000, 'down': 0x4000, 'left': 0x8000,
}


def screenshot(path):
    """Capture the DuckStation window (Hyprland only). Returns False if unavailable."""
    import json
    try:
        clients = json.loads(subprocess.check_output(['hyprctl', 'clients', '-j']))
    except (OSError, subprocess.CalledProcessError):
        return False
    for c in clients:
        if 'duckstation' in c.get('class', '').lower() and c['size'][1] > 200:
            geom = f"{c['at'][0]},{c['at'][1]} {c['size'][0]}x{c['size'][1]}"
            return subprocess.call(['grim', '-g', geom, str(path)]) == 0
    return False


@contextmanager
def emulator(cue, config_home, log=None, port=2345):
    env = dict(os.environ, XDG_CONFIG_HOME=str(config_home))
    out = open(log, 'w') if log else subprocess.DEVNULL
    proc = subprocess.Popen([DUCKSTATION, '-batch', '-fastboot', str(Path(cue).resolve())],
                            cwd='/opt/duckstation-qt', env=env, stdout=out, stderr=subprocess.STDOUT,
                            start_new_session=True)
    gdb = None
    try:
        gdb = Gdb(port=port, connect_wait=60)
        yield gdb
    finally:
        if gdb:
            gdb.close()
        try:
            os.killpg(proc.pid, signal.SIGTERM)
            proc.wait(timeout=10)
        except (ProcessLookupError, subprocess.TimeoutExpired):
            os.killpg(proc.pid, signal.SIGKILL)
        time.sleep(0.5)
