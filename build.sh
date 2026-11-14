#!/bin/bash
# Command-line build of TireTester.hex (alternative to the Arduino IDE).
# Set these to your local copies:
#   AVR_GCC   avr-gcc bin folder        ARDUINO_AVR  ArduinoCore-avr checkout
#   ARDUBOY2  Arduboy2 library folder   ARDUBOYFX    ArduboyFX library folder
#   ARDUBOYTONES  ArduboyTones library folder
set -e
cd "$(dirname "$0")"
export PATH=${AVR_GCC:-/opt/avr/avr-gcc-14.1.0-x64-linux/bin}:$PATH
AC=${ARDUINO_AVR:-/opt/ArduinoCore-avr}; CORE=$AC/cores/arduino; VAR=$AC/variants/leonardo; EE=$AC/libraries/EEPROM/src
A2=${ARDUBOY2:-/opt/Arduboy2}/src; FX=${ARDUBOYFX:-/opt/ArduboyFX}/src; AT=${ARDUBOYTONES:-/opt/libs/MLXXXp_ArduboyTones}/src
SK=TireTester; B=build; rm -rf $B; mkdir -p $B/core $B/lib
DEFS="-mmcu=atmega32u4 -DF_CPU=16000000L -DARDUINO=10819 -DARDUINO_AVR_LEONARDO -DARDUINO_ARCH_AVR -DUSB_VID=0x2341 -DUSB_PID=0x8036 -DARDUBOY_10"
INC="-I$CORE -I$VAR -I$A2 -I$FX -I$AT -I$EE -I$SK"
CF="-Os -w -ffunction-sections -fdata-sections -flto -fno-fat-lto-objects"
CXF="$CF $EXTRA -mcall-prologues -mrelax -mstrict-X -std=gnu++17 -fpermissive -fno-exceptions -fno-threadsafe-statics"
for f in $CORE/*.c; do avr-gcc $CF $DEFS $INC -c $f -o $B/core/$(basename $f).o; done
for f in $CORE/*.cpp; do avr-g++ $CXF $DEFS $INC -c $f -o $B/core/$(basename $f).o; done
for f in $CORE/*.S; do avr-gcc -x assembler-with-cpp -flto $DEFS $INC -c $f -o $B/core/$(basename $f).o; done
for f in $A2/*.cpp $FX/*.cpp $AT/*.cpp; do avr-g++ $CXF $DEFS $INC -c $f -o $B/lib/$(basename $f).o; done
avr-gcc-ar rcs $B/core.a $B/core/*.o
{ echo '#include <Arduino.h>'; cat $SK/$SK.ino; } > $B/sketch.cpp
avr-g++ $CXF $DEFS $INC -c $B/sketch.cpp -o $B/sketch.o
avr-gcc -w -Os -flto -fuse-linker-plugin -Wl,--gc-sections -mcall-prologues -mrelax -mmcu=atmega32u4 -o $B/$SK.elf $B/sketch.o $B/lib/*.o $B/core.a -lm
avr-objcopy -O ihex -R .eeprom $B/$SK.elf $B/$SK.hex
avr-size -A $B/$SK.elf | awk '/^.text|^.data|^.bss/{s[$1]=$2} END{printf "FLASH %d / 28672   RAM %d / 2560\n", s[".text"]+s[".data"], s[".data"]+s[".bss"]}'
