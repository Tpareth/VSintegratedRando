"""Generate the payload's equipment pools (build/gen/equipment.c) from a vanilla
disc: every distinct blade, grip, shield, armor piece and gem found in chest
treasure sections and enemy templates, converted to the inventory record format.

The game has no global equipment table: chests and enemies carry complete
records, so a consistent random item must be copied from one of those."""
import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import psxdisc  # noqa: E402

BLADE_IDS = range(1, 91)
GRIP_IDS = range(96, 127)
SHIELD_IDS = range(127, 143)
GEM_IDS = range(0x105, 0x134)
# Armor slots by item id range (ITEMNAME.BIN order).
ARMOR_SLOTS = [('head', range(143, 159)), ('body', range(159, 175)), ('legs', range(175, 191)),
               ('arms', range(191, 207)), ('accessory', range(223, 254))]
TEMPLATE = 0x464


def ui_equipment(rec):
    """Unpack a vs_battle_uiEquipment (0x30 bytes, enemy template format)."""
    f = {}
    (f['id'], f['subId'], f['wepId'], f['category'], f['str'], f['int'], f['agl'],
     f['curDp'], f['maxDp'], f['curPp'], f['maxPp'], f['damageType'], f['costType'], f['cost'],
     f['material'], _, f['gemSlots'], f['gemEffects'], _) = struct.unpack_from('<HbbBbbbHHHHBBBBBBBB', rec, 0)
    f['range'] = rec[0x18:0x1C]
    f['types'] = rec[0x1C:0x20]
    f['classes'] = rec[0x20:0x28]
    f['affinities'] = rec[0x28:0x30]
    return f


def blade_from_ui(f):
    rec = bytearray(0x2C)
    struct.pack_into('<BbbBHHHHbbbB', rec, 0, f['id'], f['subId'], f['wepId'], f['category'],
                     f['maxDp'], f['maxPp'], f['curDp'], f['curPp'], f['str'], f['int'], f['agl'],
                     f['cost'])
    rec[0x10] = (f['damageType'] & 3) | ((f['costType'] & 7) << 2)
    rec[0x14:0x18] = f['range']
    rec[0x18:0x20] = f['classes']
    rec[0x20:0x28] = f['affinities']
    struct.pack_into('<H', rec, 0x28, f['material'])
    return bytes(rec)


def grip_from_ui(f):
    rec = bytearray(0x10)
    struct.pack_into('<HbBbbbb', rec, 0, f['id'], f['subId'], f['category'], f['gemSlots'],
                     f['str'], f['int'], f['agl'])
    rec[8:12] = f['types']
    return bytes(rec)


def armor_from_ui(f):
    rec = bytearray(0x28)
    struct.pack_into('<BBbBHHHHbbbb', rec, 0, f['id'], f['subId'] & 0xFF, f['wepId'], f['category'],
                     f['maxDp'], f['maxPp'], f['curDp'], f['curPp'], f['gemSlots'], f['str'], f['int'],
                     f['agl'])
    rec[0x10:0x14] = f['types']
    rec[0x14:0x1C] = f['classes']
    rec[0x1C:0x24] = f['affinities']
    struct.pack_into('<H', rec, 0x24, f['material'])
    return bytes(rec)


def gem_from_ui(f):
    rec = bytearray(0x1C)
    struct.pack_into('<HbbBbbb', rec, 0, f['id'], f['subId'], 0, f['gemEffects'], f['str'], f['int'], f['agl'])
    rec[0x08:0x10] = f['classes']
    rec[0x10:0x18] = f['affinities']
    return bytes(rec)


def clear_slot_fields(kind, rec):
    """Zero the inventory bookkeeping fields; the payload sets them per slot."""
    rec = bytearray(rec)
    if kind == 'blade':
        rec[0x2A:0x2C] = b'\0\0'
    elif kind == 'grip':
        rec[0x0C:0x10] = bytes(4)
    elif kind == 'gem':
        rec[0x18:0x1C] = bytes(4)
    else:
        rec[0x26:0x28] = b'\0\0'
    return bytes(rec)


def valid_blade(r):
    return r[0] in BLADE_IDS and 1 <= r[3] <= 9 and struct.unpack_from('<H', r, 4)[0] > 0


def valid_grip(r):
    return struct.unpack_from('<H', r, 0)[0] in GRIP_IDS and 1 <= r[3] <= 4


def valid_shield(r):
    return r[0] in SHIELD_IDS and struct.unpack_from('<H', r, 4)[0] > 0


def valid_gem(r):
    return struct.unpack_from('<H', r, 0)[0] in GEM_IDS


def armor_slot(r):
    for slot, ids in ARMOR_SLOTS:
        if r[0] in ids:
            return slot
    return None


# Pool name -> (record size, key function, C count variable), in emit order.
POOLS = {
    'blades': (0x2C, lambda r: r[0], 'pool_bladeCount'),
    'grips': (0x10, lambda r: struct.unpack_from('<H', r)[0], 'pool_gripCount'),
    'shields': (0x28, lambda r: r[0], 'pool_shieldCount'),
    'armor': (0x28, lambda r: r[0], 'pool_armorCount'),
    'gems': (0x1C, lambda r: struct.unpack_from('<H', r)[0], 'pool_gemCount'),
}
VALID = {'blades': valid_blade, 'grips': valid_grip, 'shields': valid_shield,
         'armor': lambda r: armor_slot(r) is not None, 'gems': valid_gem}
SLOT_KIND = {'blades': 'blade', 'grips': 'grip', 'shields': 'armor', 'armor': 'armor', 'gems': 'gem'}


def collect(disc):
    """Pool name -> {item id: inventory-format record}."""
    pools = {name: {} for name in POOLS}

    def add(name, rec):
        if VALID[name](rec):
            pools[name].setdefault(POOLS[name][1](rec), clear_slot_fields(SLOT_KIND[name], rec))

    # Chests first: their records are exactly what the player would receive.
    # vs_battle_loot entries: (presence flag offset, [(pool, record offset)]).
    chest_entries = [
        (0x000, [('blades', 0x004), ('grips', 0x030), ('gems', 0x040), ('gems', 0x05C), ('gems', 0x078)]),
        (0x0AC, [('blades', 0x0B0)]),
        (0x0DC, [('grips', 0x0E0)]),
        (0x0F0, [('shields', 0x0F4), ('gems', 0x11C), ('gems', 0x138), ('gems', 0x154)]),
        (0x170, [('armor', 0x174)]),
        (0x19C, [('armor', 0x1A0)]),
        (0x1C8, [('armor', 0x1CC)]),
        (0x1F4, [('gems', 0x1F8)]),
    ]
    for e in sorted(disc.files(), key=lambda e: e.path):
        if not e.path.endswith('.MPD'):
            continue
        m = disc.read_lba(e.lba, e.size)
        off, ln = struct.unpack_from('<II', m, 0x28)
        if ln != 0x220:
            continue
        t = m[off:off + ln]
        for flag, records in chest_entries:
            if t[flag]:
                for name, roff in records:
                    add(name, t[roff:roff + POOLS[name][0]])

    # Then enemy equipment, converted from the template format. Materials are
    # stored beside the uiEquipment records, not in them.
    for e in sorted(disc.files(), key=lambda e: e.path):
        if not (e.path.startswith('MAP/ZONE') and e.path.endswith('.ZND')):
            continue
        z = disc.read_lba(e.lba, e.size)
        off, ln = struct.unpack_from('<II', z, 8)
        if not ln:
            continue
        n = struct.unpack_from('<I', z, off)[0]
        for i in range(n):
            t = z[off + 4 + n * 8 + i * TEMPLATE:][:TEMPLATE]
            blade = ui_equipment(t[0x34:0x64])
            blade['material'] = t[0x34 + 0xF0]
            add('blades', blade_from_ui(blade))
            add('grips', grip_from_ui(ui_equipment(t[0x64:0x94])))
            for g in range(3):
                add('gems', gem_from_ui(ui_equipment(t[0x94 + g * 0x30:][:0x30])))
            shield = ui_equipment(t[0x140:0x170])
            shield['material'] = t[0x140 + 0xC0]
            add('shields', armor_from_ui(shield))
            for g in range(3):
                add('gems', gem_from_ui(ui_equipment(t[0x170 + g * 0x30:][:0x30])))
            add('armor', armor_from_ui(ui_equipment(t[0x204:0x234])))
            for limb in range(6):
                limb_armor = t[0x238 + limb * 0x5C + 0x20:][:0x34]
                piece = ui_equipment(limb_armor)
                piece['material'] = limb_armor[0x30]
                add('armor', armor_from_ui(piece))
    return pools


def c_bytes(rec):
    return ', '.join(f'0x{b:02X}' for b in rec)


def emit(pools, out):
    lines = ['/* Generated by tools/gen_equipment.py from the vanilla disc. Do not edit. */',
             '#include "equipment.h"', '']
    for name, (size, _, count) in POOLS.items():
        pool = pools[name]
        lines.append(f'const u8 pool_{name}[][0x{size:X}] = {{')
        lines += [f'    {{ {c_bytes(pool[k])} }},' for k in sorted(pool)]
        lines += ['};', f'const int {count} = {len(pool)};', '']
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    Path(out).write_text('\n'.join(lines))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--vanilla', required=True)
    ap.add_argument('--out', required=True)
    args = ap.parse_args()
    pools = collect(psxdisc.Disc(args.vanilla))
    emit(pools, args.out)
    slots = {s: sum(1 for r in pools['armor'].values() if armor_slot(r) == s) for s, _ in ARMOR_SLOTS}
    print(f"equipment pool: {len(pools['blades'])} blades, {len(pools['grips'])} grips, "
          f"{len(pools['shields'])} shields, {len(pools['gems'])} gems, armor {slots}")


if __name__ == '__main__':
    main()
