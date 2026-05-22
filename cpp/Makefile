CXX ?= g++
AR  ?= ar

CPP_INCLUDE := ./include

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
CXXFLAGS  += -I$(CPP_INCLUDE)

SRCS   := runtime/cxx_runtime.cpp \
		  runtime/iostream.cpp
OBJS   := $(SRCS:.cpp=.o)
TARGET := libvkcpp.a

.PHONY: all clean

all: $(TARGET)

runtime/%.o: runtime/%.cpp
	@echo "  CXX     $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	@echo "  AR      $@"
	@$(AR) rcs $@ $(OBJS)

clean:
	@rm -f $(TARGET) $(OBJS)
