CC      ?= gcc
CXX     ?= g++
AR      ?= ar
OBJCOPY ?= objcopy

USERSPACE_DIR  := $(realpath $(CURDIR)/..)
include $(USERSPACE_DIR)/runtime_config.mk
include $(USERSPACE_DIR)/cxx_runtime_config.mk

USER_INCLUDE   := ../include
SYSROOT        := $(USERSPACE_ACTIVE_SYSROOT)
LIBC_DIR       := ../libc
KERNEL_INCLUDE := ../../include
VENDOR_DIR     := vendor/clownmdemu-core
ELF_CHECK      := ../../scripts/check_userspace_elf.sh

CFLAGS  := -std=c11
CFLAGS  += -D_XOPEN_SOURCE=700
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
CFLAGS  += -I$(USER_INCLUDE)
CFLAGS  += -I$(KERNEL_INCLUDE)

CXXFLAGS := -std=c++17
CXXFLAGS += -ffreestanding
CXXFLAGS += -fno-builtin
CXXFLAGS += -fno-stack-protector
CXXFLAGS += -fpie
CXXFLAGS += -m64
CXXFLAGS += -mno-red-zone
CXXFLAGS += -fno-exceptions
CXXFLAGS += -fno-rtti
CXXFLAGS += $(USERSPACE_CXX_INCLUDE_FLAGS)
ifdef DEBUG
CXXFLAGS += -Og -g3 -fno-omit-frame-pointer -DDEBUG
else
CXXFLAGS += -O3 -DNDEBUG
endif
CXXFLAGS += -Wall -Wextra
CXXFLAGS += -isystem $(SYSROOT)/include
CXXFLAGS += -I$(USER_INCLUDE)
CXXFLAGS += -I$(KERNEL_INCLUDE)

LDFLAGS := -nostdlib
LDFLAGS += -static-pie
LDFLAGS += -e _start
LDFLAGS += -Wl,--build-id=none
LDFLAGS += -Wl,-z,norelro
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -L$(SYSROOT)/lib
LDFLAGS += -L$(LIBC_DIR)

CRT0   := $(USERSPACE_RUNTIME_CRT0)
RUNTIME_GLUE := $(USERSPACE_RUNTIME_GLUE_OBJ)
CXX_MAIN_BRIDGE := $(USERSPACE_CXX_MAIN_BRIDGE_OBJ)
CPP_RUNTIME_LIB := $(USERSPACE_CXX_RUNTIME_TARGET)
DEBUG_ARG       := $(if $(DEBUG),DEBUG=$(DEBUG),)
# The frontend still forces the vendored core onto the C API path, while the
# selected C++ runtime stays configurable at the userspace build layer.
LIBS   := -Wl,--start-group $(USERSPACE_CXX_RUNTIME_LIBS) $(USERSPACE_CXX_RUNTIME_SYSTEM_LIBS) -Wl,--end-group

TARGET := clownmdemu.vbin

CPP_SRCS := main.cpp common.cpp audio.cpp browser.cpp app.cpp hqx.cpp hq2x.cpp
C_SRCS   := $(VENDOR_DIR)/unity.c
OBJS     := $(CPP_SRCS:.cpp=.o) $(C_SRCS:.c=.o)

.PHONY: all clean distclean

all: _check_sysroot $(CPP_RUNTIME_LIB) $(TARGET)

_check_sysroot:
	@test -f $(USERSPACE_RUNTIME_CHECK_FILE) || \
	    { echo ""; \
	      echo "  ERROR: $(USERSPACE_RUNTIME_DISPLAY) headers not found at $(SYSROOT)"; \
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

%.o: %.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(CRT0) $(RUNTIME_GLUE) $(CXX_MAIN_BRIDGE) $(CPP_RUNTIME_LIB)
	@echo "  LD      $@"
	@$(CC) $(LDFLAGS) -o $@ $(CRT0) $(RUNTIME_GLUE) $(CXX_MAIN_BRIDGE) $(OBJS) $(LIBS)
	@bash $(ELF_CHECK) $@
ifndef DEBUG
	@echo "  STRIP   $@"
	@strip --strip-unneeded $@
endif
	@echo "  SIZE    $@"
	@size $@ 2>/dev/null || true

clean:
	@rm -f $(OBJS) $(TARGET)

distclean: clean
