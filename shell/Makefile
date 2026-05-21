# vkernel userspace - shell
# Copyright (C) 2026 vkernel authors
#
# Builds a freestanding static binary suitable for loading
# with the vkernel process loader.

CXX     ?= g++
AR      ?= ar
OBJCOPY ?= objcopy

USER_INCLUDE   := ../include
CPP_DIR        := ../cpp
CPP_INCLUDE    := ../cpp/include
CPP_RUNTIME_LIB := $(CPP_DIR)/libvkcpp.a
DEBUG_ARG      := $(if $(DEBUG),DEBUG=$(DEBUG),)

CXXFLAGS  := -std=c++17
CXXFLAGS  += -nostdlib
CXXFLAGS  += -ffreestanding
CXXFLAGS  += -fno-builtin
CXXFLAGS  += -fno-stack-protector
CXXFLAGS  += -fpie
CXXFLAGS  += -m64
CXXFLAGS  += -mno-red-zone
CXXFLAGS  += -fno-exceptions
CXXFLAGS  += -fno-rtti
CXXFLAGS  += -nostdinc++
CXXFLAGS  += -ffunction-sections -fdata-sections
CXXFLAGS  += -O2
CXXFLAGS  += -Wall -Wextra
CXXFLAGS  += -I$(USER_INCLUDE)
CXXFLAGS  += -I$(CPP_INCLUDE)

LDFLAGS := -nostdlib
LDFLAGS += -pie
LDFLAGS += -e _start
LDFLAGS += -Wl,--build-id=none
LDFLAGS += -Wl,-z,norelro
LDFLAGS += -Wl,--gc-sections

TARGET  := shell.vbin

.PHONY: all clean

all: $(CPP_RUNTIME_LIB) $(TARGET)

$(CPP_RUNTIME_LIB):
	@$(MAKE) --no-print-directory -C $(CPP_DIR) CXX=$(CXX) AR=$(AR) $(DEBUG_ARG)

$(TARGET): shell.cpp $(CPP_RUNTIME_LIB)
	@echo "  CXX     $<  ->  $@"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(CPP_RUNTIME_LIB)
	@echo "  SIZE    $@"
	@size $@ 2>/dev/null || true

clean:
	@rm -f $(TARGET)
