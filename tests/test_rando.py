"""Run the payload randomizers (compiled for the host) over the real game data
on the disc and check their invariants."""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import gen_chesttiers  # noqa: E402
import gen_equipment  # noqa: E402
import gen_keyrooms  # noqa: E402
import psxdisc  # noqa: E402

KIND_CHESTS, KIND_ZND_ENEMIES, KIND_MPD_DROPS, KIND_ASHLEY_STATS, KIND_ASHLEY_GEAR, KIND_ABILITIES = 1, 2, 3, 4, 5, 6
KIND_DROP_ROLLS = 7
KIND_ROOM = 8

OPT_CHESTS = 1 << 0
OPT_BALANCE_QUANTITIES = 1 << 1
OPT_ENEMY_STATS = 1 << 2
OPT_BALANCE_ENEMIES = 1 << 3
OPT_ENEMY_DROPS = 1 << 4
OPT_ASHLEY_STATS = 1 << 5
OPT_ASHLEY_WEAPON = 1 << 6
OPT_ASHLEY_ARMOR = 1 << 7
OPT_ASHLEY_ITEMS = 1 << 8
OPT_BALANCE_ASHLEY = 1 << 9
OPT_LOOT_KEYS = 1 << 10
OPT_CHEST_GEAR = 1 << 11
OPT_BREAK_ARTS = 1 << 16
OPT_BALANCE_BREAK_ARTS = 1 << 17
OPT_CHAIN_LEARNING = 1 << 18
OPT_RISK_COSTS = 1 << 19
OPT_ULTIMATE_SPELLS = 1 << 20
OPT_FRAME_WINDOWS = 1 << 21
OPT_ZENITH_QUIET = 1 << 25
# options2 bits, passed to the harness in the high 32 bits of the options.
OPT2_ENEMY_GEAR_BASE, OPT2_ENEMY_GEAR_CLASS, OPT2_ENEMY_GEAR_AFFINITY, OPT2_ENEMY_GEAR_BEP, OPT2_ENEMY_APPENDAGES = (
    1 << 32, 1 << 33, 1 << 34, 1 << 35, 1 << 36)
OPT2_ENEMY_GEAR = (OPT2_ENEMY_GEAR_BASE | OPT2_ENEMY_GEAR_CLASS | OPT2_ENEMY_GEAR_AFFINITY | OPT2_ENEMY_GEAR_BEP
                   | OPT2_ENEMY_APPENDAGES)
OPT2_CHEST_GEAR_BASE, OPT2_CHEST_GEAR_CLASS, OPT2_CHEST_GEAR_AFFINITY, OPT2_CHEST_GEAR_BEP = (
    1 << 37, 1 << 38, 1 << 39, 1 << 40)
OPT2_BALANCE_ITEM_STATS = 1 << 41
OPT2_ASHLEY_CLASS, OPT2_ASHLEY_AFFINITY, OPT2_ASHLEY_BEP = 1 << 42, 1 << 43, 1 << 44
OPT2_THRESHOLD_SHIFT = 45  # 2-bit field: 0 medium, 1 low, 2 high
OPT2_BLADE_COST, OPT2_FANDANGO_COST, OPT2_ICE_WOLF = 1 << 47, 1 << 48, 1 << 49
OPT2_OPEN_WORLD, OPT2_IRON_MAIDEN = 1 << 50, 1 << 51
KEYS_AND_SIGILS = range(0x1CA, 0x1EA)
THRESHOLDS = {0: (50, 150), 1: (75, 125), 2: (25, 175)}
OPT2_CHEST_STATS = OPT2_CHEST_GEAR_BASE | OPT2_CHEST_GEAR_CLASS | OPT2_CHEST_GEAR_AFFINITY | OPT2_CHEST_GEAR_BEP
OPT2_ASHLEY_GEAR = OPT2_ASHLEY_CLASS | OPT2_ASHLEY_AFFINITY | OPT2_ASHLEY_BEP

# Stat bytes of inventory-format records that the chest/Ashley stat options
# may change: kind -> [(field, offsets)] with field one of base/class/aff/bep.
STAT_FIELDS = {
    'blade': [('base', range(0x04, 0x0C)), ('cost', [0x10]), ('class', range(0x18, 0x1E)), ('aff', range(0x20, 0x27))],
    'grip': [('bep', range(0x09, 0x0C))],
    'gem': [('base', range(0x05, 0x08)), ('class', range(0x08, 0x0E)), ('aff', range(0x10, 0x17))],
    'armor': [('base', range(0x04, 0x0C)), ('bep', range(0x11, 0x14)), ('class', range(0x14, 0x1A)),
              ('aff', range(0x1C, 0x23))],
}
CHEST_FIELD_BITS = {'base': OPT2_CHEST_GEAR_BASE, 'class': OPT2_CHEST_GEAR_CLASS, 'aff': OPT2_CHEST_GEAR_AFFINITY,
                    'bep': OPT2_CHEST_GEAR_BEP, 'cost': OPT2_BLADE_COST}
ASHLEY_FIELD_BITS = {'class': OPT2_ASHLEY_CLASS, 'aff': OPT2_ASHLEY_AFFINITY, 'bep': OPT2_ASHLEY_BEP,
                     'cost': OPT2_FANDANGO_COST}
# Chest tiers (src/battle/cheststats.c): class/affinity split, BEP and gem stat maxima.
CHEST_TIER_RANGES = [(20, 5, 7, 5), (20, 6, 7, 6), (20, 11, 20, 6), (30, 15, 25, 15)]


def stat_fields(kind, rec, options, bits):
    """(field, offsets) the options may change in this record."""
    accessory = kind == 'armor' and rec[3] == 7
    out = []
    for field, offs in STAT_FIELDS['armor' if kind == 'shield' else kind]:
        if not options & bits.get(field, 0):
            continue
        if accessory and (field == 'base' or (field == 'bep' and bits is CHEST_FIELD_BITS)):
            continue  # accessories have no DP; chest accessories get no BEP
        if kind == 'armor' and field == 'base' and bits is ASHLEY_FIELD_BITS:
            continue
        out.append((field, offs))
    return out


def mask(kind, rec, options, bits):
    rec = bytearray(rec)
    for field, offs in stat_fields(kind, rec, options, bits):
        for o in offs:
            rec[o] = rec[o] & 3 if field == 'cost' else 0  # cost: keep the damage type bits
    return bytes(rec)


def check_cost(rec):
    assert 1 <= rec[0x10] >> 2 <= 4 and rec[0x10] < 0x20, f'cost type {rec[0x10]:#x}'


def in_pool(data, kind, rec, options, bits=CHEST_FIELD_BITS):
    pool = data.pool[kind]
    key = (kind, options & sum(bits.values()), id(bits))
    if key not in data.masked:
        data.masked[key] = {mask(kind, p, options, bits) for p in pool}
    return mask(kind, rec, options, bits) in data.masked[key]


def check_rolled(kind, rec, options, tier, bits=CHEST_FIELD_BITS):
    """Balanced chest ranges by tier (tier None: unbalanced, anything goes)."""
    signed = lambda b: b - 256 if b > 127 else b
    for field, offs in stat_fields(kind, rec, options, bits):
        if field == 'cost':
            check_cost(rec)
            continue
        if field == 'base' and kind in ('blade', 'armor', 'shield'):
            mx, mpp, cur, cpp = struct.unpack_from('<4H', rec, 4)
            assert mx % 100 == 0 and cur % 100 == 0 and 4100 <= cur <= mx <= 32700, (kind, mx, cur)
            if kind != 'armor':
                assert 1 <= cpp <= mpp and 100 <= mpp <= 250, (kind, mpp, cpp)
            continue
        if tier is None:
            continue
        pos, neg, bep, gem = CHEST_TIER_RANGES[tier]
        lo, hi = {'class': (-neg, pos), 'aff': (-neg, pos), 'bep': (0, bep), 'base': (-3, gem)}[field]
        for o in offs:
            assert lo <= signed(rec[o]) <= hi, (kind, field, hex(o), signed(rec[o]), tier)
OPT_ZENITH_BOSS_GRIMOIRES = 1 << 24
# Zenith "quiet enemies": (zone, enemy) -> {offset in limb 2: spell removed}, from Zenith.cpp.
QUIET = {(32, 3): {0x14: 60}, (32, 4): {0x14: 60}, (32, 12): {0x14: 63, 0x18: 60}, (32, 13): {0x14: 63, 0x18: 60},
         (32, 21): {0x14: 63, 0x18: 60}, (32, 22): {0x14: 63, 0x18: 60}, (53, 10): {0x10: 60}, (53, 11): {0x10: 63},
         (30, 4): {0x10: 60, 0x14: 63}}
# Zenith boss Warlock Grimoires: roomKey -> spawn 0 (items, counts).
BOSS_GRIMOIRES = {0x0D04: ((0x182, 0x144, 0), (1, 3, 0)), 0x3302: ((0x18F, 0x183, 0x149), (1, 1, 0)),
                  0x3211: ((0x193, 0x184, 0x15D), (1, 1, 255)), 0x330E: ((0x197, 0x185, 0), (1, 1, 0)),
                  0x3501: ((0x19B, 0x186, 0x15B), (1, 1, 255))}
ALL = (0xFFF | 0x3F0000 | OPT2_ENEMY_GEAR | OPT2_CHEST_STATS | OPT2_BALANCE_ITEM_STATS | OPT2_ASHLEY_GEAR | OPT2_BLADE_COST
       | OPT2_FANDANGO_COST | OPT2_ICE_WOLF)
BALANCE = (OPT_BALANCE_QUANTITIES | OPT_BALANCE_ENEMIES | OPT_BALANCE_ASHLEY | OPT_BALANCE_BREAK_ARTS
           | OPT2_BALANCE_ITEM_STATS)
GRIP_FOR_BLADE = [0, 1, 1, 1, 2, 2, 2, 2, 3, 4]

CONSUMABLES = range(0x143, 0x165)
GRIMOIRES = range(0x182, 0x1C9)
PROGRESSION = range(0x1CA, 0x1EA)
BRONZE, IRON, SILVER, CHAMOMILE = 0x1CA, 0x1CB, 0x1CC, 0x1D2
# Chests.cpp area-order ranks (see src/payload/keys.c).
KEY_RANK = {8: 0, 10: 1, 18: 2, 14: 3, 9: 4, 11: 5, 13: 5, 16: 5}
MISC_OFF = 0x214
TEMPLATE = 0x464
SPAWN = 0x28
# Zones randomized (zone id -> balance group), as in src/payload/enemies.c.
ZONE_GROUPS = {9: 0, 11: 0, 12: 0, 13: 0, 14: 0, 15: 0, 16: 0, 17: 0,
               28: 1, 32: 1, 36: 1, 40: 1, 48: 1, 50: 1, 49: 2, 51: 2, 53: 2,
               22: 3, 24: 3, 25: 3, 30: 3, 31: 3, 41: 4, 52: 4, 54: 4, 55: 4, 56: 4}
BALANCED_CAPS = [(480, 130, 112, 127, 132), (510, 500, 150, 145, 145), (720, 500, 150, 145, 145),
                 (790, 666, 150, 170, 155), (999, 999, 150, 220, 200)]
STAT_BYTES = set(range(0x1C, 0x23)) | {0x25, 0x27}  # hp, mp, str, int, agl, walk, run


class GameData:
    def __init__(self, disc):
        initbtl = disc.read_file('BATTLE/INITBTL.PRG')
        self.ashley = initbtl[0xAB18 - 0x9800:][:TEMPLATE]  # D_800FAB18
        self.inventory = disc.read_file('TITLE/TITLE.PRG')[0xC424:0xC424 + 0xF00]  # D_80074C24
        exe = disc.read_file('SLUS_010.40')
        ram = lambda addr, size: exe[addr - 0x80010000 + 0x800:][:size]
        battle = disc.read_file('BATTLE/BATTLE.PRG')
        # vs_main_actions[224], arts points[40], ability points[22], the two frame-window words.
        self.abilities = (ram(0x8004B9DC, 224 * 0x34) + ram(0x8004FDE4, 80) + ram(0x8004FE5C, 44)
                          + battle[0x80078B30 - 0x80068800:][:4] + battle[0x80078B48 - 0x80068800:][:4])
        self.pools = gen_equipment.collect(disc)
        self.pool = {'blade': set(self.pools['blades'].values()), 'grip': set(self.pools['grips'].values()),
                     'armor': set(self.pools['armor'].values()), 'shield': set(self.pools['shields'].values()),
                     'gem': set(self.pools['gems'].values())}
        self.key_rooms = gen_keyrooms.collect(disc)
        self.chest_tiers = dict(gen_chesttiers.collect(disc))
        self.masked = {}
        self.chests = []  # (roomKey, treasure bytes)
        self.zones = []  # (zndId, enemy section bytes)
        self.spawns = []  # (zndId, roomKey, enemy section bytes)
        mpds = {e.lba: e for e in disc.files() if e.path.endswith('.MPD')}
        for e in sorted(disc.files(), key=lambda e: e.path):
            if not (e.path.startswith('MAP/ZONE') and e.path.endswith('.ZND')):
                continue
            znd_id = int(e.path[8:11])
            znd = disc.read_lba(e.lba, e.size)
            mpd_off, mpd_len, enemy_off, enemy_len = struct.unpack_from('<4I', znd, 0)
            if enemy_len:
                self.zones.append((znd_id, znd[enemy_off:enemy_off + enemy_len]))
            for idx in range(mpd_len // 8):
                lba = struct.unpack_from('<I', znd, mpd_off + idx * 8)[0]
                if lba not in mpds:
                    continue
                mpd = disc.read_lba(lba, mpds[lba].size)
                key = (znd_id << 8) | idx
                hdr = struct.unpack_from('<12I', mpd, 0)
                if hdr[9]:
                    self.spawns.append((znd_id, key, mpd[hdr[8]:hdr[8] + hdr[9]]))
                if hdr[11] == 0x220:
                    self.chests.append((key, mpd[hdr[10]:hdr[10] + hdr[11]]))

    def records(self):
        recs = [(KIND_CHESTS, k, 0, t) for k, t in self.chests]
        recs += [(KIND_ZND_ENEMIES, z, 0, d) for z, d in self.zones]
        recs += [(KIND_MPD_DROPS, z, k, d) for z, k, d in self.spawns]
        recs += [(KIND_ASHLEY_STATS, 0, 0, self.ashley), (KIND_ASHLEY_GEAR, 0, 0, self.inventory),
                 (KIND_ABILITIES, 0, 0, self.abilities)]
        return recs


def run(exe, seed, options, records):
    stdin = b''.join(struct.pack('<4I', kind, a, b, len(d)) + d for kind, a, b, d in records)
    out = subprocess.run([exe, str(seed), str(options)], input=stdin, capture_output=True, check=True).stdout
    res, p = [], 0
    for *_, d in records:
        res.append(out[p:p + len(d)])
        p += len(d)
    return res


def material_cap(options, area):
    """Mirror of chests.c materialCap()."""
    if not options & OPT_BALANCE_QUANTITIES or area is None:
        return 0xFF
    return 4 if area < 8 else 5 if area < 13 else 6 if area < 17 else 0xFF


# Treasure section records: (presence flag offset, record offset, kind).
LOOT_RECORDS = ([(0, 0x04, 'blade'), (0, 0x30, 'grip')] + [(0, 0x40 + g * 0x1C, 'gem') for g in range(3)]
                + [(0xAC, 0xB0, 'blade'), (0xDC, 0xE0, 'grip'), (0xF0, 0xF4, 'shield')]
                + [(0xF0, 0x11C + g * 0x1C, 'gem') for g in range(3)]
                + [(0x170, 0x174, 'armor'), (0x19C, 0x1A0, 'armor'), (0x1C8, 0x1CC, 'armor'), (0x1F4, 0x1F8, 'gem')])
LOOT_SIZES = {'blade': 0x2C, 'grip': 0x10, 'gem': 0x1C, 'shield': 0x28, 'armor': 0x28}


def check_chest_stats(data, orig, new, options, room_key):
    """Checks the chest stat rolls and returns the section with them masked
    out (in both orig and new), for the equipment checks."""
    if not options & (OPT2_CHEST_STATS | OPT2_BLADE_COST):
        return orig, new
    tier = data.chest_tiers.get(room_key, 3) if options & OPT2_BALANCE_ITEM_STATS else None
    orig, new = bytearray(orig), bytearray(new)
    for flag, off, kind in LOOT_RECORDS:
        n = LOOT_SIZES[kind]
        if not new[flag] or not struct.unpack_from('<H' if kind in ('grip', 'gem') else '<B', new, off)[0]:
            continue
        check_rolled(kind, new[off:off + n], options, tier)
        new[off:off + n] = mask(kind, new[off:off + n], options, CHEST_FIELD_BITS)
        orig[off:off + n] = mask(kind, orig[off:off + n], options, CHEST_FIELD_BITS)
    return bytes(orig), bytes(new)


def check_chest_gear(data, orig, new, options, area, room_key=None):
    """Equipment part of a treasure section (everything before the misc items)."""
    orig, new = check_chest_stats(data, orig, new, options, room_key)
    if not options & OPT_CHEST_GEAR:
        assert orig[:MISC_OFF] == new[:MISC_OFF], 'chest equipment changed'
        return 0
    pool = lambda kind, rec: in_pool(data, kind, rec, options)
    cap = material_cap(options, area)
    mat = lambda rec, off: struct.unpack_from('<H', rec, off)[0]
    changed = 0
    # (presence flag, [(pool, record offset, size)], extra checks)
    if orig[0]:
        blade, grip = new[0x04:0x30], new[0x30:0x40]
        assert pool('blade', blade) and pool('grip', grip)
        assert grip[3] == GRIP_FOR_BLADE[blade[3]], 'weapon grip does not fit its blade'
        assert mat(blade, 0x28) <= cap and new[0x40:0x94] == bytes(0x54), 'weapon material or gems'
        assert new[0x94:0xAC] == orig[0x94:0xAC], 'weapon name changed'
        changed += 1
    if orig[0xAC]:
        assert pool('blade', new[0xB0:0xDC]) and mat(new, 0xB0 + 0x28) <= cap
        changed += 1
    if orig[0xDC]:
        assert pool('grip', new[0xE0:0xF0])
        changed += 1
    if orig[0xF0]:
        assert pool('shield', new[0xF4:0x11C]) and mat(new, 0xF4 + 0x24) <= cap
        assert new[0x11C:0x170] == bytes(0x54), 'shield gems'
        changed += 1
    for flag in (0x170, 0x19C, 0x1C8):
        if orig[flag]:
            piece = new[flag + 4:flag + 0x2C]
            assert pool('armor', piece) and piece[3] == orig[flag + 4 + 3], 'armor slot changed'
            assert piece[3] == 7 or mat(piece, 0x24) <= cap
            changed += 1
    if orig[0x1F4]:
        assert pool('gem', new[0x1F8:0x214])
        changed += 1
    # Flags and absent entries stay as they were.
    for flag, end in ((0, 0xAC), (0xAC, 0xDC), (0xDC, 0xF0), (0xF0, 0x170), (0x170, 0x19C), (0x19C, 0x1C8),
                      (0x1C8, 0x1F4), (0x1F4, 0x214)):
        assert new[flag:flag + 4] == orig[flag:flag + 4]
        if not orig[flag]:
            assert new[flag:end] == orig[flag:end]
    return changed


def check_chests(orig, new, options, key_room):
    changed = 0
    for i in range(3):
        oid, oflags, ocount = struct.unpack_from('<HBB', orig, MISC_OFF + i * 4)
        nid, nflags, ncount = struct.unpack_from('<HBB', new, MISC_OFF + i * 4)
        assert oflags == nflags
        if (options & OPT_CHESTS and options & OPT_LOOT_KEYS and key_room
                and oflags == 3 and oid not in PROGRESSION):
            assert nid in CONSUMABLES or nid in GRIMOIRES or nid in PROGRESSION, hex(nid)
            assert ncount == 1 or nid in CONSUMABLES
            changed += nid != oid
            continue
        if not options & OPT_CHESTS or oflags == 0 or not (oid in CONSUMABLES or oid in GRIMOIRES):
            assert (oid, ocount) == (nid, ncount), f'{oid:#x} must not change'
        elif oid in CONSUMABLES:
            cap = 10 if options & OPT_BALANCE_QUANTITIES else 100
            assert nid in CONSUMABLES and 1 <= ncount <= cap, f'{nid:#x} x{ncount}'
        else:
            assert nid in GRIMOIRES and ncount == 1, f'{nid:#x}'
        changed += nid != oid
    return changed


# Enemy equipment (src/battle/enemygear.c): template layout and balanced
# ranges per story group, as (lo, hi) or a split (pos, -neg) coin flip.
BLADE, GRIP, WGEMS, SHIELD, SGEMS, ACC, LIMBS, LIMB = 0x34, 0x64, 0x94, 0x140, 0x170, 0x204, 0x238, 0x5C
SPLIT = lambda pos, neg: (-neg, pos)
GEAR_RANGES = {  # name: per-group (lo, hi) bounds of the signed byte
    'blade_cls': [(-20, 14)] + [(-20, 20)] * 4, 'blade_aff': [(-3, 10), (-5, 20), (-5, 20), (-5, 30), (-10, 30)],
    'gem_cls': [(-3, 15)] * 3 + [(-5, 15), (-6, 30)], 'gem_aff': [(0, 0)] + [(-15, 30)] * 4,
    'shield_cls': [(-5, -1), (-5, 10), (-5, 10), (-5, 14), (-5, 15)],
    'shield_aff': [(-6, 8), (-6, 14), (-6, 14), (-6, 15), (-6, 19)],
    'acc_cls': [(-10, 10), (-10, 15), (-12, 30), (-12, 30), (-12, 30)],
    'acc_aff': [(-5, 10), (-10, 30), (-10, 30), (-10, 30), (-25, 30)],
    'armor_cls': [(-5, 1), (-5, 20), (-5, 20), (-5, 24), (-5, 27)],
    'armor_aff': [(-5, 10), (-17, 23), (-17, 23), (-17, 23), (-17, 24)],
    'limb_aff': [(-33, 75)] + [(-33, 100)] * 4, 'limb_bep': [(-30, 35), (-30, 38), (-30, 55), (-30, 55), (-30, 60)],
    'grip_bep': [(0, 13)] + [(0, 21)] * 4, 'shield_bep': [(0, 10), (0, 20), (0, 20), (0, 25), (0, 25)],
    'armor_bep': [(0, 5)] + [(0, 12)] * 4, 'acc_bep': [(0, 0), (0, 15), (0, 15), (0, 20), (-20, 20)],
}
BLADE_IDS = [(1, 12), (13, 24), (25, 34), (35, 50), (51, 56), (57, 62), (63, 69), (70, 82), (83, 90)]


def gear_regions(t, options):
    """Template byte ranges each enabled option may change: (name, offset, length, range key)."""
    base, cls, aff, bep = (options & b for b in (OPT2_ENEMY_GEAR_BASE, OPT2_ENEMY_GEAR_CLASS,
                                                 OPT2_ENEMY_GEAR_AFFINITY, OPT2_ENEMY_GEAR_BEP))
    regions = []

    def equip(off, kind, has_bep, gems_off=None, slots=0):
        if base:
            regions.append(('base', off, 0x1C, None))
        if cls:
            regions.append(('cls', off + 0x20, 6, kind + '_cls'))
        if aff:
            regions.append(('aff', off + 0x28, 7, kind + '_aff'))
        if bep and has_bep:
            regions.append(('bep', off + 0x1D, 3, kind + '_bep'))
        for g in range(min(slots, 3)):
            gem = gems_off + g * 0x30
            if base:
                regions.append(('base', gem, 8, None))
            if cls:
                regions.append(('cls', gem + 0x20, 6, 'gem_cls'))
            if aff:
                regions.append(('aff', gem + 0x28, 7, 'gem_aff'))
    if t[BLADE + 4]:
        equip(BLADE, 'blade', False, WGEMS, t[GRIP + 0x15])
        if base:
            regions += [('base', GRIP, 8, None), ('base', 0x124, 2, None)]
        if bep:
            regions.append(('bep', GRIP + 0x1D, 3, 'grip_bep'))
    if struct.unpack_from('<H', t, SHIELD)[0]:
        equip(SHIELD, 'shield', True, SGEMS, t[SHIELD + 0x15])
        if base:
            regions.append(('base', 0x200, 2, None))
    if struct.unpack_from('<H', t, ACC)[0]:
        equip(ACC, 'acc', True)
        if base:
            regions.append(('base', 0x234, 1, None))
    for l in range(6):
        limb = LIMBS + l * LIMB
        if t[limb + 0x24]:
            equip(limb + 0x20, 'armor', True)
            if base:
                regions.append(('base', limb + 0x50, 2, None))
        if options & OPT2_ENEMY_APPENDAGES and struct.unpack_from('<H', t, limb)[0]:
            regions.append(('limb', limb, 4, None))
            if bep:
                regions.append(('bep', limb + 5, 3, 'limb_bep'))
            if aff:
                regions.append(('aff', limb + 8, 7, 'limb_aff'))
    return regions


def check_gear(data, znd_id, i, o, t, options):
    group = ZONE_GROUPS[znd_id] if options & OPT_BALANCE_ENEMIES else None
    regions = gear_regions(o, options)
    allowed = {off + k for _, off, n, _ in regions for k in range(n)}
    signed = lambda b: b - 256 if b > 127 else b
    for _, off, n, key in regions:
        if key and group is not None:
            lo, hi = GEAR_RANGES[key][group]
            for k in range(n):
                assert lo <= signed(t[off + k]) <= hi, (znd_id, i, key, hex(off + k), signed(t[off + k]))
    if options & OPT2_ENEMY_GEAR_BASE:
        if o[BLADE + 4]:
            bid = struct.unpack_from('<H', t, BLADE)[0]
            lo, hi = BLADE_IDS[o[BLADE + 4] - 1]
            assert lo <= bid <= hi, (znd_id, i, 'blade id', bid)
            assert any(b[0] == bid and b[1] == t[BLADE + 2] and b[2] == t[BLADE + 3] for b in data.pool['blade']), bid
        present = [(BLADE, True)] if o[BLADE + 4] else []
        present += [(SHIELD, True)] if struct.unpack_from('<H', o, SHIELD)[0] else []
        present += [(LIMBS + l * LIMB + 0x20, False) for l in range(6) if o[LIMBS + l * LIMB + 0x24]]
        for off, pp in present:
            cur, mx = struct.unpack_from('<HH', t, off + 8)
            assert mx % 100 == 0 and cur % 100 == 0 and 4100 <= cur <= mx <= 32700, (znd_id, i, hex(off), cur, mx)
            if pp:
                cpp, mpp = struct.unpack_from('<HH', t, off + 12)
                assert 1 <= cpp <= mpp and 100 <= mpp <= 250, (znd_id, i, hex(off), cpp, mpp)
    if options & OPT2_ENEMY_APPENDAGES:
        hp = struct.unpack_from('<h', t, 0x1C)[0]
        for l, div in enumerate((2, 2, 3, 1, 1, 3)):
            lhp = struct.unpack_from('<H', t, LIMBS + l * LIMB)[0]
            if struct.unpack_from('<H', o, LIMBS + l * LIMB)[0]:
                assert 1 <= lhp <= max(1, hp // div), (znd_id, i, l, lhp, hp)
            else:
                assert lhp == 0
    return allowed


def check_zone(znd_id, orig, new, options):
    n = struct.unpack_from('<I', orig, 0)[0]
    base = 4 + n * 8
    if znd_id == 28 and options & OPT2_ICE_WOLF:
        # Hellhound (template 0): ice breath, fire/water affinities swapped; then
        # treated as the vanilla template for the other checks.
        t = base
        assert struct.unpack_from('<H', new, t + 0x24C)[0] == 159, 'ice wolf breath'
        new = bytearray(new)
        struct.pack_into('<H', new, t + 0x24C, 147)
        if not (options & OPT2_ENEMY_APPENDAGES and options & OPT2_ENEMY_GEAR_AFFINITY):  # else re-rolled
            assert (new[t + 0x242], new[t + 0x244]) == (231, 45), 'ice wolf affinities'
            new[t + 0x242], new[t + 0x244] = 45, 231
        new = bytes(new)
    if options & OPT_ZENITH_QUIET and not options & OPT_ENEMY_STATS:
        expected = bytearray(orig)
        for (z, e), slots in QUIET.items():
            if z == znd_id:
                for off, spell in slots.items():
                    at = base + e * TEMPLATE + 0x238 + 2 * 0x5C + off
                    assert orig[at] == spell, f'zone {z} enemy {e}: vanilla spell is {orig[at]}'
                    expected[at] = 0
        assert bytes(expected) == new, f'zone {znd_id}: quiet enemies changed the wrong bytes'
        return sum(len(s) for (z, _), s in QUIET.items() if z == znd_id)
    assert orig[:base] == new[:base], 'ZUD table changed'
    if not options & (OPT_ENEMY_STATS | OPT2_ENEMY_GEAR) or znd_id not in ZONE_GROUPS:
        assert orig == new, f'zone {znd_id} must stay vanilla'
        return 0
    caps = BALANCED_CAPS[ZONE_GROUPS[znd_id]] if options & OPT_BALANCE_ENEMIES else (999, 999, 255, 255, 255)
    for i in range(n):
        o, t = orig[base + i * TEMPLATE:][:TEMPLATE], new[base + i * TEMPLATE:][:TEMPLATE]
        diff = {j for j in range(TEMPLATE) if o[j] != t[j]}
        allowed = check_gear(GEAR_DATA, znd_id, i, o, t, options) if options & OPT2_ENEMY_GEAR else set()
        if options & OPT_ENEMY_STATS:
            allowed |= STAT_BYTES
        assert diff <= allowed, f'zone {znd_id} enemy {i}: unexpected bytes {sorted(map(hex, diff - allowed))}'
        if not options & OPT_ENEMY_STATS:
            continue
        hp, mp = struct.unpack_from('<hh', t, 0x1C)
        stats = (hp, mp, t[0x20], t[0x21], t[0x22])
        assert all(1 <= v <= c for v, c in zip(stats, caps)), (znd_id, i, stats, caps)
        assert 1 <= t[0x25] <= 14 and 1 <= t[0x27] <= 28
    return n


def check_spawns(znd_id, orig, new, options, key_room, room_key=None):
    changed = 0
    if options & OPT_ZENITH_BOSS_GRIMOIRES and not options & OPT_ENEMY_DROPS:
        if room_key in BOSS_GRIMOIRES:
            items, counts = BOSS_GRIMOIRES[room_key]
            assert struct.unpack_from('<3H', new, 0x1A) == items and tuple(new[0x20:0x23]) == counts
            assert orig[:0x1A] == new[:0x1A] and orig[0x23:] == new[0x23:], 'boss spawn: other bytes changed'
            return 1
        assert orig == new
        return 0
    for i in range(len(orig) // SPAWN):
        o, t = orig[i * SPAWN:(i + 1) * SPAWN], new[i * SPAWN:(i + 1) * SPAWN]
        oid = struct.unpack_from('<H', o, 0x1E)[0]
        if (not options & OPT_ENEMY_DROPS or znd_id not in ZONE_GROUPS or o[0] != 0
                or oid in PROGRESSION):
            assert o == t, f'zone {znd_id} spawn {i} must not change'
            continue
        diff = {j for j in range(SPAWN) if o[j] != t[j]}
        assert diff <= {0x1E, 0x1F, 0x22}, f'zone {znd_id} spawn {i}: bytes {sorted(diff)}'
        nid = struct.unpack_from('<H', t, 0x1E)[0]
        loot_keys = options & OPT_LOOT_KEYS and key_room
        assert nid in CONSUMABLES or nid in GRIMOIRES or (loot_keys and nid in PROGRESSION), f'{nid:#x}'
        assert 1 <= t[0x22] <= 100
        changed += 1
    return changed


def check_ashley_body(orig, new, options):
    """Ashley's body part BEP/affinities; returns the bytes they may change."""
    allowed = set()
    signed = lambda b: b - 256 if b > 127 else b
    for l in range(6):
        limb = 0x238 + l * 0x5C
        if not struct.unpack_from('<H', orig, limb)[0]:
            continue
        for bit, off, n, (lo, hi) in ((OPT2_ASHLEY_BEP, 5, 3, (-30, 35)), (OPT2_ASHLEY_AFFINITY, 8, 7, (-33, 75))):
            if options & bit:
                allowed |= set(range(limb + off, limb + off + n))
                if options & OPT_BALANCE_ASHLEY:
                    assert all(lo <= signed(new[limb + off + k]) <= hi for k in range(n)), (l, bit)
    return allowed


def check_ashley_stats(orig, new, options):
    body = check_ashley_body(orig, new, options)
    diff = {j for j in range(TEMPLATE) if orig[j] != new[j]}
    if not options & OPT_ASHLEY_STATS:
        assert diff <= body, sorted(diff - body)
        return 0
    assert diff <= STAT_BYTES | body, sorted(diff - STAT_BYTES - body)
    hp, mp = struct.unpack_from('<hh', new, 0x1C)
    if options & OPT_BALANCE_ASHLEY:
        lo, hi = THRESHOLDS[(options >> OPT2_THRESHOLD_SHIFT) & 3]
        assert 40 <= hp <= 480 and 5 <= mp <= 130 and all(lo <= new[o] <= hi for o in (0x20, 0x21, 0x22))
    else:
        assert 40 <= hp <= 999 and 5 <= mp <= 999 and all(1 <= new[o] <= 255 for o in (0x20, 0x21, 0x22))
    assert 10 <= new[0x25] <= 14 and 20 <= new[0x27] <= 28
    return 1


def strip(kind, rec):
    return gen_equipment.clear_slot_fields(kind, rec)


# Ashley's balanced gear ranges (the first enemy group's): kind -> field -> (lo, hi).
ASHLEY_RANGES = {'blade': {'class': (-20, 14), 'aff': (-3, 10)}, 'grip': {'bep': (0, 13)},
                 'armor': {'bep': (0, 5), 'class': (-5, 1), 'aff': (-5, 10)},
                 'accessory': {'bep': (0, 0), 'class': (-10, 10), 'aff': (-5, 10)}}


def check_ashley_rolls(orig, new, options):
    """Checks the class/affinity/BEP rolls on the starting gear and returns
    both inventories with those bytes masked."""
    if not options & (OPT2_ASHLEY_GEAR | OPT2_FANDANGO_COST):
        return orig, new
    signed = lambda b: b - 256 if b > 127 else b
    orig, new = bytearray(orig), bytearray(new)
    for kind, base, size, count in (('blade', 0x280, 0x2C, 16), ('grip', 0x540, 0x10, 16), ('armor', 0x640, 0x28, 16)):
        for i in range(count):
            off = base + i * size
            rec = new[off:off + size]
            if not (struct.unpack_from('<H', rec)[0] if kind == 'grip' else rec[0]):
                continue
            ranges = ASHLEY_RANGES['accessory' if kind == 'armor' and rec[3] == 7 else kind]
            for field, offs in stat_fields(kind, rec, options, ASHLEY_FIELD_BITS):
                if field == 'cost':
                    check_cost(rec)
                elif options & OPT_BALANCE_ASHLEY:
                    lo, hi = ranges[field]
                    assert all(lo <= signed(rec[o]) <= hi for o in offs), (kind, i, field)
            new[off:off + size] = mask(kind, rec, options, ASHLEY_FIELD_BITS)
            orig[off:off + size] = mask(kind, orig[off:off + size], options, ASHLEY_FIELD_BITS)
    return bytes(orig), bytes(new)


def check_ashley_gear(data, orig, new, options):
    balanced = options & OPT_BALANCE_ASHLEY
    changed = 0
    orig, new = check_ashley_rolls(orig, new, options)
    pool = lambda kind, rec: in_pool(data, kind, strip(kind, rec), options, ASHLEY_FIELD_BITS)
    # Weapon 0 keeps its slot, name and assembly; blade and grip come from the pool.
    assert orig[:0x100] == new[:0x100], 'weapon records changed'
    w = orig[0:0x20]
    b_off, g_off = 0x280 + (w[1] - 1) * 0x2C, 0x540 + (w[2] - 1) * 0x10
    ob, nb = orig[b_off:b_off + 0x2C], new[b_off:b_off + 0x2C]
    og, ng = orig[g_off:g_off + 0x10], new[g_off:g_off + 0x10]
    if options & OPT_ASHLEY_WEAPON:
        assert pool('blade', nb), 'blade not from the pool'
        assert pool('grip', ng), 'grip not from the pool'
        assert ng[3] == GRIP_FOR_BLADE[nb[3]], f'blade category {nb[3]} with grip category {ng[3]}'
        assert nb[0x2A:0x2C] == ob[0x2A:0x2C] and ng[0x0C:0x10] == og[0x0C:0x10], 'slot fields changed'
        if balanced:
            assert struct.unpack_from('<H', nb, 0x28)[0] <= 4, 'balanced blade above Iron'
        changed += nb != ob
    else:
        assert (ob, og) == (nb, ng)
    rest = lambda inv: inv[0x280:b_off] + inv[b_off + 0x2C:g_off] + inv[g_off + 0x10:0x640]
    assert rest(orig) == rest(new), 'other blades/grips changed'
    for i in range(16):
        oa, na = orig[0x640 + i * 0x28:][:0x28], new[0x640 + i * 0x28:][:0x28]
        if not options & OPT_ASHLEY_ARMOR or oa[0] == 0:
            assert oa == na
            continue
        assert pool('armor', na), f'armor {na[0]} not from the pool'
        assert na[3] == oa[3] and na[0x26:0x28] == oa[0x26:0x28], 'armor slot or limb changed'
        if balanced and na[3] != 7:
            assert struct.unpack_from('<H', na, 0x24)[0] <= 4
        changed += na != oa
    assert orig[0x8C0:0xE00] == new[0x8C0:0xE00], 'gems changed'
    added = []
    for i in range(64):
        oid, ocount, oidx = struct.unpack_from('<HBB', orig, 0xE00 + i * 4)
        nid, ncount, nidx = struct.unpack_from('<HBB', new, 0xE00 + i * 4)
        assert oidx == nidx
        if options & OPT2_OPEN_WORLD and oid == 0 and nid:
            assert ncount == 1
            added.append(nid)
            continue
        if not options & OPT_ASHLEY_ITEMS or oid == 0:
            assert (oid, ocount) == (nid, ncount)
            continue
        assert nid in CONSUMABLES or nid in GRIMOIRES, hex(nid)
        if nid in GRIMOIRES:
            assert ncount == 1
        elif options & OPT_BALANCE_QUANTITIES:
            assert ncount == ocount
        else:
            assert 1 <= ncount <= 99
        changed += nid != oid
    if options & OPT2_OPEN_WORLD:
        assert added == list(KEYS_AND_SIGILS), 'open world: every key and sigil once'
    else:
        assert not added
    return changed


def check_iron_maiden(data, exe):
    """Mandatory Iron Maiden on the real MPDs: exactly Rooms::mandyMaiden's bytes."""
    rooms = {171: (0x2700, {56744: 1, 56748: 0xE6, 56749: 1, 56764: 1, 56768: 0xE6, 56769: 1}),
             145: (0x1F00, {60618: 0xCD, 60619: 1, 60620: 0xCE, 60621: 1, 60624: 1, 60625: 1})}
    recs = [(KIND_ROOM, key, 0, data.disc.read_file(f'MAP/MAP{m:03d}.MPD')) for m, (key, _) in rooms.items()]
    for options in (0, OPT2_IRON_MAIDEN):
        out = run(exe, 1, options | OPT_CHESTS, recs)
        for (_, key, _, orig), new, (m, (_, writes)) in zip(recs, out, rooms.items()):
            expected = bytearray(orig)
            if options:
                for off, val in writes.items():
                    expected[off] = val
            assert bytes(expected) == new, f'MAP{m}: iron maiden bytes'


def loot_key_sequence(data, results, kind_wanted):
    """Keys/sigils produced by one pass, in story (table) order."""
    by_key = {}
    for (kind, a, b, orig), new in zip(data.records(), results):
        if kind != kind_wanted:
            continue
        if kind == KIND_CHESTS:
            ids = [struct.unpack_from('<H', new, MISC_OFF + i * 4)[0] for i in range(3)]
            olds = [struct.unpack_from('<H', orig, MISC_OFF + i * 4)[0] for i in range(3)]
            by_key[a] = [n for n, o in zip(ids, olds) if n in PROGRESSION and o not in PROGRESSION]
        else:
            seq = []
            for i in range(len(orig) // SPAWN):
                o, n = (struct.unpack_from('<H', x, i * SPAWN + 0x1E)[0] for x in (orig, new))
                if n in PROGRESSION and o not in PROGRESSION:
                    seq.append(n)
            by_key[b] = seq
    return [(area, pre, item) for key, area, pre, _, _ in data.key_rooms for item in by_key.get(key, [])]


def check_key_order(sequence):
    bronze_area = silver_area = None
    out_of_order = lambda found, here: (KEY_RANK.get(found, -1) >= 0
                                        and KEY_RANK.get(here, 99) > KEY_RANK[found])
    for area, pre, item in sequence:
        assert not (item == CHAMOMILE and pre), 'Chamomile Sigil before the Minotaur'
        if item == BRONZE:
            bronze_area = area
        elif item in (SILVER, IRON):
            assert bronze_area is not None, 'Silver/Iron Key before any Bronze Key'
            assert not out_of_order(bronze_area, area), 'key outside the allowed areas'
            if item == SILVER:
                silver_area = area
            else:
                assert silver_area is not None, 'Iron Key before any Silver Key'
                assert not out_of_order(silver_area, area), 'Iron Key outside the allowed areas'
    return len(sequence)


# Ashley builds as in Ashley.cpp: HP, MP, STR, INT, AGL lower bounds (ranges are 11 wide),
# blade, grip, and per item slot the allowed ids (minus 0x100).
GRIMOIRE_SET = {i - 0x100 for i in GRIMOIRES}
SIGILS = set(range(211, 234))
BUILDS = {
    1: ((185, 15, 85, 55, 105), 2, 96, [{89}, {92}, {100}, {202} | SIGILS,
                                        {136, 138, 163, 167, 168, 179, 180, 185, 188, 190, 191}]),
    2: ((320, 30, 105, 75, 95), 26, 96, [{90}, {93}, set(range(95, 100)), {135, 137, 159, 169}, {175, 181}]),
    3: ((220, 45, 100, 85, 95), 36, 105, [{88}, {89}, {95}, {175}, {181}]),
    4: ((155, 30, 65, 75, 95), 44, 105, [{80}, {87}, set(range(90, 95)), {139, 159, 168, 169, 173},
                                         {177, 181, 184, 185, 186, 187, 189}]),
    5: ((235, 45, 95, 90, 90), 52, 105, [{78}, {93}, {98}, {136, 138, 163, 167, 168}, {176, 178, 180, 182, 185}]),
    6: ((235, 50, 100, 90, 95), 64, 105, [{88}, {90}, {92}, {176, 178, 180} | set(range(182, 188)),
                                          set(range(188, 192))]),
    7: ((235, 15, 105, 45, 95), 71, 115, [{78}, {87, 89}, {90, 92}, {173}, {181}]),
    8: ((145, 55, 50, 145, 95), 58, 105, [{74}, {87}, {91, 94}, SIGILS, GRIMOIRE_SET]),
    9: ((195, 25, 95, 75, 95), 84, 121, [{78}, {85}, {92}, {131, 143, 193, 197}, {173, 180, 188, 191, 192}]),
}


def check_build(data, stats, inv, build_value):
    """Returns the resolved build, checking stats, weapon and items against it."""
    hp, mp = struct.unpack_from('<hh', stats, 0x1C)
    got = (hp, mp, stats[0x20], stats[0x21], stats[0x22])
    matches = [b for b, (lo, *_) in BUILDS.items() if all(l <= v <= l + 10 for l, v in zip(lo, got))]
    assert matches, f'build stats {got} match no build'
    build = build_value if build_value != 10 else matches[0]
    lo, blade, grip, items = BUILDS[build]
    assert all(l <= v <= l + 10 for l, v in zip(lo, got)), (build, got)
    w = inv[0:0x20]
    assert inv[0x280 + (w[1] - 1) * 0x2C] == blade, 'build blade'
    assert struct.unpack_from('<H', inv, 0x540 + (w[2] - 1) * 0x10)[0] == grip, 'build grip'
    for i, allowed in enumerate(items):
        iid, count, _ = struct.unpack_from('<HBB', inv, 0xE00 + i * 4)
        assert iid - 0x100 in allowed, f'build {build} item {i}: {iid:#x}'
        assert count == 1 or iid in CONSUMABLES
    return build


# Ultimate spell records as hardcoded in Ashley::ultiSpells, for actions 109, 113, ... 133.
ULTIMATES = [
    [134, 81, 99, 35, 12, 12, 12, 1, 8, 8, 8, 1, 2, 160, 0, 144, 0, 0, 0, 0, 58, 161, 3, 32, 0, 0, 0, 0, 29, 50, 53, 49,
     36, 39, 50, 231, 182, 4, 231, 42, 40, 250, 6, 182, 4, 231, 0, 0, 0, 0, 0, 0],
    [135, 82, 99, 35, 12, 12, 12, 1, 8, 8, 8, 1, 2, 160, 0, 144, 0, 0, 0, 0, 58, 161, 3, 64, 0, 0, 0, 0, 29, 43, 56, 49,
     39, 40, 53, 37, 50, 47, 55, 231, 40, 250, 6, 182, 4, 231, 0, 0, 0, 0, 0, 0],
    [136, 83, 99, 35, 12, 12, 12, 1, 8, 8, 8, 1, 2, 160, 0, 144, 0, 0, 0, 0, 58, 161, 3, 96, 0, 0, 0, 0, 15, 44, 53, 40,
     250, 6, 28, 55, 50, 53, 48, 231, 40, 250, 6, 182, 4, 231, 0, 0, 0, 0, 0, 0],
    [137, 84, 99, 35, 11, 5, 11, 2, 8, 6, 8, 2, 2, 160, 0, 144, 0, 0, 0, 0, 58, 161, 3, 128, 0, 0, 0, 0, 16, 53, 36, 57,
     44, 55, 60, 231, 50, 53, 48, 231, 40, 250, 6, 182, 4, 231, 0, 0, 0, 0, 0, 0],
    [138, 85, 99, 35, 12, 12, 12, 1, 8, 8, 8, 1, 2, 160, 0, 144, 0, 0, 0, 0, 58, 161, 3, 160, 0, 0, 0, 0, 10, 38, 44, 39,
     250, 6, 15, 47, 50, 58, 231, 231, 40, 250, 6, 182, 4, 231, 0, 0, 0, 0, 0, 0],
    [139, 86, 99, 65, 12, 12, 12, 1, 8, 8, 8, 1, 2, 160, 0, 176, 0, 0, 0, 0, 58, 161, 131, 192, 0, 0, 0, 0, 19, 56, 39,
     42, 48, 40, 49, 55, 231, 58, 231, 231, 40, 250, 6, 182, 4, 231, 0, 0, 0, 0, 0, 0],
    [140, 87, 99, 65, 12, 12, 12, 1, 8, 8, 8, 1, 2, 160, 0, 176, 0, 0, 0, 0, 186, 162, 131, 224, 0, 0, 0, 0, 10, 51, 50,
     38, 36, 47, 60, 51, 54, 40, 231, 231, 40, 250, 6, 182, 4, 231, 0, 0, 0, 0, 0, 0],
]
RISK_ABILITIES = [22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34, 35, 37, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
                  51, 52, 53]


def check_abilities(orig, new, options):
    acts = lambda b: [b[i * 0x34:(i + 1) * 0x34] for i in range(224)]
    o, n = acts(orig), acts(new)
    pts = lambda b, off, count: list(struct.unpack_from(f'<{count}H', b, off))
    o_arts, n_arts = pts(orig, 224 * 0x34, 40), pts(new, 224 * 0x34, 40)
    o_chain, n_chain = pts(orig, 224 * 0x34 + 80, 22), pts(new, 224 * 0x34 + 80, 22)
    o_frames = struct.unpack_from('<2I', orig, len(orig) - 8)
    n_frames = struct.unpack_from('<2I', new, len(new) - 8)
    changed = set()
    if options & OPT_BREAK_ARTS:
        lo, hi, plo, phi = (25, 75, 10, 485) if options & OPT_BALANCE_BREAK_ARTS else (0, 255, 1, 999)
        assert all(lo <= n[184 + i][3] <= hi for i in range(40)), 'Break Art cost'
        assert all(plo <= p <= phi for p in n_arts), 'Break Art points'
        changed |= set(range(184, 224))
    else:
        assert o_arts == n_arts
    if options & OPT_CHAIN_LEARNING:
        assert all(a < b for a, b in zip(n_chain, n_chain[1:])), 'ability points not increasing'
        assert all(0 <= v <= c for v, c in zip(n_chain, o_chain)), 'ability points above vanilla'
    else:
        assert o_chain == n_chain
    if options & OPT_RISK_COSTS:
        assert all(1 <= n[i][3] <= 6 for i in RISK_ABILITIES)
        changed |= set(RISK_ABILITIES)
    if options & OPT_ULTIMATE_SPELLS:
        for k in range(7):
            assert list(n[109 + 4 * k]) == ULTIMATES[k], f'ultimate {k} differs from Ashley::ultiSpells'
            changed.add(109 + 4 * k)
    if options & OPT_FRAME_WINDOWS:
        assert n_frames[0] == n_frames[1] and 0x24100000 + 8 <= n_frames[0] <= 0x24100000 + 30
    else:
        assert n_frames == o_frames
    for i in range(224):
        if i in changed:
            # Only the cost byte changes, except for replaced spells.
            if i not in range(109, 134, 4):
                assert o[i][:3] == n[i][:3] and o[i][4:] == n[i][4:], f'action {i}'
        else:
            assert o[i] == n[i], f'action {i} changed'
    return len(changed) + (options & OPT_CHAIN_LEARNING and 22) + (options & OPT_FRAME_WINDOWS and 1)


def check_drop_rolls(exe):
    """Zenith repeat-kill drops: rando_dropRoll against a model of Zenith's
    routine (with each history cycling on its own)."""
    import random
    rnd = random.Random(7)
    calls = []
    for _ in range(4000):
        kind = rnd.randrange(4)
        calls.append((rnd.randrange(255), rnd.choice((0, 2, 3, 4, 10, 40, 120, 250)), kind,
                      rnd.randrange(7) if kind == 3 else 0, rnd.choice((0, 1, 2, 3, 5, 0x101, 0x102))))
    # Long runs of one item, to reach the larger boosts.
    calls += [(200, 40, 0, 0, 9)] * 12 + [(45, 20, 3, 2, 9)] * 12
    recs = b''.join(struct.pack('<4BHH', *c, 0) for c in calls)
    out, = run(exe, 1, 0, [(KIND_DROP_ROLLS, 0, 0, recs)])
    history = [[] for _ in range(8)]
    boosted = 0
    for i, (roll, rate, kind, category, item) in enumerate(calls):
        got = struct.unpack_from('<H', out, i * 8 + 6)[0]
        if rate >= 4 and item:
            h = history[kind + ((category if category < 5 else 1) if kind == 3 else 0)]
            seen = h.count(item)
            h.append(item)
            del h[:-8]
            if seen >= 3:
                rate += (2 if seen < 6 else rate >> 2) * seen
                boosted += 1
        assert got == (roll < rate), f'drop roll {i} {calls[i]}: got {got}'
    tail = [struct.unpack_from('<H', out, len(out) - 8 * j + 6)[0] for j in (12, 1)]
    assert tail == [0, 1], 'a roll of 45 against a rate of 20 should succeed after enough kills'
    return boosted


def check(data, results, options):
    key_rooms = {r[0] for r in data.key_rooms}
    areas = {r[0]: r[1] for r in data.key_rooms}
    counts = {'abilities': 0, 'chest items': 0, 'chest gear': 0, 'enemy templates': 0, 'spawn drops': 0, 'ashley stats': 0, 'ashley gear': 0}
    for (kind, a, b, orig), new in zip(data.records(), results):
        if kind == KIND_CHESTS:
            counts['chest items'] += check_chests(orig, new, options, a in key_rooms)
            counts['chest gear'] += check_chest_gear(data, orig, new, options, areas.get(a), a)
        elif kind == KIND_ZND_ENEMIES:
            counts['enemy templates'] += check_zone(a, orig, new, options)
        elif kind == KIND_ASHLEY_STATS:
            counts['ashley stats'] += check_ashley_stats(orig, new, options)
        elif kind == KIND_ASHLEY_GEAR:
            counts['ashley gear'] += check_ashley_gear(data, orig, new, options)
        elif kind == KIND_ABILITIES:
            counts['abilities'] += check_abilities(orig, new, options)
        else:
            counts['spawn drops'] += check_spawns(a, orig, new, options, b in key_rooms, b)
    if options & OPT_LOOT_KEYS:
        counts['loot keys'] = sum(check_key_order(loot_key_sequence(data, results, kind))
                                  for kind in (KIND_CHESTS, KIND_MPD_DROPS))
    return counts


def main():
    global GEAR_DATA
    disc = psxdisc.Disc(ROOT / 'reference' / 'Vagrant Story (USA).bin')
    data = GEAR_DATA = GameData(disc)
    data.disc = disc
    records = data.records()
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / 'host_rando'
        equipment = Path(tmp) / 'equipment.c'
        gen_equipment.emit(data.pools, equipment)
        keyrooms = Path(tmp) / 'keyrooms.c'
        gen_keyrooms.emit(data.key_rooms, keyrooms)
        sources = [ROOT / 'src/payload' / f for f in ('abilities.c', 'ashley.c', 'builds.c', 'config.c', 'zenith.c', 'chests.c', 'enemies.c', 'gear.c',
                                                      'items.c', 'keys.c', 'rng.c')]
        sources += [ROOT / 'src/battle' / f for f in ('enemygear.c', 'cheststats.c', 'roll.c', 'progression.c')]
        chesttiers = Path(tmp) / 'chesttiers.c'
        gen_chesttiers.emit(sorted(data.chest_tiers.items()), chesttiers)
        sources.append(chesttiers)
        subprocess.run(['cc', '-O2', '-Wall', '-Werror', '-fwrapv', '-DRANDO_HOST', '-I', ROOT / 'src/payload', '-o', exe,
                        ROOT / 'tests/host_rando.c', equipment, keyrooms, *sources], check=True)

        print(f'drop rolls OK ({check_drop_rolls(exe)} boosted)')
        check_iron_maiden(data, exe)
        print('iron maiden OK')
        a = run(exe, 1234, ALL, records)
        # Ashley builds: every fixed build and Random over a few seeds.
        ashley = [r for r in records if r[0] in (KIND_ASHLEY_STATS, KIND_ASHLEY_GEAR)]
        resolved = set()
        for value in range(1, 11):
            for seed in range(1, 6 if value == 10 else 2):
                stats, inv = run(exe, seed, value << 12, ashley)
                build = check_build(data, stats, inv, value)
                if value == 10:
                    resolved.add(build)
                assert inv[0x640:0x8C0] == data.inventory[0x640:0x8C0], 'build changed armor'
        print(f'builds OK; Random gave {sorted(resolved)} over 5 seeds')
        for seed in range(1, 41):  # key-order rules over many seeds
            check(data, run(exe, seed, OPT_CHESTS | OPT_ENEMY_DROPS | OPT_LOOT_KEYS, records),
                  OPT_CHESTS | OPT_ENEMY_DROPS | OPT_LOOT_KEYS)
        assert a == run(exe, 1234, ALL, records), 'not deterministic'
        assert a != run(exe, 5678, ALL, records), 'seed has no effect'
        for options in (ALL, ALL & ~BALANCE, 0, OPT_CHESTS, OPT_ENEMY_STATS, OPT_ENEMY_DROPS,
                        OPT_ASHLEY_STATS, OPT_ASHLEY_WEAPON | OPT_BALANCE_ASHLEY, OPT_ASHLEY_ARMOR,
                        OPT_ASHLEY_ITEMS | OPT_BALANCE_QUANTITIES,
                        OPT_CHESTS | OPT_ENEMY_DROPS | OPT_LOOT_KEYS, OPT_CHEST_GEAR,
                        OPT_CHEST_GEAR | OPT_BALANCE_QUANTITIES, OPT_BREAK_ARTS, OPT_CHAIN_LEARNING,
                        OPT_ULTIMATE_SPELLS, OPT_FRAME_WINDOWS | OPT_RISK_COSTS, OPT_ZENITH_QUIET,
                        OPT_ZENITH_BOSS_GRIMOIRES, OPT2_ENEMY_GEAR, OPT2_ENEMY_GEAR | OPT_BALANCE_ENEMIES,
                        OPT2_ENEMY_GEAR_CLASS | OPT_BALANCE_ENEMIES, OPT2_ENEMY_APPENDAGES | OPT_ENEMY_STATS,
                        OPT2_ENEMY_GEAR | OPT_ENEMY_STATS | OPT_BALANCE_ENEMIES, OPT2_CHEST_STATS,
                        OPT2_CHEST_STATS | OPT2_BALANCE_ITEM_STATS, OPT2_CHEST_STATS | OPT_CHEST_GEAR | OPT2_BALANCE_ITEM_STATS,
                        OPT2_CHEST_GEAR_BEP | OPT_CHEST_GEAR, OPT2_ASHLEY_GEAR, OPT2_ASHLEY_GEAR | OPT_BALANCE_ASHLEY,
                        OPT2_ASHLEY_GEAR | OPT_ASHLEY_WEAPON | OPT_ASHLEY_ARMOR | OPT_ASHLEY_STATS | OPT_BALANCE_ASHLEY,
                        OPT_ASHLEY_STATS | OPT_BALANCE_ASHLEY | 1 << OPT2_THRESHOLD_SHIFT,
                        OPT_ASHLEY_STATS | OPT_BALANCE_ASHLEY | 2 << OPT2_THRESHOLD_SHIFT,
                        OPT2_BLADE_COST, OPT2_BLADE_COST | OPT_CHEST_GEAR, OPT2_FANDANGO_COST | OPT_ASHLEY_WEAPON,
                        OPT2_ICE_WOLF, OPT2_ICE_WOLF | OPT_ENEMY_STATS, OPT2_OPEN_WORLD,
                        OPT2_OPEN_WORLD | OPT_ASHLEY_ITEMS | OPT_ASHLEY_WEAPON):
            res = a if options == ALL else run(exe, 1234, options, records)
            counts = check(data, res, options)
            print(f'options {options:#04x}: ' + ', '.join(f'{v} {k}' for k, v in counts.items()))
    print(f'OK: {len(data.chests)} chests, {len(data.zones)} zones, {len(data.spawns)} rooms with spawns')


if __name__ == '__main__':
    main()
