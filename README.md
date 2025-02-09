# attiny13-2wire-lights

Use an ATTINY13 to drive a simple two-wire two-group string of fairy lights.

## Overview

Battery acid ate the original controller for a set of simple two-wire two-group fairy lights we had, so I used an ATTINY13(A) to make a new one.
Previously the lights could dim, flash, and pulse in various way in two interleaved groups of discrete LEDs.
They would stay on for some number of hours, then stay off for many hours, then turn back on at about the same time of day as they were first turned on.

![Under text reading "simple two-wire two-group fairy lights" is depicted two LEDs connected in opposite polarities to two shared wires](two-wire-lights.svg)

The two interleaved groups of LEDs are controllable using only two wires by way of opposing polarities.
Each LED in both groups have one terminal connected to each wire, but those LEDs of one group has cathode and anode reversed relative to those the other group.
With no voltage difference between the wires, all the LEDs be dark.
Raise the voltage of one wire above the forward-bias voltage of the LEDs and one group will light up while the other stay dark (reversed-biased).
Reverse the voltage difference across the wires and the first group of LEDs will go dark as the second group light up.
Repeat this fast enough, say 4000 times a second, and to the human eye it appears that all the LEDs are shining bright.
Mutual or opposing dimming/flashing/pulsing of the two groups can be achieved by carefully adjusting the proportion of time spent in one of these three states.

## Circuit

Coming soon...

## Prerequisites

```sh
sudo apt install gcc-avr avr-libc
```

avr-gcc docs: <https://gcc.gnu.org/wiki/avr-gcc>

AVR-LibC docs: <https://avrdudes.github.io/avr-libc/avr-libc-user-manual/index.html>

AVR-LibC inline Assembler Cookbook: <https://avrdudes.github.io/avr-libc/avr-libc-user-manual/inline_asm.html>

## Build

Based on: <https://avrdudes.github.io/avr-libc/avr-libc-user-manual/group__demo__project.html>

```sh
# Compile
avr-gcc -g -Os -mmcu=attiny13a -c tiny_fairy.c
# Link
avr-gcc -g -mmcu=attiny13a -o tiny_fairy.elf tiny_fairy.o
# Convert to Hex file
avr-objcopy -j .text -j .data -O ihex tiny_fairy.elf tiny_fairy.hex
# All-in-one
avr-gcc -g -Os -mmcu=attiny13a -c tiny_fairy.c && avr-gcc -g -mmcu=attiny13a -o tiny_fairy.elf tiny_fairy.o && avr-objcopy -j .text -j .data -O ihex tiny_fairy.elf tiny_fairy.hex
```

### Optional outputs

```sh
# Optional: Disassembly to see what instructions will actually be run
avr-objdump -h -S tiny_fairy.elf > tiny_fairy.lst
# Optional: Map file to see the size of the code/data and what is pulled from where
avr-gcc -g -mmcu=attiny13a -Wl,-Map,tiny_fairy.map -o tiny_fairy.elf tiny_fairy.o
```

## Programing the ATTINY13 from Windows

Using Arduino Uno running ArduinoISP.ino (File>Examples>11.ArduinoISP) as programmer.

### Prerequisites

Download release from: <https://github.com/avrdudes/avrdude/releases/tag/v8.0>

### Wiring

Based on: <https://docs.arduino.cc/built-in-examples/arduino-isp/ArduinoISP/>
and ATTINY13A datasheet.

| Signal | UNO | ATTINY13A |
| ------ | --- | --------- |
| Reset  | 10  | 1 (PB5)   |
| MOSI   | 11  | 5 (PB0)   |
| MISO   | 12  | 6 (PB1)   |
| SCLK   | 13  | 7 (PB2)   |
| GND    | GND | 4 (GND)   |
| VCC    | 5V  | 8 (VCC)   |

### Programming

Based on: <https://electronics.stackexchange.com/questions/205055/using-avrdude-to-program-attiny-via-arduino-as-isp>

Options: <https://avrdudes.github.io/avrdude/8.0/avrdude_4.html#Option-Descriptions>

```cmd
avrdude-v8.0-windows-x64\avrdude.exe -p t13 -c avrisp -b 19200 -P com3 -e -U flash:w:tiny_fairy.hex
```

## Speculative sections

### Programming the ATTINY13 from Linux

Apparently avrdude doesn't work under Windows Subsystem for Linux (WSL), so programming from Linux has not yet been attempted.

You can probably install avrdude using:

```sh
sudo apt install avrdude
```

Perhaps the options used in the Windows will also work here.

### Calibration

Would try this if I had a supported programmer:

```sh
# Calibrate oscillator (only supported on STK500v2, AVRISP mkII, and JTAG ICE mkII hardware)
avrdude -p t13 -c avrispmkII -O
```

