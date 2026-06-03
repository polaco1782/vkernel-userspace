ifndef USERSPACE_DIR
$(error USERSPACE_DIR must be defined before including cxx_runtime_config.mk)
endif

ifndef USERSPACE_RUNTIME_DIR
$(error Include runtime_config.mk before cxx_runtime_config.mk)
endif

USERSPACE_CXX_RUNTIME ?= libcxx

USERSPACE_LLVM_PROJECT_VENDOR_DIR := $(USERSPACE_RUNTIME_DIR)/vendor/llvm-project
USERSPACE_CXX_RUNTIME_SETUP_HINT := bash $(realpath $(USERSPACE_DIR)/../scripts/setup_userspace_runtime.sh)
USERSPACE_CXX_RUNTIME_SETUP_CHECK_FILE :=
USERSPACE_CXX_RUNTIME_SYSTEM_LIBS :=
USERSPACE_CXX_MAIN_BRIDGE_OBJ ?= $(USERSPACE_LIBC_DIR)/cxx_main_bridge.o
USERSPACE_CXX_ENTRY_FLAGS := -Dmain=__vkernel_cpp_main

ifeq ($(USERSPACE_CXX_RUNTIME),libcxx)
USERSPACE_CXX_RUNTIME_DISPLAY := libcxx
USERSPACE_CXX_INCLUDE_FLAGS := -nostdinc++ -isystem $(USERSPACE_ACTIVE_SYSROOT)/include/c++/v1 $(USERSPACE_CXX_ENTRY_FLAGS)
USERSPACE_CXX_RUNTIME_TARGET := $(USERSPACE_ACTIVE_SYSROOT)/lib/libc++.a
USERSPACE_CXX_RUNTIME_LIBS := -lc++ -lc++abi
USERSPACE_CXX_RUNTIME_CHECK_FILE := $(USERSPACE_ACTIVE_SYSROOT)/include/c++/v1/__config
USERSPACE_CXX_RUNTIME_SETUP_CHECK_FILE := $(USERSPACE_ACTIVE_SYSROOT)/include/c++/v1/__config
USERSPACE_CXX_RUNTIME_SYSTEM_LIBS := $(USERSPACE_RUNTIME_SYSTEM_LIBS)

ifeq ($(origin CXX),default)
CXX := clang++
else ifeq ($(origin CXX),file)
ifeq ($(CXX),g++)
override CXX := clang++
endif
endif
else
$(error Unsupported USERSPACE_CXX_RUNTIME='$(USERSPACE_CXX_RUNTIME)' (expected libcxx))
endif
