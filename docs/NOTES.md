# Vagrant Story (USA) integrated randomizer — technical notes

Target: `Vagrant Story (USA).bin`, single track MODE2/2352, MD5 `8531bc73ea7cd33a4c8e97bdf139ba9c`.
`SLUS_010.40` SHA1 `fababcfd4325d42f350d95b3472874affeb0e48c` matches the Rood Reverse decomp
(https://github.com/ser-pounce/rood-reverse, CC0), which is the main source for addresses and structs.

## RAM map (USA)

| Range | Contents |
|---|---|
| `0x80010000`-`0x80062000` | SLUS_010.40 text/data/bss (exe `t_size` = `0x52000`) |
| `0x80062000`-`0x80068800` | **unused** — no references in the decomp. Our resident payload goes here. |
| `0x80068800` | overlay slot 0: TITLE.PRG or BATTLE.PRG (BATTLE incl. bss ends `0x800F5924`) |
| `0x800F9800` | overlay slot 1: INITBTL.PRG / MAINMENU.PRG / EFFECT PLGs |
| `0x80102800` | overlay slot 2: MENUx.PRG |
| `0x8010C000`-`0x801FE000` | game heap (`vs_main_initHeap(0x8010C000, 0xF2000)`) |
| `0x801FE000`-`0x80200000` | stack (`SYSTEM.CNF` STACK = `801fff00`) |

The exe entry (`pc0` = `0x8001F544`) only clears `0x80033678`-`0x800401A8`.

## Disc layout

The game reads files by hardcoded LBA tables, not via ISO9660. Large runs of empty Form 2
padding sectors exist between directories, e.g. LBA 193-1099 (907 sectors, directly after
DBGFNT.TIM), 76292-84999, 31462-62999, 135752-255999. We relocate SLUS_010.40 into the
193+ gap (updating its ISO directory record) so it can grow to cover `0x80062000`-`0x80068800`.

## Hook points

All hooks replace a `jal` (see `tools/build.py`).

| Address | File | Original | Purpose |
|---|---|---|---|
| `0x80042BD0` | SLUS_010.40 | `jal _loadTitlePrg` in `vs_main_execTitle`, after `_sysReinit` | first boot: load the options menu (LBA 400) into overlay slot 0 and run it |
| `0x80043960` | SLUS_010.40 | `jal vs_main_updatePadState` (port 0) | test harness input injection (`rando_debugButtons`) |
| `0x800715A4` | TITLE.PRG (patched in RAM after `_loadTitlePrg`, not on disc) | `selectedOption = _nop()` on New Game | apply the boot menu's pending settings (config + Ashley's gear); returns 0 |
| `0x800F9E2C` | INITBTL.PRG | `jal func_80076F24(0, &D_800FAB18, ...)` | Ashley's stats, patched into his template each time the battle program starts |
| `0x80088FA8` | BATTLE.PRG | `jal _loadZndEnemies(data, len)` in `_finishLoadZnd` | zone enemy templates (0x464 bytes each), raw ZND buffer still live |
| `0x80089E80` | BATTLE.PRG | `jal _loadMpdRoomSection(0, room)` in `func_80089DC0` | first parse after an MPD finishes loading; `_zoneContext.mpdData` holds the whole file |
| `0x800C9840` | BATTLE.PRG | `jal vs_mainMenu_exec(menu)` in `func_800C97BC`, every frame while a menu is open | seed and preset on the top-level main menu (`menu & 0x3F == 0`) |

`_zoneContext` is at `0x800F1880` (`mpdData` at +0x5C, `zndId` +0x00, `mapId` +0x34).
`D_800F1AB0` byte 0/1 = zone id / map id being entered.

## MPD file

Header = 6 × (offset, length) u32 pairs: room, cleared, script, door, enemy (`_mpdEnemy`, 0x28 bytes
each), treasure. The treasure section, when present, is exactly 0x220 bytes = `vs_battle_loot`
(see `src/payload/vs.h`); 144 of 512 MPDs have one, always at end of file. Entry offsets:
weapon `0x000` (0xAC: flags, blade, grip, 3 gems, name), blade `0x0AC`, grip `0x0DC`, shield
`0x0F0` (with 3 gems), armor `0x170`/`0x19C`, accessory `0x1C8`, gem `0x1F4`, misc `0x214`.
Every present entry in the vanilla data has flags 3; vanilla chest weapons have no gems.

## ZND file (zone)

Header u32 pairs: (mpd list offset, length), (enemy section offset, length), TIM offset, ...
The MPD list is (lba, size) per room; a room's index in it is the `mpdId` used at runtime.
Enemy section = u32 count, count x 8-byte ZUD (model) file entries, count x 0x464-byte templates.
A template is `vs_battle_charInitData` (the game casts it directly): hp `0x1C`, mp `0x1E`,
str/int/agl `0x20`-`0x22` (read unsigned; vanilla has 250), walk `0x25`, run `0x27`, weapon from
`0x34` (its category byte `0x38` is what ChaoticBrave's model swap matches on), shield `0x140`,
accessory `0x204`, six limbs `0x238`. Stats are clamped to 0-999 when an actor spawns.

## MPD enemy spawns

`_mpdEnemy` entries with `type == 0` are enemies (others are object spawns with another layout).
`id` (0x04) indexes the zone's templates. `itemId`/`item2Id` (+counts) are guaranteed drops;
`item3Id` (0x1E) with `item3Count` (0x22) is the chance-based drop, the count being its drop rate.

## Ashley

`D_800FAB18` (INITBTL.PRG, loaded at `0x800F9800`) is Ashley's `vs_battle_charInitData`: 250 HP,
50 MP, 100 STR/INT/AGL, walk 10, run 20. INITBTL runs at every `vs_battle_exec` (New Game and
Continue) and creates Ashley from it; a loaded save then supplies his current stats.

The starting inventory is `D_80074C24` in TITLE.PRG (file offset `0xC424`, a full
`vs_main_inventory_t`): Fandango = Scimitar blade (14) + Short Hilt (96); arm, arm, head, body,
legs armor plus one accessory; Cure Root x10, Vera Root x10, Yggdrasil's Tears x5, Faerie
Chortle x5, Spirit Orison x5. `_initGameData` copies it into `vs_main_inventory` (`0x80060168`)
when the title starts, i.e. before the options menu, so starting gear is randomized in
`vs_main_inventory` when the menu confirms.

Ashley builds (`src/payload/builds.c`, from `Ashley::buildIterate`): the build number lives in
option bits 12-15 (0 off, 1-9 Dagger..Crossbow, 10 Random, resolved from the seed). Build blade
ids: Scramasax 2, Norse Sword 26, Battle Axe 36, Spiked Club 44, Large Crescent 52, Sabre Mace 64,
Spear 71 (the wiki says Glaive; the code writes 71), Clergy Rod 58, Light Crossbow 84; grips
Short Hilt 96, Wooden Grip 105, Wooden Pole 115, Simple Bolt 121.

There is no global equipment table; chests and enemies carry full records. `tools/gen_equipment.py`
builds the pools of distinct blades, grips, shields, armor and gems from both (enemy records converted from the
`vs_battle_uiEquipment` layout; their materials live beside the record, e.g. template `0x124` for
the blade). Blade category to grip category: 1-3 -> 1 (guards), 4-7 -> 2 (grips), 8 -> 3 (poles),
9 -> 4 (bolts). Armor category is the body slot: 2 head, 3 body, 4 legs, 5 arms, 7 accessory.
Note Ashley.cpp's threshold `dist_3, dist_4, dist_5 = ...` only assigns AGL (comma operator).

## Keys, sigils and locks

Every key and sigil has exactly one vanilla source, a chest (always misc slot 2) or a guaranteed
enemy/boss drop (item1; the Chest Key is item3 of MAP366 spawn 0). E.g. Bronze Key = MAP201
boss drop, Iron Key = MAP255 chest, Silver Key = MAP142 chest. `tests/test_rando.py` treats
all of ids 0x1CA-0x1E9 as progression and checks none of their sources change.

Room section (MPD section 0): a 96-byte header of 24 u32 section lengths, sections follow in
order (0 geometry, 1 collision map, 2 section3, 3 doors, ..., 9 section A, ...).
- Doors (12 bytes): dest zone, dest room, u16 tile = x + z * collision-map width, ...
  `D_800F1AB0` is the door being taken, so its first two bytes are the room being entered.
- Section A (20 bytes): locks. s16 x, z, ...; byte 8 lock state (0 open, 1 key/sigil,
  2 magic, 3 locked until scripted); s16 keyOrSigil at 0x0C. Keys are only checked for in
  the inventory; sigils are consumed and set flag `0x1B6 + id`.
Linking a lock to a door by tile gives the key/sigil graph between rooms (e.g. MAP160's Bronze
lock at (13,6) is the door to zone 51 room 27). Story gating is in event scripts, not here.

Loot keys (ChaoticBrave's behaviour, `src/payload/keys.c`): random chest misc items and drops
come uniformly from the 116 valid misc ids including keys/sigils (about 27%); Chamomile never in
the 11 pre-Minotaur maps; Bronze/Silver/Iron follow his order rules (his variable names swap
Iron and Silver; the effective order is Bronze, Silver, Iron). His map-order dependence is
reproduced by replaying earlier rooms' draws from a story-ordered room table. His vanilla
key-chest exclusion compares against a never-updated variable (`ivm`), so his tool can overwrite
vanilla key chests; ours never touches vanilla sources.

## Abilities (SLUS_010.40 data, resident all session)

- `vs_main_actions` (`0x8004B9DC`, 0x34 bytes each: id, ?, type bits, cost, ..., name at +0x1C):
  Break Arts are 184-223 (cost = HP cost), risk-cost abilities are 22-53 (28 of them, Heavy Shot
  .. Phantom Shield), Lv.4 elemental spells are 109, 113, .. 133, enemy ultimates 134-140
  (Tornado .. Apocalypse). ChaoticBrave's "learn ultimate spells" records are the ultimate with
  bytes 2 and 4-11 (cost type, range, area) from the Lv.4 spell.
- `vs_main_artsPointsRequirements` (`0x8004FDE4`, u16[40]) and
  `vs_main_battleAbilitiesPointsRequirements` (`0x8004FE5C`, u16[22], increasing).
- BATTLE.PRG `0x80078B30` and `0x80078B48`: `li s0, 8`, the chain/defense input window in frames.
`src/payload/abilities.c` restores these from a snapshot and re-applies them at every battle
program start, so a vanilla save loaded later in the session gets vanilla tables.

## Zenith options (from ChaoticBrave's Zenith.cpp, decoded)

- Perfect Analyze: action 58 (Analyze) byte 0x15 `0xD1` -> `0x41` (hit function 6 -> 2).
- Friendly Manabreaker: `_getDefenseGemBuff` (BATTLE.PRG): `0x8007E6F8` nop -> `lbu t3,0x33(a0)`,
  `0x8007E704` `li t0,1` -> `slti t0,t3,1` (a Manabreaker gem only blocks when that byte is 0).
- Boss Warlock Grimoires: spawn 0 of MAP032/286/277/298/312 (roomKeys 0x0D04/0x3302/0x3211/
  0x330E/0x3501) gets Grimoire Zephyr/Teslae/Incendie/Terre/Glace as item2, displaced items
  shifting down (the original edits ids only, leaving e.g. a grimoire with count 3 or 0).
- Quiet enemies: Herakles (60) / Prostasia (63) removed from limb-2 action slots of Crimson
  Blades (zone 32 templates 3, 4, 12, 13, 21, 22) and Gremlins (zone 53 10-11, zone 30 4).
- Free teleport and repeat-kill drops were isolated from the standalone Zenith PPF (the
  PC release's `ZENITH_QOL/` files are vanilla copies, so there newDrops does nothing and
  free teleport is only the BATTLE poke). Most of Zenith's MENU0/BATTLE diff is its own
  rebalancing (damage formulas, spell menus) and is left out.
- Free teleport: BATTLE `0x800736DC` `subu v0,a0,v1` -> `subu v0,a0,zero` (MP - 0 after a
  teleport). In MENU0 (`0x80102800`), `_teleportMenu` caches `stats->currentMP` (0x1C) at
  `0x80106A0C` to grey out save points costing more (cost 15 + 4 per save point); Zenith
  caches `maxMP (0x1E) + 45` and nops the `_setMPCostDirect` cost preview at `0x80104168`.
  We patch MENU0 on disc to call `rando_teleportMenuMp` / `rando_teleportShowCost`, which
  do the vanilla thing unless the option is on.
- Repeat-kill drops: Zenith redirects the `nop; slt v0,v0,v1` after the roll in
  `_dropWeaponRand/Shield/Accessory/ArmorRand` into a routine with an 8-entry item-id
  history per kind (armor per category, byte 4) in memory past BATTLE.PRG's end. With 3+
  earlier rolls for the same id, rate += 2*seen (seen < 6) or (rate/4)*seen; rates < 4
  are untouched. Zenith's shift-when-full only ever shifts the weapon history (bug); ours
  (`rando_dropRoll` via the `drops.s` stubs) cycles each history, kept in resident RAM and
  cleared when BATTLE starts.

## Item ids (ITEMNAME.BIN, 24-byte entries)

| Range | Kind |
|---|---|
| `0x001`-`0x05A` | blades |
| `0x060`-`0x07E` | grips |
| `0x07F`-`0x08E` | shields |
| `0x08F`-`0x0CE` | armor: head, body, legs, arms (16 each) |
| `0x0DF`-`0x0FD` | accessories |
| `0x143`-`0x164` | consumables |
| `0x182`-`0x1C8` | grimoires (holes are "untitled") |
| `0x1CA`-`0x1D1` | keys (Bronze, Iron, Silver, Gold, Platinum, Steel, Crimson, Chest) |
| `0x1D2`-`0x1E9` | sigils |

## Randomizer config persistence

The seed/options live in the last 16 bytes of `vs_main_stateFlags` (`0x800619C8`, flag indices
0x430-0x43F) as u32 options2, u16 magic `0x5652`, u16 checksum, u32 seed, u32 options (option bits are listed
in `src/payload/rando.h`; bits 12-15 hold the Ashley build). The game copies the whole flag array into
every save, so Continue restores it with no save hooks. `_initGameData` (title) zeroes the flags
before New Game applies the config.

Nothing in vanilla uses those bytes (`tools/scan_flags.py`, run by `make test`): script opcodes
either build 10-bit flag numbers (max 0x3FF), write fixed flags, or (opcodes 0x1F, 0x52, 0xB2,
0xB6) take an unmasked u16 that, decoded across all 1173 scripts (EVT files, MPD script sections,
door scripts; code from +16 with the opcode length table at BATTLE.PRG `0x800E9C30`), never
exceeds 0x385. C code writes fixed flags, keys/sigils at 0x1B6+id (to 0x39F) and opened chests at
0x3C0+index. The only writers reaching 0x434 are the title's full reset and the ending's New Game+
reset (`for (i = 16; i < 1088; ...)` in ENDING.PRG), which is patched to stop at 0x430
(`0x8006B814`: `slti v0,v1,1088` -> `0x430`) so New Game+ keeps the seed; the title writes the
clear save before its own reset.

## Enemy equipment (battle overlay)

`src/battle/enemygear.c` ports the equipment part of `Enemies::mapIterate`. Template layout:
blade `0x34`, grip `0x64`, weapon gems `0x94`+0x30n (count = grip gemSlots `+0x15`), weapon
material/drop `0x124/0x125`, shield `0x140` (gems `0x170`, material/drop `0x200/0x201`),
accessory `0x204` (drop `0x234`), limbs `0x238`+0x5C n (hp u16, agility defense `+2`, chain
evasion `+3`, BEP `+5..7`, affinities `+8`, armor `+0x20`, material/drop `+0x50/0x51`). Records are
`vs_battle_uiEquipment`: STR/INT/AGL `+5`, cur/max DP `+8/+0xA`, cur/max PP `+0xC/+0xE`, damage
type/cost type/cost `+0x10..0x12`, range `+0x18`, BEP `+0x1D..0x1F`, classes `+0x20` (6),
affinities `+0x28` (7). Gem ids are `0x100|n`. Blade wepId (model) differs from the id for half
the blades, so new ids come from the vanilla pools. The PC code's quirks we do not copy are
listed at the top of the file. The second option word lives at flags `0x430-0x433`: `unk400`
(`0x400-0x43F`) is never referenced by game code, scripts write at most `0x385`, opened chests use
`0x3C0+index` (`chestsOpened[64]`).

## Chest and Ashley stat rolls

Chest stats (`src/battle/cheststats.c`) run in `rando_onMpdLoaded` after the chest contents are
chosen. The PC "Item Base Stats" option replaces each item with a random one and fills its real
stats from tables (our "Chest equipment" already picks real items), so only its random parts are
kept: DP 4100-32700/PP 100-250 and gem STR/INT/AGL. Balanced tiers come from three map lists in
`Chests.cpp` (`tools/gen_chesttiers.py`); per tier class/affinity split (+20/-5, +20/-6,
+20/-11, +30/-15), BEP max 7, 7, 20, 25, gem stats max 5, 6, 6, 15 (or -3..-1).
Ashley's class/affinity/BEP options (`Ashley.cpp`) use the first enemy group's ranges. His
starting inventory is set on the title screen, before the overlay exists, so New Game sets
`rando_newGameGear` and `rando_onAshleyInit` rolls the inventory once the overlay is loaded;
his body parts are rolled in the INITBTL template every time (it is reloaded from disc).

## Small options

- Blade/Fandango cost stat: `damageCostType` (blade `+0x10`) is `4 * costType + damageType`;
  vanilla cost type is 2. The options roll cost type 1-4 and keep the damage type.
- Ice Wolf (`Enemies.cpp`, inside its placement code): ZONE028 template 0 (Hellhound) limb 0
  action 1 `147` (fire breath) -> `159` (the Snow Dragon's ice breath, ZONE053), affinities
  Fire `+0x242` and Water `+0x244` swapped (45/-25). The PC code's spawn byte `+0x21` writes are
  placement bookkeeping (drop counts), not part of the effect.
- Ashley threshold: options2 bits 13-14 (0 medium 50-150, 1 low 75-125, 2 high 25-175), for all
  three of STR/INT/AGL.

## Open World and Mandatory Iron Maiden

`Rooms::openWorld` writes every key/sigil (0x1CA-0x1E9, count 1) over the first 32 starting misc
slots (TITLE.PRG `0xD224` = inventory misc); we fill empty slots instead, keeping the starting
items (misc slot indexes are fixed per slot, so only id and count are set).
`Rooms::mandyMaiden`: MAP171 (Great Cathedral entrance, room 0x2700) has two section-A lock
records (0x14 bytes: lock state `+8`, keyOrSigil `+0xC`); both become state 1 / Tigertail Sigil
0x1E6. MAP145 (room 0x1F00) spawn 0 (Kali) gets item/item2 = Gold Key 0x1CD / Platinum Key 0x1CE,
counts 1. Vanilla sources: Gold Key chest MAP241, Platinum Key drop MAP350, Tigertail Sigil drop
MAP404 (Asura, Iron Maiden); Tigertail lock MAP130, Platinum locks MAP376/377 (Iron Maiden).
Section A sits after sections 1-9 of the room section, whose 24 u32 lengths start the section.

## Compatibility with other mods

- Rood Reverse QoL mod v3.1 (default variant, `reference/*.xdelta`, apply to vanilla first, then
  our PPF): it extends BATTLE.PRG with code at `0x800F5924`-`0x800F5FBB` and variables up to
  `0x800F5FFF`, which is why our battle overlay loads at `0x800F6000`. None of its edits touch our
  hook sites or runtime patches, and it never touches SLUS_010.40. Our only TITLE.PRG edit (the
  New Game hook) shared a sector with a QoL edit, so it is now installed in RAM by
  `rando_beforeTitle` after `_loadTitlePrg` (which loads synchronously), and TITLE.PRG stays
  vanilla on disc. The two patches now change disjoint sectors, and every changed sector of the
  combined image has valid EDC/ECC. Emulator-verified: QoL code intact, overlay loaded, Wine
  Cellar enemies randomized. The xdelta needs DJW secondary decompression; build the xdelta3 CLI from
  the source bundled in the `xdelta3` pip package if no system xdelta3 is available.
- Zenith 1.94: the standard PPF on top does NOT work (it boots a relocated vanilla exe,
  dropping Zenith's exe changes, and its MENU0 teleport words collide). `make zenith` builds the
  Zenith edition (`-DRANDO_ZENITH_EDITION`, `build/zenith/`) against two bases made in
  `build/bases/`: Zenith 1.94 (SHA-1 `02b764a6...`) and Zenith 1.94 + QoL v3.1 for Zenith
  (`78ba2218...`). `build.py --base` identifies the base by SHA-1, and the Zenith edition skips
  the MENU0 hooks/words. Zenith keeps SLUS's header and size, every other hook site and word
  patch matches, our sectors are free, no Zenith/QoL code refers to our RAM, and every game
  function we call is byte-identical. Zenith moves `vs_main_actions` to `0x8004B9E0` with a
  different record layout (8 of 10 MENU0 refs, 60 of 63 BATTLE refs use the new base), so the
  Zenith edition never touches action records: Break Arts randomizes only the learning kills
  (`0x8004FDE4`, same place and format), Risk Costs and Ultimate Spells are dropped, and our own
  Zenith options are compiled out (the real Zenith has them). The flag scan on both bases finds
  nothing above `0x385` (5 decode problems, all in MAP416/417/506, which Zenith stubs out).
  Zenith's PPF has a FILE_ID.DIZ trailer with a 2-byte length; `ppf.apply` finds it by marker.
  Emulator-verified on both: Zenith edition menu, New Game, Zenith's 6-frame timing and action
  table intact (it changes by 10 bytes during the game's own New Game setup, before our code),
  Break Art kills randomized, overlay loaded, Wine Cellar enemies randomized.

## Boot menu

The options menu (`src/menu`) is a separate binary linked at `0x80068800` and stored at LBA 400
(after the relocated exe). On first boot the resident payload loads it there through the game's
CD queue right after `_sysReinit`, before `_loadTitlePrg` puts TITLE.PRG over it, flushes the
I-cache and calls it. Settings wait in `rando_pending` until New Game. It draws 320x240
double-buffered at (0,0)/(0,256), 38 columns x 24 lines of 9 px. Text is drawn as tinted 8x8
sprites (GP0 0x74) straight from the debug font texture `FntLoad` puts at (960,256): 4-bit,
16 glyphs per row from ' ' to '_' (upper case only), CLUT at (960,384). That gives per-string
colours (FntPrint has one). The background is a Gouraud quad.

Layout follows MottZilla's SotN Integrated Rando (screenshots taken by patching a scratch copy of
the user's SotN image and driving DuckStation with `hyprctl dispatch sendkeystate`): main screen
(Enter Seed # / Randomize / Select Preset / Select Options / Load Seed MemCard / Launch Vanilla
Game, seed and green preset on the right, help below, hints bottom right, build date bottom left);
a seed keypad screen (Tri random, X types a digit, O deletes/back, Start begins; the 8th digit
starts); an options screen (sections with green headers, "x" before enabled options, "more.."
when scrolled, L1/R1 jump 10, preset and the highlighted option's help on the right). Randomize
rolls a fresh seed at press time (the frame counter is nearly constant at boot, so press timing
is the entropy). Starting a randomized game asks whether to save the seed and options to the
card first; Load Seed MemCard reads them back and starts immediately.
Settings file: `bu00:BASLUS-01040RNDO` (memory card slot 1, one block), written with the BIOS
`open`/`write`/`read`/`close`/`erase` linked into SLUS_010.40 in blocking mode (the game starts
the card driver at boot). Standard save header (title "VS RANDOMIZER SETTINGS", one 16x16 icon),
then magic `SRND`, version (3: u32 options + options2; versions 1-2 still load), seed,
options, checksum. The game's save screen only parses its
`BASLUS-01040VAGn` files (`_memcardFileNumberFromFilename`) and counts others as used blocks.

In game, `vs_battle_renderTextRaw(text, y << 16 | x, NULL)` (BATTLE.PRG `0x800C6828`) draws ASCII in
the menu font into the current frame: upper case, digits and punctuation only (lower case has
zero width). Vagrant Story has no pause screen (Start is first-person view / skip, Select is
zoom); the main menu (Triangle) is where the seed is shown.

The title screen (for reference) uses a 512x480 interlaced display at VRAM (512,16), set once by
`_initTitleScreen`.
