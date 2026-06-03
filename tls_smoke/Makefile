CXX   ?= g++
AR    ?= ar
STRIP ?= strip

USERSPACE_DIR  := $(realpath $(CURDIR)/..)
include $(USERSPACE_DIR)/runtime_config.mk
include $(USERSPACE_DIR)/cxx_runtime_config.mk

USER_INCLUDE   := ../include
HEADER_SYSROOT := $(USERSPACE_HEADER_SYSROOT)
LINK_SYSROOT   := $(USERSPACE_LINK_SYSROOT)
LIBC_DIR       := $(USERSPACE_LIBC_DIR)
KERNEL_INCLUDE := ../../include
ELF_CHECK      := ../../scripts/check_userspace_elf.sh

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
CXXFLAGS  += -isystem $(HEADER_SYSROOT)/include
CXXFLAGS  += -I$(USER_INCLUDE)
CXXFLAGS  += -I$(KERNEL_INCLUDE)

LDFLAGS  := -nostdlib
LDFLAGS  += -static-pie
LDFLAGS  += -e _start
LDFLAGS  += -Wl,--build-id=none
LDFLAGS  += -Wl,-z,norelro
LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -L$(LINK_SYSROOT)/lib
LDFLAGS  += -L$(LIBC_DIR)

CRT0            := $(USERSPACE_RUNTIME_CRT0)
RUNTIME_GLUE    := $(USERSPACE_RUNTIME_GLUE_OBJ)
CXX_MAIN_BRIDGE := $(USERSPACE_CXX_MAIN_BRIDGE_OBJ)
CPP_RUNTIME_LIB := $(USERSPACE_CXX_RUNTIME_TARGET)
LIBS            := -Wl,--start-group $(USERSPACE_CXX_RUNTIME_LIBS) $(USERSPACE_CXX_RUNTIME_SYSTEM_LIBS) -Wl,--end-group
DEBUG_ARG       := $(if $(DEBUG),DEBUG=$(DEBUG),)

TARGET := tls_smoke.vbin
OBJS   := main.o

.PHONY: all clean

all: _check_sysroot $(CPP_RUNTIME_LIB) $(TARGET)

_check_sysroot:
	@test -f $(USERSPACE_RUNTIME_CHECK_FILE) || \
	    { echo ""; \
	      echo "  ERROR: $(USERSPACE_RUNTIME_DISPLAY) headers not found at $(HEADER_SYSROOT)"; \
	      echo "  Run:  $(USERSPACE_RUNTIME_SETUP_HINT)"; \
	      echo ""; exit 1; }

$(CPP_RUNTIME_LIB):
	@test -f $@ || \
	    { echo ""; \
	      echo "  ERROR: $(USERSPACE_CXX_RUNTIME_DISPLAY) runtime archive not found at $@"; \
	      echo "  Run:  $(USERSPACE_CXX_RUNTIME_SETUP_HINT)"; \
	      echo ""; exit 1; }

$(CRT0) $(RUNTIME_GLUE) $(CXX_MAIN_BRIDGE):
	@$(MAKE) --no-print-directory -C $(LIBC_DIR) CC=$(CC) $(DEBUG_ARG)

%.o: %.cpp
	@echo "  CXX     $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(CRT0) $(RUNTIME_GLUE) $(CXX_MAIN_BRIDGE) $(CPP_RUNTIME_LIB)
	@echo "  LD      $@"
	@$(CXX) $(LDFLAGS) -o $@ $(CRT0) $(RUNTIME_GLUE) $(CXX_MAIN_BRIDGE) $(OBJS) $(LIBS)
	@bash $(ELF_CHECK) $@
ifndef DEBUG
	@echo "  STRIP   $@"
	@$(STRIP) --strip-unneeded $@
endif
	@echo "  SIZE    $@"
	@size $@ 2>/dev/null || true

clean:
	@rm -f $(OBJS) $(TARGET)
