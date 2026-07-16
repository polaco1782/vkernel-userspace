# vkernel - UEFI Microkernel
# Copyright (C) 2026 vkernel authors
#
# userspace/Makefile - Aggregated userspace build

ROOT_DIR ?= $(abspath ..)
ROOT_BUILD_DIR ?= $(abspath $(ROOT_DIR)/build)
SYMBOLS_DIR := $(ROOT_BUILD_DIR)/symbols
USERSPACE_SYMBOLS_DIR := $(SYMBOLS_DIR)/userspace
USERSPACE_DIR := $(CURDIR)

include runtime_config.mk
include cxx_runtime_config.mk

include artifacts.mk

LIBC_DIR := libc
SYSROOT_DIR := $(USERSPACE_ACTIVE_SYSROOT)

HELLO_VBIN := hello/hello.vbin
TLS_SMOKE_VBIN := tls_smoke/tls_smoke.vbin
RAYTRACER_VBIN := raytracer/raytracer.vbin
SHELL_VBIN := shell/shell.vbin
DOOM_VBIN := doom/doom.vbin
QUAKE_VBIN := quake/quake.vbin
DUKE3D_VBIN := duke3d/duke3d.vbin
MODPLAY_VBIN := MODPlay/modplay.vbin
CLOWNMDEMU_VBIN := clownmdemu/clownmdemu.vbin
MINIMP3_VBIN := minimp3/minimp3.vbin
ROTOZOOM_VBIN := rotozoom/rotozoom.vbin
VKGUI_VBIN := vkgui/vkgui.vbin
SR_CUBE_VBIN := sr_cube/sr_cube.vbin
VKOBJ_VBIN := vkobj/vkobj.vbin
VNES_VBIN := vnes/vnes.vbin
SNES9X_VBIN := snes9x/snes9x.vbin
VSPCPLAY_VBIN := vspcplay/vspcplay.vbin

USERSPACE_BINARIES := $(USERSPACE_BINARY_RELATIVE)

CROSS_PREFIX ?= x86_64-redhat-linux-
NM ?= nm

symbol_map_target = $(USERSPACE_SYMBOLS_DIR)/$(1).map
line_map_target = $(USERSPACE_SYMBOLS_DIR)/$(1).lines
USERSPACE_SYMBOL_MAPS := $(foreach bin,$(USERSPACE_BINARIES),$(call symbol_map_target,$(bin)))
USERSPACE_LINE_MAPS := $(foreach bin,$(USERSPACE_BINARIES),$(call line_map_target,$(bin)))

USERSPACE_STAGE_ASSETS := $(wildcard doom/*.wad)
USERSPACE_STAGE_ASSETS += $(wildcard shell/shell_exec.txt)
USERSPACE_STAGE_ASSETS += $(wildcard MODPlay/makemove.mod)
USERSPACE_STAGE_ASSETS += $(wildcard MODPlay/UNREALPM.S3M)
USERSPACE_STAGE_ASSETS += $(wildcard rotozoom/head.bmp)
USERSPACE_STAGE_ASSETS += $(wildcard quake/pak0.pak)
USERSPACE_STAGE_ASSETS += $(wildcard quake/progs.dat)
USERSPACE_STAGE_ASSETS += $(wildcard quake/zeus_pak0.pak)
USERSPACE_STAGE_ASSETS += $(wildcard duke3d/*.grp)
USERSPACE_STAGE_ASSETS += $(wildcard vkgui/runtime_plugins/*.vplg)
USERSPACE_STAGE_ASSETS += $(wildcard quake/reaperfx)
USERSPACE_STAGE_ASSETS += $(wildcard clownmdemu/roms)
USERSPACE_STAGE_ASSETS += $(wildcard vnes/roms)
USERSPACE_STAGE_ASSETS += $(wildcard snes9x/roms)
USERSPACE_STAGE_ASSETS += $(wildcard vspcplay/tracks)
USERSPACE_STAGE_ASSETS += $(wildcard minimp3/tracks)

USERSPACE_BUILD_OUTPUTS := $(USERSPACE_BINARIES)
ifdef DEBUG
USERSPACE_BUILD_OUTPUTS += $(USERSPACE_SYMBOL_MAPS)
USERSPACE_BUILD_OUTPUTS += $(USERSPACE_LINE_MAPS)
endif

USERSPACE_BUILD_STAMP := .build/userspace$(if $(DEBUG),-debug,).stamp
USERSPACE_RUNTIME_READY := $(USERSPACE_RUNTIME_LIBC_CHECK_FILE)

.PHONY: all clean distclean runtime-setup libc-glue FORCE

all: $(USERSPACE_BUILD_STAMP)

$(USERSPACE_BUILD_STAMP): $(USERSPACE_BUILD_OUTPUTS) $(USERSPACE_STAGE_ASSETS)
	@mkdir -p $(dir $@)
	@touch $@

$(USERSPACE_SYMBOLS_DIR)/%.map: %
	@echo "  MAP     $@"
	@mkdir -p $(dir $@)
	@$(NM) -n -C --defined-only $< > $@

$(USERSPACE_SYMBOLS_DIR)/%.lines: % $(ROOT_DIR)/scripts/generate_line_map.sh
	@echo "  LINES   $@"
	@mkdir -p $(dir $@)
	@bash $(ROOT_DIR)/scripts/generate_line_map.sh $< $@

$(USERSPACE_RUNTIME_READY):
	@USERSPACE_CXX_RUNTIME=$(USERSPACE_CXX_RUNTIME) bash $(ROOT_DIR)/scripts/setup_userspace_runtime.sh

runtime-setup:
	@USERSPACE_CXX_RUNTIME=$(USERSPACE_CXX_RUNTIME) bash $(ROOT_DIR)/scripts/setup_userspace_runtime.sh

_DEBUG_FLAG := $(if $(DEBUG),DEBUG=$(DEBUG),)

libc-glue: $(USERSPACE_RUNTIME_READY)
	@$(MAKE) --no-print-directory -C $(LIBC_DIR) CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

# Always descend into each app submake so the app-owned dependency graph decides
# whether anything actually needs recompiling.
$(HELLO_VBIN): FORCE hello/hello.c hello/Makefile libc-glue
	@$(MAKE) --no-print-directory -C hello CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

$(TLS_SMOKE_VBIN): FORCE tls_smoke/main.cpp tls_smoke/Makefile libc-glue
	@$(MAKE) --no-print-directory -C tls_smoke $(_DEBUG_FLAG)

$(RAYTRACER_VBIN): FORCE raytracer/raytracer.c raytracer/Makefile
	@$(MAKE) --no-print-directory -C raytracer $(_DEBUG_FLAG)

$(SHELL_VBIN): FORCE shell/shell.cpp shell/Makefile
	@$(MAKE) --no-print-directory -C shell $(_DEBUG_FLAG)

$(DOOM_VBIN): FORCE doom/Makefile libc-glue
	@$(MAKE) --no-print-directory -C doom CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

$(QUAKE_VBIN): FORCE quake/Makefile libc-glue
	@$(MAKE) --no-print-directory -C quake CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

$(DUKE3D_VBIN): FORCE duke3d/Makefile libc-glue
	@$(MAKE) --no-print-directory -C duke3d CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

$(MODPLAY_VBIN): FORCE MODPlay/Makefile libc-glue
	@$(MAKE) --no-print-directory -C MODPlay CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

$(CLOWNMDEMU_VBIN): FORCE clownmdemu/Makefile libc-glue
	@$(MAKE) --no-print-directory -C clownmdemu CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

$(MINIMP3_VBIN): FORCE minimp3/Makefile libc-glue
	@$(MAKE) --no-print-directory -C minimp3 CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

$(ROTOZOOM_VBIN): FORCE rotozoom/Makefile libc-glue
	@$(MAKE) --no-print-directory -C rotozoom CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

$(VKGUI_VBIN): FORCE $(wildcard vkgui/*.cpp) vkgui/Makefile libc-glue
	@$(MAKE) --no-print-directory -C vkgui $(_DEBUG_FLAG)

$(SR_CUBE_VBIN): FORCE sr_cube/Makefile libc-glue
	@$(MAKE) --no-print-directory -C sr_cube CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

$(VKOBJ_VBIN): FORCE vkobj/main.cpp vkobj/Makefile libc-glue
	@$(MAKE) --no-print-directory -C vkobj $(_DEBUG_FLAG)

$(VNES_VBIN): FORCE $(wildcard vnes/*.cpp) vnes/Makefile libc-glue
	@$(MAKE) --no-print-directory -C vnes $(_DEBUG_FLAG)

$(SNES9X_VBIN): FORCE snes9x/Makefile libc-glue
	@$(MAKE) --no-print-directory -C snes9x CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

$(VSPCPLAY_VBIN): FORCE vspcplay/Makefile libc-glue
	@$(MAKE) --no-print-directory -C vspcplay CC=$(CROSS_PREFIX)gcc $(_DEBUG_FLAG)

FORCE:

clean:
	@echo "Cleaning userspace artifacts..."
	@rm -rf .build $(USERSPACE_SYMBOLS_DIR)
	@$(MAKE) --no-print-directory -C $(LIBC_DIR) clean
	@$(MAKE) --no-print-directory -C hello clean
	@$(MAKE) --no-print-directory -C tls_smoke clean
	@$(MAKE) --no-print-directory -C raytracer clean
	@$(MAKE) --no-print-directory -C shell clean
	@$(MAKE) --no-print-directory -C doom clean
	@$(MAKE) --no-print-directory -C quake clean
	@$(MAKE) --no-print-directory -C duke3d clean
	@$(MAKE) --no-print-directory -C MODPlay clean
	@$(MAKE) --no-print-directory -C clownmdemu clean
	@$(MAKE) --no-print-directory -C minimp3 clean
	@$(MAKE) --no-print-directory -C rotozoom clean
	@$(MAKE) --no-print-directory -C vkobj clean
	@$(MAKE) --no-print-directory -C vkgui clean
	@$(MAKE) --no-print-directory -C vnes clean
	@$(MAKE) --no-print-directory -C snes9x clean
	@$(MAKE) --no-print-directory -C vspcplay clean
	@$(MAKE) --no-print-directory -C sr_cube clean

distclean: clean
	@echo "Removing userspace runtime sysroot and build..."
	@$(MAKE) --no-print-directory -C $(LIBC_DIR) distclean
	@bash $(ROOT_DIR)/scripts/setup_userspace_runtime.sh clean 2>/dev/null || true
