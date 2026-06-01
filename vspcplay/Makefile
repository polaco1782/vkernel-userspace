CXX     ?= g++
CC      ?= gcc
AR      ?= ar
STRIP   ?= strip

ROOT_DIR        := $(realpath $(CURDIR)/../..)
USER_INCLUDE    := ../include
SYSROOT         := ../sysroot
LIBC_DIR        := ../libc
CPP_DIR         := ../cpp
CPP_INCLUDE     := $(CPP_DIR)/include
KERNEL_INCLUDE  := $(ROOT_DIR)/include
SNES9X_SRC      := ../snes9x-src
BUILD_DIR       := build

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
CXXFLAGS  += -nostdinc++
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
CXXFLAGS  += -I$(CPP_INCLUDE)
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
CFLAGS += -I$(CPP_INCLUDE)
CFLAGS += -I$(KERNEL_INCLUDE)

LDFLAGS  := -nostdlib
LDFLAGS  += -static-pie
LDFLAGS  += -e _start
LDFLAGS  += -Wl,--build-id=none
LDFLAGS  += -Wl,-z,norelro
LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -L$(SYSROOT)/lib
LDFLAGS  += -L$(LIBC_DIR)
LDFLAGS  += -L$(CPP_DIR)

CRT0            := $(LIBC_DIR)/crt0.o
CPP_RUNTIME_LIB := $(CPP_DIR)/libvkcpp.a
LIBVKSYS        := $(LIBC_DIR)/libvksys.a
LIBS            := -Wl,--start-group -lvkcpp -lvksys -lc -lm -lgcc -Wl,--end-group

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

all: $(TARGET)

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
	@$(MAKE) --no-print-directory -C $(CPP_DIR) CXX=$(CXX) AR=$(AR) $(CPP_MAKE_ARGS)

$(CRT0) $(LIBVKSYS): libc_build
	@:

$(CPP_RUNTIME_LIB): cpp_build
	@:

$(TARGET): $(CRT0) $(LIBVKSYS) $(CPP_RUNTIME_LIB) $(ALL_OBJS)
	@echo "  LD      $@"
	@$(CXX) $(LDFLAGS) -o $@ $(CRT0) $(ALL_OBJS) $(LIBS)
ifndef DEBUG
	@echo "  STRIP   $@"
	@$(STRIP) --strip-unneeded $@
endif
	@echo "  SIZE    $@"
	@size $@ 2>/dev/null || true

clean:
	@rm -rf $(BUILD_DIR) $(TARGET)

-include $(ALL_DEPS)
