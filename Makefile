# Build flags match the commands documented in README.md.
CC := avr-gcc
CXX := avr-g++
OBJCOPY := avr-objcopy
SIZE := avr-size

MCU := attiny84a
CPPFLAGS := -DF_CPU=8000000UL
CFLAGS := -std=gnu99 -Os -Wall -Wextra
CXXFLAGS := -std=c++11 -Os -Wall -Wextra -fno-exceptions -fno-rtti

.PHONY: all c cpp size clean

all: c cpp

c: build/parking-assist-alarm-c.hex

cpp: build/parking-assist-alarm-cpp.hex

build:
	mkdir -p build

build/parking-assist-alarm-c.elf: main.c Makefile | build
	$(CC) -mmcu=$(MCU) $(CPPFLAGS) $(CFLAGS) main.c -o $@

build/parking-assist-alarm-cpp.elf: main.cpp Makefile | build
	$(CXX) -mmcu=$(MCU) $(CPPFLAGS) $(CXXFLAGS) main.cpp -o $@

build/%.hex: build/%.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

size: c cpp
	$(SIZE) build/parking-assist-alarm-c.elf build/parking-assist-alarm-cpp.elf

clean:
	rm -f build/parking-assist-alarm-c.elf build/parking-assist-alarm-c.hex build/parking-assist-alarm-cpp.elf build/parking-assist-alarm-cpp.hex
