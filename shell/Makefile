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

$(TARGET): $(OBJS) $(CPP_RUNTIME_LIB)
	@echo "  LD      $@"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(CPP_RUNTIME_LIB)
	@echo "  SIZE    $@"
	@size $@ 2>/dev/null || true

clean:
	@rm -rf $(BUILD_DIR) $(TARGET)
