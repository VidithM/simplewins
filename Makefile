.PHONY: all
.DEFAULT_GOAL := all
SIMPLEWINS_SOURCES := $(wildcard ./src/*.cpp)

all:
	@g++ $(SIMPLEWINS_SOURCES) -I./include -I/usr/include/libdrm -L/usr/include/lib/x86_64-linux-gnu -ldrm -o ./build/main
