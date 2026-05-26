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
BUILD_DIR      := .build
OBJ_DIR        := $(BUILD_DIR)/obj
APPLET_DIR     := applets
APPLET_HEADER  := $(APPLET_DIR)/applet.h
APPLET_SRCS    := $(wildcard $(APPLET_DIR)/*.cpp)
APPLET_REGISTRY_SRC := applet_registry.cpp
SHELL_SRCS     := shell.cpp shell_support.cpp $(APPLET_REGISTRY_SRC) $(APPLET_SRCS)
SHELL_OBJS     := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SHELL_SRCS))
OBJS           := $(SHELL_OBJS)

CXXFLAGS  := -std=c++20
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
ifdef DEBUG
CXXFLAGS  += -Og -g3 -fno-omit-frame-pointer -DDEBUG
else
CXXFLAGS  += -O2 -DNDEBUG
endif
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

$(OBJ_DIR)/%.o: %.cpp $(APPLET_HEADER)
	@echo "  CXX     $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(CPP_RUNTIME_LIB)
	@echo "  LD      $@"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(CPP_RUNTIME_LIB)
	@echo "  SIZE    $@"
	@size $@ 2>/dev/null || true

clean:
	@rm -rf $(BUILD_DIR) $(TARGET)
