# Vagrant Story Integrated Randomizer

> **Note:** This project was developed with the help of AI tools, under human direction and
> play-testing. Mentioning it up front so anyone who prefers to avoid AI-assisted projects
> knows before diving in.

An on-disc randomizer for Vagrant Story (USA), distributed as a PPF patch, in the style of
MottZilla's SotN Integrated Rando: the seed and options are chosen in-game and randomization
runs on the console. Randomizer logic is ported from ChaoticBrave's
[VagrantStoryRandomizer](https://github.com/ChaoticBrave/VagrantStoryRandomizer).
Addresses and structs come from [Rood Reverse](https://github.com/ser-pounce/rood-reverse) (CC0).


## Using the randomizer

Apply the PPF to a vanilla Vagrant Story (USA) image (or one patched with the Rood Reverse QoL
mod, see [Using with other mods](#using-with-other-mods)) and boot it. The randomizer menu appears
before the title screen. X confirms and O goes back.

| Main menu | What it does |
|---|---|
| Enter Seed # | Type an 8-digit seed on a keypad to replay or share a run. The game starts after the 8th digit (or press Start). Triangle fills in a random seed. |
| Randomize | Starts a new randomized game with a fresh random seed. |
| Select Preset | Cycles Standard / Chaos / Vanilla (Left/Right or X). Changing any option shows "Custom". |
| Select Options | Opens the full option list (below). X toggles, Left/Right picks values, L1/R1 jump 10 rows. |
| Load Seed MemCard | Starts a game with the seed and options saved on memory card slot 1. |
| Launch Vanilla Game | Plays the unmodified game. |

Before a randomized game starts, the menu offers to save the seed and options to memory card
slot 1, so the same run can be loaded later or on another console. The seed and options are also
stored in your game saves (Continue and New Game+ keep them), and the in-game main menu
(Triangle) shows the seed and preset.

### Presets

- **Standard**: everything in the Chests, Enemies, Ashley and Abilities sections except Ashley Build,
  Ultimate Spells, Ability Timing and Loot Keys/Sigils, with every Balance option on.
- **Chaos**: the same, plus Ultimate Spells, Ability Timing and Loot Keys/Sigils, with nothing
  balanced.
- **Vanilla**: no randomization.

Ashley Build, the Zenith options, Open World and Iron Maiden Required are never part of a preset;
turn them on yourself.

### Options

**Chests**

| Option | What it does |
|---|---|
| Chest Items | Shuffles the consumables and grimoires found in chests. Keys and sigils stay where they are. |
| Chest Equipment | Weapons, blades, grips, shields, armor, accessories and gems in chests become random real items of the same kind. |
| Chest Gear Stats | Random DP and PP for chest weapons, shields and armor, and random STR/INT/AGL for chest gems. |
| Chest Gear Classes | Randomizes the class values (Human, Beast, Undead...) of chest equipment. |
| Chest Gear Affinity | Randomizes the elemental affinity values of chest equipment. |
| Chest Gear BEP | Randomizes the blunt, edged and piercing values of chest grips, shields and armor. |
| Blade Cost Stat | Chest blades cost a random stat when used. |
| Balance Item Stats | Keeps the chest gear stat rolls small early in the game and lets them widen later (affects the chest gear options above). |
| Balance Chests | Vanilla-like item amounts, and chest equipment materials that fit the point in the story. |

**Enemies**

| Option | What it does |
|---|---|
| Enemy Stats | Randomizes enemy HP, MP, STR, INT, AGL and movement speed. |
| Balance Enemy Stats | Enemy stats grow with the story instead of reaching 999/255 anywhere. Also narrows the enemy gear options below. |
| Enemy Drops | Randomizes each enemy's chance-based drop and its drop rate. Guaranteed drops (including keys and sigils) are kept. |
| Enemy Gear Stats | Changes which weapon, shield, armor and accessory enemies carry, and their STR/INT/AGL, DP/PP, material and drop rate. Enemies drop equipment much more often with this on. |
| Enemy Gear Classes | Randomizes the class values of enemy equipment. |
| Enemy Gear Affinity | Randomizes the affinity values of enemy equipment. |
| Enemy Gear BEP | Randomizes the blunt, edged and piercing values of enemy equipment. |
| Enemy Body Parts | Randomizes the HP and defenses of each enemy body part (and their affinities and BEP when those options are on). |
| Ice Wolf | The City Walls Hellhounds breathe ice instead of fire, with their fire and water affinities swapped. |

**Ashley**

| Option | What it does |
|---|---|
| Ashley Build | Starts Ashley with a setup for one weapon type (Dagger, Great Sword, Axe, Mace, Great Axe, Great Mace, Polearm, Staff, Crossbow, or Random): stats, blade, grip and items. Replaces Ashley Stats, Starting Weapon and Starting Items. |
| Ashley Stats | Randomizes Ashley's starting HP, MP, STR, INT, AGL and speed. |
| Starting Weapon | Replaces the Fandango's blade with a random one and a matching grip. |
| Starting Armor | Replaces each starting armor piece and the accessory with a random one for the same slot. |
| Starting Items | Replaces the starting items with random consumables and grimoires. |
| Fandango Cost Stat | The starting blade costs a random stat when used. |
| Ashley Gear Classes | Randomizes the class values of Ashley's starting equipment. |
| Ashley Affinity | Randomizes the affinities of Ashley's starting equipment and his body parts. |
| Ashley BEP | Randomizes the blunt, edged and piercing values of Ashley's starting equipment and body parts. |
| Balance Ashley | Keeps Ashley's stats near vanilla and limits his starting gear to Bronze or Iron. |
| Ashley Threshold | With Balance Ashley, sets his STR/INT/AGL range: Low 75-125, Medium 50-150 (default), High 25-175. |

**Abilities**

| Option | What it does |
|---|---|
| Break Arts | Randomizes the HP cost of each Break Art and the kills needed to learn it. |
| Balance Break Arts | Keeps Break Art costs (25-75) and kills (10-485) within the vanilla ranges. |
| Ability Learning | Randomizes the chains needed to learn each battle ability, never more than vanilla. |
| Ability Risk Costs | Randomizes the risk cost of each chain and defense ability (1-6). |
| Ultimate Spells | The Lv.4 elemental spells become the enemies' ultimate spells. |
| Ability Timing | Randomizes how many frames you have to time chain and defense abilities (8-30). |

**Keys and progress**

| Option | What it does |
|---|---|
| Open World | Ashley starts with every key and sigil, so every locked door can be opened from the start. |
| Iron Maiden Required | Great Cathedral is locked with the Tigertail Sigil, which Asura drops in the Iron Maiden; Kali drops the Gold and Platinum Keys to get you there. |
| Loot Keys/Sigils | Random chest items and enemy drops can also be keys or sigils, as in the PC randomizer. The vanilla ones stay where they are. |

**Zenith** (individual features of the_eyes' Vagrant Story Zenith mod; standard edition only, see [Zenith edition](#vagrant-story-zenith-zenith-edition))

| Option | What it does |
|---|---|
| Perfect Analyze | Analyze never misses. |
| Kind Manabreaker | A Manabreaker gem no longer blocks your own spells. |
| Boss Grimoires | Five bosses drop a Warlock Grimoire (Zephyr, Teslae, Incendie, Terre, Glace). |
| Quiet Foes | Gremlins and Crimson Blades stop casting Herakles and Prostasia. |
| Free Teleport | Teleporting costs no MP; save points within max MP + 45 can be reached. |
| Kill Drops | Killing enemies with the same equipment again and again raises its drop rate. |

## Using with other mods

### Rood Reverse QoL mod

The randomizer works together with the [Rood Reverse QoL mod](https://github.com/eremtsov/rood-reverse/blob/qol-mod/MOD.md)
(tested with v3.1, default variant). Patch in this order:

1. Apply the QoL mod's xdelta to a clean Vagrant Story (USA) image.
2. Apply this randomizer's PPF to the result.

The two patches change separate parts of the disc, so the combined image has valid sector
checksums, and the randomizer's code stays clear of the RAM the QoL mod uses. QoL's
confirm/cancel swap does not apply to the randomizer menu, which always uses X to confirm.

The QoL xdelta uses xdelta3's secondary compression, so use the official `xdelta3` tool (or a
patcher built on it, such as Delta Patcher) to apply it.

### Vagrant Story Zenith (Zenith edition)

The standard PPF does not work on top of the Vagrant Story Zenith mod. Use the **Zenith
edition** instead, which comes as two PPFs:

| Patch | Apply to |
|---|---|
| `VSIntegratedRando-Zenith.ppf` | Vagrant Story Zenith 1.94 (vanilla + `zenith_194.ppf`) |
| `VSIntegratedRando-Zenith-QoL.ppf` | Zenith 1.94 + the Rood Reverse QoL mod v3.1 for Zenith 1.94 |

For Zenith, the QoL mod and the randomizer together, patch in this order:

1. Apply `zenith_194.ppf` to a clean Vagrant Story (USA) image.
2. Apply the QoL mod's Zenith xdelta (`Vagrant.Story.USA.-.QoL.mod.v3.1.for.Zenith.1.94.xdelta`).
3. Apply `VSIntegratedRando-Zenith-QoL.ppf`.

Each PPF only matches the exact image it was made for (other Zenith versions won't work), and
the order matters.

In the Zenith edition the menu title reads "VAGRANT STORY RANDOMIZER ZENITH", and a few
options differ from the standard edition:

- The **Zenith** section is gone: Zenith itself provides those features.
- **Break Arts** only randomizes the kills needed to learn each Break Art (Zenith rearranges
  the data that holds their HP costs), and **Balance Break Arts** only keeps the kills in range.
- **Ability Risk Costs** and **Ultimate Spells** are not available, for the same reason.
- Everything else (chests, enemies, Ashley, keys and progress, Ability Learning and Ability
  Timing) works as in the standard edition, on top of Zenith's own changes.

## Credits

- **ChaoticBrave**: the original [Vagrant Story Randomizer](https://github.com/ChaoticBrave/VagrantStoryRandomizer),
  whose rules this ports.
- **MottZilla**: the [SotN Integrated Randomizer](https://github.com/MottZilla/SotN_Integrated_Rando),
  the model for this project.
- **ser-pounce and contributors**: the [Rood Reverse](https://github.com/ser-pounce/rood-reverse)
  decompilation (CC0), the source of the game addresses and structures used here.
- **eremtsov**: the [Rood Reverse QoL mod](https://github.com/eremtsov/rood-reverse/blob/qol-mod/MOD.md).
- **the_eyes**: [Vagrant Story Zenith](https://vszenith.wordpress.com/).

## Building

```sh
tools/build_toolchain.sh   # once: mipsel-none-elf GCC + binutils + armips into ~/.local/opt/psx
make                       # -> build/VSIntegratedRando.ppf (and the patched .bin)
make zenith                # -> build/zenith/VSIntegratedRando-Zenith.ppf and -Zenith-QoL.ppf
make release               # both editions, versioned, with SHA1SUMS, in build/release/
make test                  # host-side randomizer tests
```

Requires a vanilla `reference/Vagrant Story (USA).bin` (SHA-1
`38c63db6c49a06b91d87cd5a4e31b1ec68fe212c`). `make zenith` also needs `reference/zenith_194.ppf`
and the QoL mod's Zenith xdelta, from which it builds the two base images in `build/bases/`.
Applying that xdelta needs an `xdelta3` built with LZMA support; point `XDELTA3=` at it if the
one on your PATH lacks it.

## Layout

- `src/payload/` — C/asm code resident in RAM at `0x80062000` (appended to `SLUS_010.40`)
- `src/menu/` — boot options menu, a separate binary loaded at boot (see `docs/NOTES.md`)
- `src/battle/` — battle overlay, loaded into free RAM each time the battle program starts (equipment stats, Open World, Iron Maiden)
- `tools/` — disc image I/O (`psxdisc.py`), PPF (`ppf.py`), text codec, image builder (`build.py`)
- `docs/NOTES.md` — reverse-engineering notes: RAM map, hook points, file formats
