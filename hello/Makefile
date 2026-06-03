# vkernel userspace - C runtime smoke test
# Copyright (C) 2026 vkernel authors
#
# Builds a static PIE smoke binary linked against the transitional
# libc runtime while the musl migration is in flight.

CC      ?= gcc
OBJCOPY ?= objcopy
STRIP   ?= strip

USERSPACE_DIR  := $(realpath $(CURDIR)/..)
include $(USERSPACE_DIR)/runtime_config.mk

USER_INCLUDE   := ../include
HEADER_SYSROOT := $(USERSPACE_HEADER_SYSROOT)
LINK_SYSROOT   := $(USERSPACE_LINK_SYSROOT)
LIBC_DIR       := $(USERSPACE_LIBC_DIR)
KERNEL_INCLUDE := ../../include
ELF_CHECK      := ../../scripts/check_userspace_elf.sh

CFLAGS  := -std=c11
CFLAGS  += -ffreestanding
CFLAGS  += -fno-builtin
CFLAGS  += -fno-stack-protector
CFLAGS  += -fpie
CFLAGS  += -m64
CFLAGS  += -mno-red-zone
ifdef DEBUG
CFLAGS  += -Og -g3 -fno-omit-frame-pointer -DDEBUG
else
CFLAGS  += -O3 -DNDEBUG
endif
CFLAGS  += -Wall -Wextra
CFLAGS  += -isystem $(HEADER_SYSROOT)/include
CFLAGS  += -I$(USER_INCLUDE)
CFLAGS  += -I$(KERNEL_INCLUDE)

LDFLAGS := -nostdlib
LDFLAGS += -static-pie
LDFLAGS += -e _start
LDFLAGS += -Wl,--build-id=none
LDFLAGS += -Wl,-z,norelro
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -L$(LINK_SYSROOT)/lib
LDFLAGS += -L$(LIBC_DIR)

# Link order: crt0 → runtime glue → program objects → grouped runtime libs.
CRT0   := $(USERSPACE_RUNTIME_CRT0)
RUNTIME_GLUE := $(USERSPACE_RUNTIME_GLUE_OBJ)
C_MAIN_BRIDGE := $(USERSPACE_C_MAIN_BRIDGE_OBJ)
LIBS   := -Wl,--start-group $(USERSPACE_RUNTIME_SYSTEM_LIBS) -Wl,--end-group

TARGET := hello.vbin

.PHONY: all clean distclean

all: _check_sysroot $(TARGET)

_check_sysroot:
	@test -f $(USERSPACE_RUNTIME_CHECK_FILE) || \
	    { echo ""; \
	      echo "  ERROR: $(USERSPACE_RUNTIME_DISPLAY) headers not found at $(HEADER_SYSROOT)"; \
	      echo "  Run:  $(USERSPACE_RUNTIME_SETUP_HINT)"; \
	      echo ""; exit 1; }

$(CRT0) $(RUNTIME_GLUE) $(C_MAIN_BRIDGE):
	@$(MAKE) --no-print-directory -C $(LIBC_DIR) CC=$(CC) $(if $(DEBUG),DEBUG=$(DEBUG),)

$(TARGET): hello.c $(CRT0) $(RUNTIME_GLUE) $(C_MAIN_BRIDGE)
	@echo "  CC      $<  →  $@"
	@$(CC) $(CFLAGS) -c hello.c -o hello.o
	@echo "  LD      $@"
	@$(CC) $(LDFLAGS) -o $@ $(CRT0) $(RUNTIME_GLUE) $(C_MAIN_BRIDGE) hello.o $(LIBS)
	@bash $(ELF_CHECK) $@
ifndef DEBUG
	@$(STRIP) --strip-unneeded $@
endif
	@rm -f hello.o
	@echo "  SIZE    $@"
	@size $@ 2>/dev/null || true

clean:
	@rm -f $(TARGET) hello.o

distclean: clean
