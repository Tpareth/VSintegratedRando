TOOLCHAIN ?= $(HOME)/.local/opt/psx/bin
PREFIX    := $(TOOLCHAIN)/mipsel-none-elf-
CC        := $(PREFIX)gcc
OBJCOPY   := $(PREFIX)objcopy
NM        := $(PREFIX)nm

VANILLA   ?= reference/Vagrant Story (USA).bin
VERSION   := 0.1b

# Zenith edition (`make zenith`): the same randomizer built on top of Vagrant
# Story Zenith 1.94, as two PPFs: one for Zenith, one for Zenith + the Rood
# Reverse QoL mod for Zenith. The base images are made from the files below
# (the QoL xdelta needs an xdelta3 with LZMA support).
EDITION           ?= vanilla
XDELTA3           ?= xdelta3
ZENITH_PPF        ?= reference/zenith_194.ppf
QOL_ZENITH_XDELTA ?= reference/Vagrant.Story.USA.-.QoL.mod.v3.1.for.Zenith.1.94.xdelta
ZENITH_BASE       ?= build/bases/zenith.bin
ZENITH_QOL_BASE   ?= build/bases/zenith-qol.bin

ifeq ($(EDITION),zenith)
BUILD          := build/zenith
EDITION_CFLAGS := -DRANDO_ZENITH_EDITION
PPFS           := $(BUILD)/VSIntegratedRando-Zenith.ppf $(BUILD)/VSIntegratedRando-Zenith-QoL.ppf
else
BUILD          := build
PPFS           := $(BUILD)/VSIntegratedRando.ppf
endif
PAYLOAD   := src/payload
MENU      := src/menu
OVERLAY   := src/battle

CFLAGS := -O2 -G0 -march=r3000 -mabi=32 -mno-abicalls -fno-pic -msoft-float \
          -ffreestanding -fno-builtin -nostdlib -fno-common -Wall -Wextra -Werror \
          -Wno-unused-parameter -Wno-missing-field-initializers -fwrapv -std=gnu11 $(EDITION_CFLAGS) -DRANDO_VERSION='"$(VERSION)"'
ASFLAGS := -march=r3000 -mabi=32 -mno-abicalls -fno-pic -G0

SRCS := $(wildcard $(PAYLOAD)/*.c)
ASMS := $(wildcard $(PAYLOAD)/*.s)
OBJS := $(patsubst $(PAYLOAD)/%.c,$(BUILD)/payload/%.o,$(SRCS)) $(patsubst $(PAYLOAD)/%.s,$(BUILD)/payload/%.o,$(ASMS)) \
        $(BUILD)/payload/equipment.o $(BUILD)/payload/keyrooms.o

.PHONY: all clean test smoke zenith release
all: $(PPFS)

zenith:
	$(MAKE) EDITION=zenith

# Both editions, renamed with the version, in build/release/.
release:
	$(MAKE)
	$(MAKE) EDITION=zenith
	mkdir -p build/release
	cp build/VSIntegratedRando.ppf build/release/VSIntegratedRando-v$(VERSION).ppf
	cp build/zenith/VSIntegratedRando-Zenith.ppf build/release/VSIntegratedRando-Zenith-v$(VERSION).ppf
	cp build/zenith/VSIntegratedRando-Zenith-QoL.ppf build/release/VSIntegratedRando-Zenith-QoL-v$(VERSION).ppf
	cd build/release && sha1sum *.ppf > SHA1SUMS

$(BUILD)/payload/%.o: $(PAYLOAD)/%.c $(wildcard $(PAYLOAD)/*.h) Makefile | $(BUILD)/payload
	$(CC) $(CFLAGS) -c $< -o $@

# Equipment pools generated from the vanilla disc.
$(BUILD)/gen/equipment.c: tools/gen_equipment.py tools/psxdisc.py
	python3 -I tools/gen_equipment.py --vanilla "$(VANILLA)" --out $@

$(BUILD)/payload/equipment.o: $(BUILD)/gen/equipment.c $(PAYLOAD)/equipment.h | $(BUILD)/payload
	$(CC) $(CFLAGS) -I $(PAYLOAD) -c $< -o $@

$(BUILD)/gen/keyrooms.c: tools/gen_keyrooms.py tools/psxdisc.py
	python3 -I tools/gen_keyrooms.py --vanilla "$(VANILLA)" --out $@

$(BUILD)/payload/keyrooms.o: $(BUILD)/gen/keyrooms.c $(PAYLOAD)/keys.h | $(BUILD)/payload
	$(CC) $(CFLAGS) -I $(PAYLOAD) -c $< -o $@

$(BUILD)/payload/%.o: $(PAYLOAD)/%.s | $(BUILD)/payload
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/payload.elf: $(OBJS) $(PAYLOAD)/link.ld $(PAYLOAD)/game.ld
	$(CC) $(CFLAGS) -T $(PAYLOAD)/link.ld -L $(PAYLOAD) -Wl,-Map,$(BUILD)/payload.map $(OBJS) -lgcc -o $@

$(BUILD)/payload.bin: $(BUILD)/payload.elf
	$(OBJCOPY) -O binary -R .bss $< $@

$(BUILD)/payload.sym: $(BUILD)/payload.elf
	$(NM) $< > $@

# Boot menu overlay, linked against the resident payload's symbols.
MENU_SRCS := $(wildcard $(MENU)/*.c)
MENU_OBJS := $(patsubst $(MENU)/%.c,$(BUILD)/menu/%.o,$(MENU_SRCS))

$(BUILD)/menu/%.o: $(MENU)/%.c $(wildcard $(MENU)/*.h) $(wildcard $(PAYLOAD)/*.h) Makefile | $(BUILD)/menu
	$(CC) $(CFLAGS) -I $(PAYLOAD) -c $< -o $@

# Payload symbols as linker-script assignments for the overlay (game symbols
# come from game.ld itself).
$(BUILD)/payload_syms.ld: $(BUILD)/payload.sym
	awk '$$2 ~ /^[TDBR]$$/ { printf "%s = 0x%s;\n", $$3, $$1 }' $< > $@

$(BUILD)/menu.elf: $(MENU_OBJS) $(MENU)/link.ld $(PAYLOAD)/game.ld $(BUILD)/payload_syms.ld
	$(CC) $(CFLAGS) -T $(MENU)/link.ld -L $(PAYLOAD) -L $(BUILD) \
		-Wl,-Map,$(BUILD)/menu.map $(MENU_OBJS) -lgcc -o $@

$(BUILD)/menu.bin: $(BUILD)/menu.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/menu.sym: $(BUILD)/menu.elf
	$(NM) $< > $@

# Battle overlay, likewise linked against the resident payload.
OVERLAY_SRCS := $(wildcard $(OVERLAY)/*.c)
OVERLAY_OBJS := $(patsubst $(OVERLAY)/%.c,$(BUILD)/battle/%.o,$(OVERLAY_SRCS)) $(BUILD)/battle/chesttiers.o

# Optimised for size: the overlay's space is fixed and its code runs once per load.
$(BUILD)/battle/%.o: $(OVERLAY)/%.c $(wildcard $(OVERLAY)/*.h) $(wildcard $(PAYLOAD)/*.h) Makefile | $(BUILD)/battle
	$(CC) $(CFLAGS) -Os -I $(PAYLOAD) -c $< -o $@

$(BUILD)/gen/chesttiers.c: tools/gen_chesttiers.py tools/gen_keyrooms.py tools/psxdisc.py
	python3 -I tools/gen_chesttiers.py --vanilla "$(VANILLA)" --out $@

$(BUILD)/battle/chesttiers.o: $(BUILD)/gen/chesttiers.c $(PAYLOAD)/rando.h | $(BUILD)/battle
	$(CC) $(CFLAGS) -I $(PAYLOAD) -c $< -o $@

$(BUILD)/battle.elf: $(OVERLAY_OBJS) $(OVERLAY)/link.ld $(PAYLOAD)/game.ld $(BUILD)/payload_syms.ld
	$(CC) $(CFLAGS) -T $(OVERLAY)/link.ld -L $(PAYLOAD) -L $(BUILD) \
		-Wl,-Map,$(BUILD)/battle.map $(OVERLAY_OBJS) -lgcc -o $@

$(BUILD)/battle.bin: $(BUILD)/battle.elf
	$(OBJCOPY) -O binary $< $@

PATCH_DEPS := $(BUILD)/payload.bin $(BUILD)/payload.sym $(BUILD)/menu.bin $(BUILD)/menu.sym $(BUILD)/battle.bin \
              tools/build.py tools/psxdisc.py tools/ppf.py
# build.py BASE EDITION OUT-BIN OUT-PPF
build_patch = python3 -I tools/build.py --base "$(1)" --edition $(2) --payload $(BUILD)/payload.bin \
	--symbols $(BUILD)/payload.sym --menu $(BUILD)/menu.bin --menu-symbols $(BUILD)/menu.sym \
	--overlay $(BUILD)/battle.bin --out-bin $(3) --out-ppf $(4) --version $(VERSION) $(BUILD_ARGS)

$(BUILD)/VSIntegratedRando.ppf: $(PATCH_DEPS)
	$(call build_patch,$(VANILLA),vanilla,$(BUILD)/VSIntegratedRando.bin,$@)

$(BUILD)/VSIntegratedRando-Zenith.ppf: $(PATCH_DEPS) $(ZENITH_BASE)
	$(call build_patch,$(ZENITH_BASE),zenith,$(BUILD)/VSIntegratedRando-Zenith.bin,$@)

$(BUILD)/VSIntegratedRando-Zenith-QoL.ppf: $(PATCH_DEPS) $(ZENITH_QOL_BASE)
	$(call build_patch,$(ZENITH_QOL_BASE),zenith,$(BUILD)/VSIntegratedRando-Zenith-QoL.bin,$@)

build/bases/zenith.bin: $(ZENITH_PPF) tools/ppf.py
	mkdir -p $(dir $@)
	python3 -I tools/ppf.py "$(VANILLA)" $(ZENITH_PPF) $@

build/bases/zenith-qol.bin: $(ZENITH_BASE) $(QOL_ZENITH_XDELTA)
	mkdir -p $(dir $@)
	$(XDELTA3) -d -f -s $(ZENITH_BASE) "$(QOL_ZENITH_XDELTA)" $@

$(BUILD)/payload $(BUILD)/menu $(BUILD)/battle:
	mkdir -p $@

test:
	python3 -I tests/test_rando.py
	python3 -I tools/scan_flags.py --vanilla "$(VANILLA)"

# Boots the image in DuckStation (window opens) and reports MPD hook hits.
# EMU_CONFIG_HOME: XDG config dir with duckstation/settings.ini (GDB server on, NTSC-U BIOS).
smoke: $(BUILD)/VSIntegratedRando.ppf
	python3 -I tools/smoke_test.py --config-home "$(EMU_CONFIG_HOME)" --loads 3

clean:
	rm -rf $(BUILD)
