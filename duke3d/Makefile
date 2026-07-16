# vkernel userspace - Chocolate Duke3D
# Copyright (C) 2026 vkernel authors
# Copyright (C) 1996-2003 3D Realms Entertainment
# Copyright (C) Fabien Sanglard (Chocolate Duke3D)

CC      ?= gcc
STRIP   ?= strip

USERSPACE_DIR  := $(realpath $(CURDIR)/..)
include $(USERSPACE_DIR)/runtime_config.mk

DUKE_ROOT      := ../duke3d-src
DUKE_ENGINE    := $(DUKE_ROOT)/Engine/src
DUKE_GAME      := $(DUKE_ROOT)/Game/src
DUKE_AUDIO     := $(DUKE_GAME)/audiolib
USER_INCLUDE   := ../include
SYSROOT        := $(USERSPACE_ACTIVE_SYSROOT)
LIBC_DIR       := ../libc
KERNEL_INCLUDE := ../../include
SDL_SHIM       := sdl_shim
ELF_CHECK      := ../../scripts/check_userspace_elf.sh

CFLAGS  := -std=c11
CFLAGS  += -D_GNU_SOURCE
CFLAGS  += -ffreestanding
CFLAGS  += -fno-builtin
CFLAGS  += -fno-stack-protector
CFLAGS  += -fpie
CFLAGS  += -fcommon
CFLAGS  += -m64
CFLAGS  += -mno-red-zone
CFLAGS  += -ffunction-sections -fdata-sections
ifdef DEBUG
CFLAGS  += -Og -g3 -fno-omit-frame-pointer -DDEBUG
else
CFLAGS  += -O2 -DNDEBUG
endif
CFLAGS  += -Wall
CFLAGS  += -Wno-address-of-packed-member
CFLAGS  += -Wno-format
CFLAGS  += -Wno-incompatible-pointer-types
CFLAGS  += -Wno-implicit-fallthrough
CFLAGS  += -Wno-int-conversion
CFLAGS  += -Wno-maybe-uninitialized
CFLAGS  += -Wno-missing-field-initializers
CFLAGS  += -Wno-parentheses
CFLAGS  += -Wno-pointer-sign
CFLAGS  += -Wno-pointer-to-int-cast
CFLAGS  += -Wno-sign-compare
CFLAGS  += -Wno-unused-but-set-variable
CFLAGS  += -Wno-unused-function
CFLAGS  += -Wno-unused-parameter
CFLAGS  += -Wno-int-to-pointer-cast
CFLAGS  += -isystem $(SYSROOT)/include
CFLAGS  += -I$(SDL_SHIM)
CFLAGS  += -I$(USER_INCLUDE)
CFLAGS  += -I$(KERNEL_INCLUDE)
CFLAGS  += -I.
CFLAGS  += -I$(DUKE_ENGINE)
CFLAGS  += -I$(DUKE_GAME)
CFLAGS  += -DPLATFORM_UNIX
CFLAGS  += -DUSE_SDL=1
CFLAGS  += -DBYTE_ORDER=LITTLE_ENDIAN
CFLAGS  += -DDUKE_DEFAULT_GAME_DIR='"/data/duke3d"'

LDFLAGS := -nostdlib
LDFLAGS += -static-pie
LDFLAGS += -e _start
LDFLAGS += -Wl,--build-id=none
LDFLAGS += -Wl,-z,norelro
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -L$(SYSROOT)/lib
LDFLAGS += -L$(LIBC_DIR)

CRT0 := $(USERSPACE_RUNTIME_CRT0)
RUNTIME_GLUE := $(USERSPACE_RUNTIME_GLUE_OBJ)
C_MAIN_BRIDGE := $(USERSPACE_C_MAIN_BRIDGE_OBJ)
LIBS := -Wl,--start-group $(USERSPACE_RUNTIME_SYSTEM_LIBS) -Wl,--end-group

ENGINE_SRCS := \
	$(DUKE_ENGINE)/cache.c \
	$(DUKE_ENGINE)/draw.c \
	$(DUKE_ENGINE)/engine.c \
	$(DUKE_ENGINE)/filesystem.c \
	$(DUKE_ENGINE)/fixedPoint_math.c \
	$(DUKE_ENGINE)/mmulti.c \
	$(DUKE_ENGINE)/network.c \
	$(DUKE_ENGINE)/tiles.c

GAME_SRCS := \
	$(DUKE_GAME)/actors.c \
	$(DUKE_GAME)/animlib.c \
	$(DUKE_GAME)/config.c \
	$(DUKE_GAME)/console.c \
	$(DUKE_GAME)/control.c \
	$(DUKE_GAME)/cvar_defs.c \
	$(DUKE_GAME)/cvars.c \
	$(DUKE_GAME)/dummy_audiolib.c \
	$(DUKE_GAME)/game.c \
	$(DUKE_GAME)/gamedef.c \
	$(DUKE_GAME)/global.c \
	$(DUKE_GAME)/keyboard.c \
	$(DUKE_GAME)/menues.c \
	$(DUKE_GAME)/player.c \
	$(DUKE_GAME)/rts.c \
	$(DUKE_GAME)/scriplib.c \
	$(DUKE_GAME)/sector.c \
	$(DUKE_GAME)/sounds.c

AUDIO_SRCS := \
	$(DUKE_AUDIO)/fx_man.c \
	$(DUKE_AUDIO)/ll_man.c \
	$(DUKE_AUDIO)/multivoc.c \
	$(DUKE_AUDIO)/mv_mix.c \
	$(DUKE_AUDIO)/mvreverb.c \
	$(DUKE_AUDIO)/nodpmi.c \
	$(DUKE_AUDIO)/pitch.c \
	$(DUKE_AUDIO)/user.c

VK_SRCS := \
	display_vk.c \
	dsl_vk.c \
	midi_vk.c

OBJ_DIR := obj
ENGINE_OBJS := $(patsubst $(DUKE_ENGINE)/%.c,$(OBJ_DIR)/engine_%.o,$(ENGINE_SRCS))
GAME_OBJS := $(patsubst $(DUKE_GAME)/%.c,$(OBJ_DIR)/game_%.o,$(GAME_SRCS))
AUDIO_OBJS := $(patsubst $(DUKE_AUDIO)/%.c,$(OBJ_DIR)/audio_%.o,$(AUDIO_SRCS))
VK_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(VK_SRCS))
PREMAP_OBJ := $(OBJ_DIR)/game_premap.o
ALL_OBJS := $(ENGINE_OBJS) $(GAME_OBJS) $(AUDIO_OBJS) $(VK_OBJS) $(PREMAP_OBJ)

TARGET := duke3d.vbin

.PHONY: all clean distclean

all: _check_sysroot $(TARGET)

_check_sysroot:
	@test -f $(USERSPACE_RUNTIME_CHECK_FILE) || \
	    { echo ""; \
	      echo "  ERROR: $(USERSPACE_RUNTIME_DISPLAY) headers not found at $(SYSROOT)"; \
	      echo "  Run:  $(USERSPACE_RUNTIME_SETUP_HINT)"; \
	      echo ""; exit 1; }

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/engine_%.o: $(DUKE_ENGINE)/%.c | $(OBJ_DIR)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/game_%.o: $(DUKE_GAME)/%.c | $(OBJ_DIR)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/audio_%.o: $(DUKE_AUDIO)/%.c | $(OBJ_DIR)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PREMAP_OBJ): $(DUKE_GAME)/premap.c | $(OBJ_DIR)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -O0 -c $< -o $@

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(ALL_OBJS) $(CRT0) $(RUNTIME_GLUE) $(C_MAIN_BRIDGE)
	@echo "  LD      $@"
	@$(CC) $(LDFLAGS) -o $@ $(CRT0) $(RUNTIME_GLUE) $(C_MAIN_BRIDGE) $(ALL_OBJS) $(LIBS)
	@bash $(ELF_CHECK) $@
ifndef DEBUG
	@echo "  STRIP   $@"
	@$(STRIP) --strip-unneeded $@
endif
	@echo "  SIZE    $@"
	@size $@ 2>/dev/null || true

clean:
	@rm -rf $(OBJ_DIR) $(TARGET)

distclean: clean
