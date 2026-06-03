# vkernel userspace - C runtime glue library
# Copyright (C) 2026 vkernel authors
#
# Builds:
#   crt0.o         — entry-point object (linked first by normal programs)
#   runtime_glue.o — always-linked syscall/C++ ABI glue object

CC           ?= gcc
CXX          ?= clang++
ROOT_DIR     := $(realpath $(CURDIR)/../..)
USERSPACE_DIR := $(realpath $(CURDIR)/..)
include $(USERSPACE_DIR)/runtime_config.mk

SYSROOT      := $(USERSPACE_GLUE_SYSROOT)
KERNEL_INC   := $(ROOT_DIR)/include

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
CFLAGS  += -isystem $(SYSROOT)/include
CFLAGS  += -I$(KERNEL_INC)

TARGET_CRT := crt0.o
TARGET_GLUE := runtime_glue.o
TARGET_C_MAIN := c_main_bridge.o
TARGET_CXX_MAIN := cxx_main_bridge.o

.PHONY: all clean distclean

all: $(TARGET_CRT) $(TARGET_GLUE) $(TARGET_C_MAIN) $(TARGET_CXX_MAIN)

$(TARGET_CRT): crt0.c
	@echo "  CC      $<  →  $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_GLUE): runtime_glue.c
	@echo "  CC      $<  →  $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_C_MAIN): c_main_bridge.c
	@echo "  CC      $<  →  $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_CXX_MAIN): cxx_main_bridge.cpp
	@echo "  CXX     $<  →  $@"
	@$(CXX) -std=c++20 -ffreestanding -fno-builtin -fno-stack-protector -fpie -m64 -mno-red-zone -fno-exceptions -fno-rtti -Wall -Wextra -isystem $(SYSROOT)/include -I$(KERNEL_INC) -c $< -o $@

clean:
	@rm -f $(TARGET_CRT) $(TARGET_GLUE) $(TARGET_C_MAIN) $(TARGET_CXX_MAIN)

distclean: clean
