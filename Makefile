.PHONY: all
.DEFAULT_GOAL := all
CXX_FLAGS :=
SIMPLEWINS_SOURCES := $(wildcard ./src/*.cpp ./src/utils/*.cpp)

all:
	@g++ $(SIMPLEWINS_SOURCES) $(CXX_FLAGS) `pkg-config --cflags --libs libinput libdrm libudev` -I./include -o ./build/main

debug: CXX_FLAGS += -g
debug: all
