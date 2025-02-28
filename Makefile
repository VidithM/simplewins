.PHONY: all
.DEFAULT_GOAL := all
SIMPLEWINS_SOURCES := $(wildcard ./src/*.cpp)

all:
	@g++ $(SIMPLEWINS_SOURCES) `pkg-config --cflags --libs libinput libdrm` -I./include -o ./build/main
