"""Check that no event script can write the state-flag bytes holding the
randomizer config (vs_main_stateFlags + 0x430..0x43F).

Scripts (EVENT/*.EVT, MPD script sections and per-door scripts in MPD door
sections) share one layout: u16 offsets at +2 (strings), +4 and +6 (entry
tables), code from +16 up to the strings. Code is decoded opcode by opcode with
the game's own length table (BATTLE.PRG D_800E9C30). Opcodes that can write a
flag (handlers calling vs_battle_setStateFlag, found by scanning the handler
table) either build a 10-bit flag number or write fixed flags, except these,
which take an unmasked u16 operand: (opcode -> operand offset, flags written)."""
import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import psxdisc  # noqa: E402

BATTLE_LOAD = 0x80068800
LENGTHS = 0x800E9C30
WIDE_FLAG_OPS = {
    0x1F: (3, 3),  # actor position -> flag, flag+1, flag+2
    0x52: (3, 1),  # set flag if item in inventory
    0xB2: (6, 2),  # distance and angle between actors -> flag, flag+1
    0xB6: (5, 1),
}
CONFIG = range(0x430, 0x440)


def decode(blob, lengths):
    """Yields (offset, opcode) for the code region, and a list of problems."""
    problems = []
    if len(blob) < 16:
        return [], ['too short']
    strings, t1, t2 = struct.unpack_from('<HHH', blob, 2)
    end = min(strings, len(blob)) if strings >= 16 else len(blob)
    ops = []
    p = 16
    while p < end:
        op = blob[p]
        n = lengths[op]
        if n == 0:
            problems.append(f'zero-length opcode {op:#x} at {p:#x}')
            break
        ops.append((p, op))
        p += n
    starts = {p for p, _ in ops}
    for table, count in ((t1, 7), (t2, 8)):
        for i in range(count):
            if table + 2 * i + 2 > len(blob):
                break
            entry = struct.unpack_from('<H', blob, table + 2 * i)[0]
            if entry and table + entry < end and (table + entry) not in starts:
                problems.append(f'entry {table + entry:#x} not on an instruction boundary')
    return ops, problems


def scripts(disc):
    for e in sorted(disc.files(), key=lambda e: e.path):
        if e.path.startswith('EVENT/') and e.path.endswith('.EVT'):
            yield e.path, disc.read_lba(e.lba, e.size)
        elif e.path.endswith('.MPD'):
            m = disc.read_lba(e.lba, e.size)
            off, ln = struct.unpack_from('<II', m, 0x10)
            if ln:
                yield e.path + ' script', m[off:off + ln]
            off, ln = struct.unpack_from('<II', m, 0x18)
            if ln >= 2:
                doors = m[off:off + ln]
                if len(doors) < 2:  # section past the end of the file (seen in Zenith's MPDs)
                    yield f'{e.path} doors', b''
                    continue
                first = struct.unpack_from('<H', doors, 0)[0]
                for i in range(min(first, ln) // 2):
                    start = struct.unpack_from('<H', doors, 2 * i)[0]
                    if 0 < start < ln:  # 0 = no script for this door
                        yield f'{e.path} door {i}', doors[start:]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--vanilla', required=True)
    args = ap.parse_args()
    disc = psxdisc.Disc(args.vanilla)
    battle = disc.read_file('BATTLE/BATTLE.PRG')
    lengths = battle[LENGTHS - BATTLE_LOAD:LENGTHS - BATTLE_LOAD + 256]

    count = written = 0
    highest = 0
    hits, bad = [], []
    for name, blob in scripts(disc):
        ops, problems = decode(blob, lengths)
        count += 1
        if problems:
            bad.append((name, problems[0]))
        for p, op in ops:
            if op in WIDE_FLAG_OPS and p + WIDE_FLAG_OPS[op][0] + 2 <= len(blob):
                operand, span = WIDE_FLAG_OPS[op]
                flag = struct.unpack_from('<H', blob, p + operand)[0]
                written += 1
                highest = max(highest, flag + span - 1)
                if any(f in CONFIG for f in range(flag, flag + span)):
                    hits.append((name, p, op, flag))
    print(f'{count} scripts decoded, {len(bad)} with decode problems, {written} wide flag writes, '
          f'highest flag written {highest:#x}')
    for name, problem in bad[:10]:
        print(f'  decode problem: {name}: {problem}')
    for hit in hits:
        print(f'  WRITES CONFIG BYTES: {hit}')
    sys.exit(1 if hits or bad else 0)


if __name__ == '__main__':
    main()
