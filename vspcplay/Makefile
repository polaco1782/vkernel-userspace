CXX     ?= g++
CC      ?= gcc
AR      ?= ar
STRIP   ?= strip

USERSPACE_DIR   := $(realpath $(CURDIR)/..)
include $(USERSPACE_DIR)/runtime_config.mk
include $(USERSPACE_DIR)/cxx_runtime_config.mk

ROOT_DIR        := $(realpath $(CURDIR)/../..)
USER_INCLUDE    := ../include
SYSROOT         := $(USERSPACE_ACTIVE_SYSROOT)
LIBC_DIR        := ../libc
KERNEL_INCLUDE  := $(ROOT_DIR)/include
SNES9X_SRC      := ../snes9x-src
BUILD_DIR       := build
ELF_CHECK       := ../../scripts/check_userspace_elf.sh

DEPFLAGS := -MMD -MP

CXXFLAGS  := -std=c++20
CXXFLAGS  += -ffreestanding
CXXFLAGS  += -fno-builtin
CXXFLAGS  += -fno-stack-protector
CXXFLAGS  += -fpie
CXXFLAGS  += -m64
CXXFLAGS  += -mno-red-zone
CXXFLAGS  += -fno-exceptions
CXXFLAGS  += -fno-rtti
CXXFLAGS  += $(USERSPACE_CXX_INCLUDE_FLAGS)
ifdef DEBUG
CXXFLAGS  += -Og -g3 -fno-omit-frame-pointer -DDEBUG
else
CXXFLAGS  += -O2 -DNDEBUG
endif
CXXFLAGS  += -Wall -Wextra
CXXFLAGS  += -Wno-unused-parameter
CXXFLAGS  += -Wno-unused-variable
CXXFLAGS  += -Wno-write-strings
CXXFLAGS  += -Wno-register
CXXFLAGS  += -Wno-format
CXXFLAGS  += -Wno-missing-field-initializers
CXXFLAGS  += -Wno-deprecated-volatile
CXXFLAGS  += -isystem $(SYSROOT)/include
CXXFLAGS  += -I$(USER_INCLUDE)
CXXFLAGS  += -I$(KERNEL_INCLUDE)
CXXFLAGS  += -I$(SNES9X_SRC)
CXXFLAGS  += -DVSPCPLAY_TRACE_HOOKS

CFLAGS := -std=c11
CFLAGS += -ffreestanding
CFLAGS += -fno-builtin
CFLAGS += -fno-stack-protector
CFLAGS += -fpie
CFLAGS += -m64
CFLAGS += -mno-red-zone
ifdef DEBUG
CFLAGS += -Og -g3 -fno-omit-frame-pointer -DDEBUG
else
CFLAGS += -O2 -DNDEBUG
endif
CFLAGS += -Wall -Wextra
CFLAGS += -Wno-unused-parameter
CFLAGS += -isystem $(SYSROOT)/include
CFLAGS += -I$(USER_INCLUDE)
CFLAGS += -I$(KERNEL_INCLUDE)

LDFLAGS  := -nostdlib
LDFLAGS  += -static-pie
LDFLAGS  += -e _start
LDFLAGS  += -Wl,--build-id=none
LDFLAGS  += -Wl,-z,norelro
LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -L$(SYSROOT)/lib
LDFLAGS  += -L$(LIBC_DIR)

CRT0            := $(USERSPACE_RUNTIME_CRT0)
CPP_RUNTIME_LIB := $(USERSPACE_CXX_RUNTIME_TARGET)
RUNTIME_GLUE    := $(USERSPACE_RUNTIME_GLUE_OBJ)
CXX_MAIN_BRIDGE := $(USERSPACE_CXX_MAIN_BRIDGE_OBJ)
LIBS            := -Wl,--start-group $(USERSPACE_CXX_RUNTIME_LIBS) $(USERSPACE_CXX_RUNTIME_SYSTEM_LIBS) -Wl,--end-group

CPP_MAKE_ARGS :=
ifdef DEBUG
CPP_MAKE_ARGS += DEBUG=$(DEBUG)
endif

LIBC_MAKE_ARGS :=
ifdef DEBUG
LIBC_MAKE_ARGS += DEBUG=$(DEBUG)
endif

LOCAL_CPP_SRCS := \
	app.cpp \
	browser.cpp \
	common.cpp \
	libspc.cpp \
	main.cpp \
	s9x_env.cpp \
	spc_backend.cpp \
	ui.cpp

EXTERNAL_CPP_SRCS := \
	../snes9x-src/apu/apu.cpp \
	../snes9x-src/apu/bapu/dsp/sdsp.cpp \
	../snes9x-src/apu/bapu/smp/smp.cpp \
	../snes9x-src/apu/bapu/smp/smp_state.cpp

C_SRCS := \
	font.c \
	id666.c \
	wavewriter.c

LOCAL_CPP_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(LOCAL_CPP_SRCS))
EXTERNAL_CPP_OBJS := $(patsubst ../%.cpp,$(BUILD_DIR)/%.o,$(EXTERNAL_CPP_SRCS))
C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
ALL_OBJS := $(LOCAL_CPP_OBJS) $(EXTERNAL_CPP_OBJS) $(C_OBJS)
ALL_DEPS := $(ALL_OBJS:.o=.d)

TARGET := vspcplay.vbin

.PHONY: all clean libc_build cpp_build

all: _check_sysroot $(TARGET)

_check_sysroot:
	@test -f $(USERSPACE_RUNTIME_CHECK_FILE) || \
	    { echo ""; \
	      echo "  ERROR: $(USERSPACE_RUNTIME_DISPLAY) headers not found at $(SYSROOT)"; \
	      echo "  Run:  $(USERSPACE_RUNTIME_SETUP_HINT)"; \
	      echo ""; exit 1; }

$(BUILD_DIR)/%.o: %.cpp
	@echo "  CXX     $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(DEPFLAGS) -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/%.o: ../%.cpp
	@echo "  CXX     $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(DEPFLAGS) -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@echo "  CC      $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(DEPFLAGS) -MF $(@:.o=.d) -c $< -o $@

libc_build:
	@$(MAKE) -C $(LIBC_DIR) $(LIBC_MAKE_ARGS)

cpp_build:
	@test -f $(CPP_RUNTIME_LIB) || \
	    { echo ""; \
	      echo "  ERROR: $(USERSPACE_CXX_RUNTIME_DISPLAY) runtime archive not found at $(CPP_RUNTIME_LIB)"; \
	      echo "  Run:  $(USERSPACE_CXX_RUNTIME_SETUP_HINT)"; \
	      echo ""; exit 1; }

$(CRT0) $(RUNTIME_GLUE) $(CXX_MAIN_BRIDGE): libc_build
	@:

$(CPP_RUNTIME_LIB): cpp_build
	@:

$(TARGET): $(CRT0) $(RUNTIME_GLUE) $(CXX_MAIN_BRIDGE) $(CPP_RUNTIME_LIB) $(ALL_OBJS)
	@echo "  LD      $@"
	@$(CXX) $(LDFLAGS) -o $@ $(CRT0) $(RUNTIME_GLUE) $(CXX_MAIN_BRIDGE) $(ALL_OBJS) $(LIBS)
	@bash $(ELF_CHECK) $@
ifndef DEBUG
	@echo "  STRIP   $@"
	@$(STRIP) --strip-unneeded $@
endif
	@echo "  SIZE    $@"
	@size $@ 2>/dev/null || true

clean:
	@rm -rf $(BUILD_DIR) $(TARGET)

-include $(ALL_DEPS)
