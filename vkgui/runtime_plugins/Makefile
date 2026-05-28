CXX ?= g++

ROOT_DIR       := $(realpath $(CURDIR)/../../..)
USER_INCLUDE   := $(ROOT_DIR)/userspace/include
SYSROOT        := $(ROOT_DIR)/userspace/sysroot
CPP_INCLUDE    := $(ROOT_DIR)/userspace/cpp/include
KERNEL_INCLUDE := $(ROOT_DIR)/include
VKGUI_DIR       := $(ROOT_DIR)/userspace/vkgui

DEPFLAGS := -MMD -MP

CXXFLAGS  := -std=c++20
CXXFLAGS  += -ffreestanding
CXXFLAGS  += -fno-builtin
CXXFLAGS  += -fno-stack-protector
CXXFLAGS  += -fPIC
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
CXXFLAGS  += -isystem $(SYSROOT)/include
CXXFLAGS  += -I$(USER_INCLUDE)
CXXFLAGS  += -I$(CPP_INCLUDE)
CXXFLAGS  += -I$(KERNEL_INCLUDE)
CXXFLAGS  += -I$(VKGUI_DIR)

LDFLAGS  := -nostdlib
LDFLAGS  += -shared
LDFLAGS  += -Wl,--build-id=none
LDFLAGS  += -Wl,-z,norelro
CXXFLAGS += -fvisibility=hidden
LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -Wl,--hash-style=sysv

SRCS := runtime_demo_plugin.cpp
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)
TARGETS := runtime_demo.vplg

.PHONY: all clean

all: $(TARGETS)

%.o: %.cpp
	@echo "  CXX [vplg]  $<"
	@$(CXX) $(CXXFLAGS) $(DEPFLAGS) -MF $(@:.o=.d) -c $< -o $@

runtime_demo.vplg: runtime_demo_plugin.o
	@echo "  LD  [vplg]  $@"
	@$(CXX) $(LDFLAGS) -o $@ $^

clean:
	@rm -f $(OBJS) $(DEPS) $(TARGETS)

-include $(DEPS)
