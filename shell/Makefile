# vkernel userspace - shell
# Copyright (C) 2026 vkernel authors
#
# Builds a freestanding static binary suitable for loading
# with the vkernel process loader.

CXX     ?= g++
CC      ?= gcc
AR      ?= ar
OBJCOPY ?= objcopy

USERSPACE_DIR   := $(realpath $(CURDIR)/..)
include $(USERSPACE_DIR)/runtime_config.mk
include $(USERSPACE_DIR)/cxx_runtime_config.mk

USER_INCLUDE   := ../include
HEADER_SYSROOT := $(USERSPACE_HEADER_SYSROOT)
LINK_SYSROOT   := $(USERSPACE_LINK_SYSROOT)
LIBC_DIR       := $(USERSPACE_LIBC_DIR)
CRT0           := $(USERSPACE_RUNTIME_CRT0)
CPP_RUNTIME_LIB := $(USERSPACE_CXX_RUNTIME_TARGET)
RUNTIME_GLUE   := $(USERSPACE_RUNTIME_GLUE_OBJ)
CXX_MAIN_BRIDGE := $(USERSPACE_CXX_MAIN_BRIDGE_OBJ)
DEBUG_ARG      := $(if $(DEBUG),DEBUG=$(DEBUG),)
ELF_CHECK      := ../../scripts/check_userspace_elf.sh
BUILD_DIR      := .build
OBJ_DIR        := $(BUILD_DIR)/obj
APPLET_DIR     := applets
APPLET_HEADER  := $(APPLET_DIR)/applet.h
APPLET_SRCS    := $(wildcard $(APPLET_DIR)/*.cpp)
APPLET_REGISTRY_SRC := $(BUILD_DIR)/applet_registry.cpp
SHELL_SRCS     := shell.cpp shell_support.cpp $(APPLET_SRCS)
SHELL_OBJS     := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SHELL_SRCS))
APPLET_REGISTRY_OBJ := $(OBJ_DIR)/applet_registry.o
OBJS           := $(SHELL_OBJS) $(APPLET_REGISTRY_OBJ)

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
CXXFLAGS  += $(USERSPACE_CXX_INCLUDE_FLAGS)
CXXFLAGS  += -ffunction-sections -fdata-sections
ifdef DEBUG
CXXFLAGS  += -Og -g3 -fno-omit-frame-pointer -DDEBUG
else
CXXFLAGS  += -O2 -DNDEBUG
endif
CXXFLAGS  += -Wall -Wextra
CXXFLAGS  += -isystem $(HEADER_SYSROOT)/include
CXXFLAGS  += -I$(USER_INCLUDE)

LDFLAGS := -nostdlib
LDFLAGS += -static-pie
LDFLAGS += -e _start
LDFLAGS += -Wl,--build-id=none
LDFLAGS += -Wl,-z,norelro
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -L$(LINK_SYSROOT)/lib
LDFLAGS += -L$(LIBC_DIR)

RUNTIME_LIBS := -Wl,--start-group $(USERSPACE_CXX_RUNTIME_LIBS) $(USERSPACE_CXX_RUNTIME_SYSTEM_LIBS) -Wl,--end-group

TARGET  := shell.vbin

.PHONY: all clean

all: _check_sysroot $(CPP_RUNTIME_LIB) $(TARGET)

$(CPP_RUNTIME_LIB):
	@test -f $@ || \
	    { echo ""; \
	      echo "  ERROR: $(USERSPACE_CXX_RUNTIME_DISPLAY) runtime archive not found at $@"; \
	      echo "  Run:  $(USERSPACE_CXX_RUNTIME_SETUP_HINT)"; \
	      echo ""; exit 1; }

_check_sysroot:
	@test -f $(USERSPACE_RUNTIME_CHECK_FILE) || \
	    { echo ""; \
	      echo "  ERROR: $(USERSPACE_RUNTIME_DISPLAY) headers not found at $(HEADER_SYSROOT)"; \
	      echo "  Run:  $(USERSPACE_RUNTIME_SETUP_HINT)"; \
	      echo ""; exit 1; }
	@test -f $(USERSPACE_CXX_RUNTIME_CHECK_FILE) || \
	    { echo ""; \
	      echo "  ERROR: $(USERSPACE_CXX_RUNTIME_DISPLAY) headers not found"; \
	      echo "  Run:  $(USERSPACE_CXX_RUNTIME_SETUP_HINT)"; \
	      echo ""; exit 1; }

$(CRT0) $(RUNTIME_GLUE) $(CXX_MAIN_BRIDGE):
	@$(MAKE) --no-print-directory -C $(LIBC_DIR) CC=$(CC) AR=$(AR) $(DEBUG_ARG)

$(APPLET_REGISTRY_SRC): $(APPLET_SRCS) $(APPLET_HEADER)
	@echo "  GEN     $@"
	@mkdir -p $(dir $@)
	@{ \
		printf '#include "../applets/applet.h"\n'; \
		printf '#include <array>\n\n'; \
		for src in $(APPLET_SRCS); do \
			name=$$(basename "$$src" .cpp); \
			printf 'namespace applet::%s { extern const shell::command_spec kCommand; }\n' "$$name"; \
		done; \
		printf '\nnamespace shell {\nnamespace {\n'; \
		printf 'constexpr std::array<const command_spec*, %s> kAppletCommands = {{\n' "$(words $(APPLET_SRCS))"; \
		for src in $(APPLET_SRCS); do \
			name=$$(basename "$$src" .cpp); \
			printf '    &applet::%s::kCommand,\n' "$$name"; \
		done; \
		printf '}};\n'; \
		printf '} // namespace\n\n'; \
		printf 'auto applet_commands() -> command_list_view\n{\n'; \
		printf '    return {kAppletCommands.data(), static_cast<vk_usize>(kAppletCommands.size())};\n'; \
		printf '}\n'; \
		printf '} // namespace shell\n'; \
	} > $@

$(OBJ_DIR)/%.o: %.cpp $(APPLET_HEADER)
	@echo "  CXX     $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(APPLET_REGISTRY_OBJ): $(APPLET_REGISTRY_SRC) $(APPLET_SRCS) $(APPLET_HEADER)
	@echo "  CXX     $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(CPP_RUNTIME_LIB) $(CRT0) $(RUNTIME_GLUE) $(CXX_MAIN_BRIDGE)
	@echo "  LD      $@"
	@$(CXX) $(LDFLAGS) -o $@ $(CRT0) $(RUNTIME_GLUE) $(CXX_MAIN_BRIDGE) $(OBJS) $(RUNTIME_LIBS)
	@bash $(ELF_CHECK) $@
	@echo "  SIZE    $@"
	@size $@ 2>/dev/null || true

clean:
	@rm -rf $(BUILD_DIR) $(TARGET)
