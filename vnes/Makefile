CXX     ?= g++
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
VKGUI_DIR        := ../vkgui
IMGUI_DIR       := $(VKGUI_DIR)/deps/imgui
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
CXXFLAGS  += -isystem $(SYSROOT)/include
CXXFLAGS  += -I$(USER_INCLUDE)
CXXFLAGS  += -I$(KERNEL_INCLUDE)
CXXFLAGS  += -I$(VKGUI_DIR)
CXXFLAGS  += -I$(IMGUI_DIR)

IMGUI_CXXFLAGS := $(CXXFLAGS)
IMGUI_CXXFLAGS += -Wno-unused-parameter
IMGUI_CXXFLAGS += -Wno-missing-field-initializers
IMGUI_CXXFLAGS += -Wno-sign-compare
IMGUI_CXXFLAGS += -Wno-type-limits
IMGUI_CXXFLAGS += -Wno-misleading-indentation
IMGUI_CXXFLAGS += -DIMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS

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

CORE_SRCS := apu.cpp       \
			 app.cpp       \
			 browser.cpp   \
			 bus.cpp       \
			 cartridge.cpp \
			 cpu.cpp       \
			 hq2x.cpp      \
			 hqx.cpp       \
			 input.cpp     \
			 main.cpp      \
			 mapper.cpp    \
			 mapper_000.cpp \
			 mapper_001.cpp \
			 mapper_002.cpp \
			 mapper_004.cpp \
			 mapper_009.cpp \
			 ppu.cpp       \
			 sound.cpp     \
			 ui.cpp

IMGUI_SRCS := $(IMGUI_DIR)/imgui.cpp      \
		      $(IMGUI_DIR)/imgui_draw.cpp \
		      $(IMGUI_DIR)/imgui_tables.cpp \
		      $(IMGUI_DIR)/imgui_widgets.cpp

VKGUI_SRCS := $(VKGUI_DIR)/imgui_impl_vk.cpp

CORE_OBJS  := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CORE_SRCS))
IMGUI_OBJS := $(patsubst $(IMGUI_DIR)/%.cpp,$(BUILD_DIR)/vkgui_imgui/%.o,$(IMGUI_SRCS))
VKGUI_OBJS  := $(patsubst $(VKGUI_DIR)/%.cpp,$(BUILD_DIR)/vkgui/%.o,$(VKGUI_SRCS))

ALL_OBJS := $(CORE_OBJS) $(IMGUI_OBJS) $(VKGUI_OBJS)
ALL_DEPS := $(ALL_OBJS:.o=.d)

TARGET := vnes.vbin

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

$(BUILD_DIR)/vkgui_imgui/%.o: $(IMGUI_DIR)/%.cpp
	@echo "  CXX [imgui] $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(IMGUI_CXXFLAGS) $(DEPFLAGS) -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/vkgui/%.o: $(VKGUI_DIR)/%.cpp
	@echo "  CXX [vkgui]  $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(IMGUI_CXXFLAGS) $(DEPFLAGS) -MF $(@:.o=.d) -c $< -o $@

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
