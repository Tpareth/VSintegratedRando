# Handoff — 2026-10-28 (updated after ability randomization)

## Status
- **Builds and runs:** `make` produces `build/VSIntegratedRando.ppf` plus a patched `.bin`/`.cue`. `make test` passes: the host randomizer tests and the script flag scan.
- **Toolchain:** `~/.local/opt/psx/bin` (mipsel-none-elf GCC 15.2, armips). Rebuild it with `tools/build_toolchain.sh`.
- **Emulator testing:** DuckStation with its GDB stub, driven by `tools/emu.py` and `tools/gdbrsp.py`. It needs an isolated `XDG_CONFIG_HOME` containing `duckstation/settings.ini` with `EnableGDBServer = true` and an NTSC-U BIOS. The previous one was in the session scratchpad, which is gone. Recreate it by copying `~/.config/duckstation/settings.ini` and the BIOS `~/Documents/Windows Backup/SOTN image/scph7003.bin` into a new directory, and set `SearchDirectory` to an absolute path. See the memory note "emulator-testing".
- **Test scripts:** `tools/smoke_test.py`, `menu_test.py`, `ashley_test.py`, `enemy_test.py`, `chest_test.py` and `pause_test.py`. Each takes `--config-home <dir>`.
- **Resident payload:** 24.8 KB of 26 KB used (`0x80062000`-`0x80068800`). The boot menu is a separate overlay at LBA 400.
- **Battle overlay** (`src/battle`, LBA 420): loaded at every battle-program start (`rando_onAshleyInit`) into free RAM `0x800F6000`-`0x800F9800` (after BATTLE.PRG, past the code the QoL and Zenith mods append to it; verified untouched during play), max 7 sectors (14 KB), 7.3 KB used (built with -Os). Put new battle-time features here, not in the resident payload.
- **Config:** 16 bytes at flags `0x430` (`options2` added below the old 12 bytes; old saves still valid). Card file v3.

## Done
- **Boot options menu (laid out after the SotN Integrated Rando, user-approved 2026-10-29):** main screen Enter Seed # / Randomize / Select Preset / Select Options / Load Seed MemCard / Launch Vanilla Game; seed keypad screen; grouped, scrolling options list with help on the right; save-to-card prompt before a randomized start (`bu00:BASLUS-01040RNDO`). X confirms, O goes back.
- **Seed and preset display** on the in-game main menu (Triangle).
- **Chests:** consumables and grimoires, real-item equipment (weapon+grip, blade, grip, shield, armor, accessory, gem), "Balance chests" quantities and material tier by story area, and optional loot keys/sigils (ChaoticBrave's Bronze→Silver→Iron rules, replayed in story order).
- **Enemies:** base stats (balanced by zone group) and the chance-based drop (item3).
- **Ashley:** stats, starting weapon, armor and items; Balance Ashley; 9 builds plus Random.
- **Abilities:** Break Art costs and learning points (plus Balance), ability learning points, risk costs, learn ultimate spells, and ability time frames.
- **Zenith options:** Perfect Analyze, Friendly Manabreaker, boss Warlock Grimoires, and quiet Gremlins/Crimson Blades, free teleport, and repeat-kill drops (the last two isolated from the standalone Zenith PPF; patches verified in RAM, host-tested drop math; teleport/drops not yet play-tested).
- **Equipment stat rolling (group 1):** enemy gear base/class/affinity/BEP and body parts (`src/battle/enemygear.c`), chest gear DP/PP, gem stats, class/affinity/BEP with Balance item stats tiers (`cheststats.c`, tiers from `tools/gen_chesttiers.py`), Ashley's gear and body class/affinity/BEP (end of `enemygear.c`; starting gear rolled at the first battle start after New Game via `rando_newGameGear`). Emulator-verified; all in the Standard and Chaos presets.
- **Small items:** Ashley stat threshold (Low/Medium/High, options2 bits 13-14), blade and Fandango cost stat (cost type 1-4, damage type kept), Ice Wolf (zone 28 Hellhound: action 147 -> 159, fire/water affinities swapped). Host-tested; costs emulator-verified; Ice Wolf not seen in play yet (needs City Walls).
- **Open World / Mandatory Iron Maiden** (`src/battle/progression.c`, options2 bits 18-19, not in presets): Open World adds all 32 keys/sigils (0x1CA-0x1E9) to empty starting misc slots (emulator-verified). Iron Maiden locks both MAP171 (room 0x2700) doors with the Tigertail Sigil and gives Kali (MAP145, room 0x1F00, spawn 0) the Gold/Platinum Keys; host test matches the PC's exact bytes. The route (Kali keys -> Iron Maiden -> Asura's Tigertail Sigil, MAP404 -> Great Cathedral) is the PC design and has not been played through.
- **Zenith edition** (`make zenith`): PPFs for Zenith 1.94 and for Zenith 1.94 + QoL for Zenith (the user's goal: all three together). No Zenith options, no action-record edits (Zenith rearranges that table); see NOTES.md "Compatibility with other mods". Emulator-verified on both bases. Building the Zenith+QoL base needs an xdelta3 with LZMA (`XDELTA3=`).
- **Config persistence:** stored in the save flags at `0x434`; the scan proves no script writes there; New Game+ keeps it via an ENDING.PRG patch.

## Next (user's priority order)
1. **Bigger items (user does not care much about these):** music (area, boss, restore cut), area progression, enemy placement, model swaps, Inconsistent Gear. These need data loaded on demand from disc, the same way the menu is (resident RAM is getting tight).

## User testing (scratchpad.md, DuckStation)
Confirmed: a new randomized game, a randomized starting chest, randomized enemies, save and load
(the seed persists), and seed save/load across an emulator restart. Requested and done: the save
prompt on start and load-and-start.

## Untested in real play
- New Game+ (needs a clear save).
- The ability options (ultimate spells, timing) in play.
- Real hardware, including the memory card.

## Where things are
- `src/payload/`: resident code. Hooks are in `hooks.c`, `boot.c` and `seedinfo.c`; features in `chests.c`, `enemies.c`, `ashley.c`, `builds.c`, `keys.c` and `gear.c`.
- `src/menu/`: boot menu overlay (`menu.c`, `card.c`).
- `tools/build.py`: hook list (`EXE_HOOKS`, `INITBTL_HOOKS`, `BATTLE_HOOKS`, `MENU0_HOOKS`, `WORD_PATCHES`; the TITLE.PRG New Game hook is installed in RAM by `boot.c`). Generators: `gen_equipment.py` and `gen_keyrooms.py`.
- `docs/NOTES.md`: RE notes covering the RAM map, hook table, file formats, item IDs, and the keys/locks/door data.
- References (the session scratchpad clones are gone; re-clone if needed):
  - Rood Reverse decomp (CC0): https://github.com/ser-pounce/rood-reverse
  - ChaoticBrave PC rando: https://github.com/ChaoticBrave/VagrantStoryRandomizer
