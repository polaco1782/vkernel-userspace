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
SNES9X_DIR      := ../snes9x-src
BUILD_DIR       := build

DEPFLAGS := -MMD -MP

CXXFLAGS  := -std=c++17
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
CXXFLAGS  += -Wno-unused-function
CXXFLAGS  += -Wno-sign-compare
CXXFLAGS  += -Wno-missing-field-initializers
CXXFLAGS  += -Wno-implicit-fallthrough
CXXFLAGS  += -isystem $(SYSROOT)/include
CXXFLAGS  += -I$(USER_INCLUDE)
CXXFLAGS  += -I$(CPP_INCLUDE)
CXXFLAGS  += -I$(KERNEL_INCLUDE)
CXXFLAGS  += -I$(SNES9X_DIR)
CXXFLAGS  += -DHAVE_STDINT_H
CXXFLAGS  += -DHAVE_STRINGS_H
CXXFLAGS  += -DRIGHTSHIFT_IS_SAR
CXXFLAGS  += -DALLOW_CPU_OVERCLOCK

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

FRONTEND_SRCS := main.cpp app.cpp audio.cpp browser.cpp common.cpp compat_stubs.cpp

CORE_SRCS := \
	$(SNES9X_DIR)/fxinst.cpp \
	$(SNES9X_DIR)/fxemu.cpp \
	$(SNES9X_DIR)/fxdbg.cpp \
	$(SNES9X_DIR)/c4.cpp \
	$(SNES9X_DIR)/c4emu.cpp \
	$(SNES9X_DIR)/apu/apu.cpp \
	$(SNES9X_DIR)/apu/bapu/dsp/sdsp.cpp \
	$(SNES9X_DIR)/apu/bapu/smp/smp.cpp \
	$(SNES9X_DIR)/apu/bapu/smp/smp_state.cpp \
	$(SNES9X_DIR)/msu1.cpp \
	$(SNES9X_DIR)/dsp.cpp \
	$(SNES9X_DIR)/dsp1.cpp \
	$(SNES9X_DIR)/dsp2.cpp \
	$(SNES9X_DIR)/dsp3.cpp \
	$(SNES9X_DIR)/dsp4.cpp \
	$(SNES9X_DIR)/spc7110.cpp \
	$(SNES9X_DIR)/obc1.cpp \
	$(SNES9X_DIR)/seta.cpp \
	$(SNES9X_DIR)/seta010.cpp \
	$(SNES9X_DIR)/seta011.cpp \
	$(SNES9X_DIR)/seta018.cpp \
	$(SNES9X_DIR)/controls.cpp \
	$(SNES9X_DIR)/crosshairs.cpp \
	$(SNES9X_DIR)/cpu.cpp \
	$(SNES9X_DIR)/sa1.cpp \
	$(SNES9X_DIR)/debug.cpp \
	$(SNES9X_DIR)/sdd1.cpp \
	$(SNES9X_DIR)/tile.cpp \
	$(SNES9X_DIR)/tileimpl-n1x1.cpp \
	$(SNES9X_DIR)/tileimpl-n2x1.cpp \
	$(SNES9X_DIR)/tileimpl-h2x1.cpp \
	$(SNES9X_DIR)/srtc.cpp \
	$(SNES9X_DIR)/gfx.cpp \
	$(SNES9X_DIR)/memmap.cpp \
	$(SNES9X_DIR)/clip.cpp \
	$(SNES9X_DIR)/ppu.cpp \
	$(SNES9X_DIR)/dma.cpp \
	$(SNES9X_DIR)/snes9x.cpp \
	$(SNES9X_DIR)/globals.cpp \
	$(SNES9X_DIR)/stream.cpp \
	$(SNES9X_DIR)/conffile.cpp \
	$(SNES9X_DIR)/bsx.cpp \
	$(SNES9X_DIR)/snapshot.cpp \
	$(SNES9X_DIR)/movie.cpp \
	$(SNES9X_DIR)/statemanager.cpp \
	$(SNES9X_DIR)/sha256.cpp \
	$(SNES9X_DIR)/cpuops.cpp \
	$(SNES9X_DIR)/cpuexec.cpp \
	$(SNES9X_DIR)/sa1cpu.cpp \
	$(SNES9X_DIR)/cheats.cpp \
	$(SNES9X_DIR)/sdd1emu.cpp \
	$(SNES9X_DIR)/netplay.cpp \
	$(SNES9X_DIR)/server.cpp \
	$(SNES9X_DIR)/loadzip.cpp \
	$(SNES9X_DIR)/fscompat.cpp

FRONTEND_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/frontend/%.o,$(FRONTEND_SRCS))
CORE_OBJS := $(patsubst $(SNES9X_DIR)/%.cpp,$(BUILD_DIR)/core/%.o,$(CORE_SRCS))
ALL_OBJS := $(FRONTEND_OBJS) $(CORE_OBJS)
ALL_DEPS := $(ALL_OBJS:.o=.d)

TARGET := snes9x.vbin

.PHONY: all clean libc_build cpp_build

all: $(TARGET)

$(BUILD_DIR)/frontend/%.o: %.cpp
	@echo "  CXX     $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(DEPFLAGS) -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/core/%.o: $(SNES9X_DIR)/%.cpp
	@echo "  CXX [s9x] $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(DEPFLAGS) -MF $(@:.o=.d) -c $< -o $@

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
