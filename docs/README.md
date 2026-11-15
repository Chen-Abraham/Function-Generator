# function generator

a microcontroller-based function generator built for b ee 425 at uw bothell (fall 2026). a raspberry pi pico 2 drives an AD9833 dds chip, a digital potentiometer sets amplitude and dc offset, and a two-stage TL072 output amplifier on +/-12 V rails delivers up to +/-5 V at a BNC jack. everything sits on a custom 2-layer pcb and runs from a usb wall adapter, no laptop needed.

<img width="1028" height="541" alt="Screenshot 2026-11-12 at 9 46 22 PM" src="https://github.com/user-attachments/assets/604a5644-283d-4c9c-9a50-8e737255e969" />

## pcb

| top (3d render) | bottom (3d render) |
| --- | --- |
| ![pcb top](docs/images/pcb-3d-top.png) | ![pcb bottom](docs/images/pcb-3d-bottom.png) |

![pcb layout](docs/images/pcb-layout.png)

## specs

| requirement | how it's met |
| --- | --- |
| sine, triangle, square | AD9833 dds, selected in firmware |
| 10 Hz to 100 kHz | 25 MHz dds clock, 28-bit tuning word (~0.1 Hz steps) |
| output 0 to +/-5 V | MCP42010 digital pot + TL072 gain stages (~19.5 x) on +/-12 V |
| dc offset (extra credit) | second MCP42010 channel summed into the output stage |
| display | 0.96 in. SSD1306 oled (i2c) |
| controls | KY-040 rotary encoder + waveform and output on/off buttons |
| runs without a laptop | 5 V usb wall adapter into the pico |
| custom pcb | 2-layer, 135 x 72 mm, all parts on top |

## how it works

```
encoder / buttons --> pico 2 --spi--> AD9833 --> MCP42010 (amplitude) --> ac couple
                         |                                                   |
                         +--i2c--> oled          TL072 stage 1 (x4.3) <-------+
                                                       |
                         MCP42010 (offset) --> TL072 stage 2 (x4.5, sums offset) --> 51 ohm --> BNC

usb 5 V --> pico (3.3 V for logic) --> DCP010512DBP (+/-12 V for the TL072)
```

1. the pico only writes registers over spi when a setting changes. the AD9833 generates the waveform on its own, so there is no real-time code in the signal path.
2. the AD9833 puts out about 0.6 Vpp. channel A of the MCP42010 attenuates it to set amplitude.
3. a 10 uF coupling cap removes the dds dc level, then TL072 stage 1 (non-inverting, x4.3) and stage 2 (inverting summer, x4.5) bring it up to +/-5 V. channel B of the pot feeds a dc level into stage 2 for offset.
4. a 51 ohm series resistor protects the op-amp and drives the BNC.

## hardware

| ref | part | job |
| --- | --- | --- |
| A1 | raspberry pi pico 2 (RP2350) | controller, socketed on headers |
| AD1 | AD9833 module (GY-9833, 25 MHz) | waveform generation |
| U1 | MCP42010 (dip-14) | dual 10k digital pot: amplitude + offset |
| U2 | TL072 (dip-8) | two output gain stages |
| U3 | DCP010512DBP | isolated 5 V to +/-12 V converter |
| J1 | TE 5227161-9 | right-angle BNC output |
| J2 | SSD1306 oled module | display |
| J3 | KY-040 module | rotary encoder with push switch |
| SW1 / SW2 / SW3 | 6 mm tactile | reset / waveform / output on/off |

design notes:

- **power:** the DCP010512DBP is unregulated, so 2.7k preload resistors keep it above 10% load (below that its output can rise toward double). ferrite beads and 1 uF + 10 uF caps filter each rail.
- **grounds:** analog (AGND) and digital (GND) pours are split on both layers and tied at one point by R1 (0 ohm) next to the AD9833 header. stitching vias join the top and bottom pours on each side.
- **why +/-12 V:** a TL072 needs ~1.5 V of headroom to each rail, so +/-5 V rails can't produce a full +/-5 V output.

### pico pinout

| signal | pico pin | gpio |
| --- | --- | --- |
| oled sda / scl | 6 / 7 | GP4 / GP5 (i2c0) |
| encoder a / b / push | 14 / 15 / 16 | GP10 / GP11 / GP12 |
| waveform button | 19 | GP14 |
| output on/off button | 20 | GP15 |
| pot cs (MCP42010) | 21 | GP16 |
| dds fsync (AD9833) | 22 | GP17 |
| spi sck | 24 | GP18 (spi0) |
| spi tx | 25 | GP19 (spi0) |
| reset | 30 | RUN |
| 5 V out to converter | 40 | VBUS |

the buttons have no external pull-ups. enable the pico's internal pull-ups in firmware (`gpio_pull_up()`), including the encoder push switch.

### test points

| tp | net | what to expect |
| --- | --- | --- |
| TP1 / TP2 | +5V / +3V3 | 5 V / 3.3 V |
| TP3 / TP4 | +12V / -12V | ~+/-12 to 13 V |
| TP5 / TP6 | GND / AGND | ground clips (use AGND for analog probing) |
| TP7 | DDS_OUT | raw AD9833 output, ~0.6 Vpp |
| TP8 | POT_OUT | after the amplitude pot |
| TP9 | STAGE1_OUT | ~4.3 x TP8 |
| TP10 | POT_OFFSET | dc offset level, 0 to 3.3 V |

### bring-up order

1. unpowered: check +5V, +3V3, +12V and -12V are not shorted to ground.
2. first power-up with only the converter section populated (TL072 and MCP42010 out of their sockets). check TP1 and TP3/TP4.
3. insert the pico, check TP2, flash a blink test.
4. add the AD9833 module and look for a sine at TP7.
5. insert the MCP42010 and TL072, then follow the signal TP8 -> TP9 -> BNC.
6. calibrate the offset zero and amplitude in firmware.

## firmware

c, built with the raspberry pi pico sdk (`PICO_BOARD=pico2`).

```
cd software
mkdir build && cd build
cmake ..
make
```

copy `function_generator.uf2` onto the pico while holding BOOTSEL. the current test firmware sets the AD9833 to a 1 kHz sine.

firmware notes:

- AD9833: spi mode 2 (cpol 1, cpha 0), 25 MHz mclk in the frequency math.
- MCP42010: spi mode 0, so switch spi format before each chip's transfer.
- square wave: the AD9833 square output is logic level (~3.3 Vpp), so cap the amplitude pot in square mode to keep the output off the rails.

## repo layout

```
hardware/      kicad project (schematic + pcb)
software/      pico firmware (c, pico sdk)
calculations/  design calculations
prototype/     breadboard prototype
mechanical/    enclosure cad
docs/          images and documentation
```

## team

- hardware: cris r. martinez hernandez
- firmware: abraham ([@Chen-Abraham](https://github.com/Chen-Abraham))

## license

mit, see [LICENSE](LICENSE).
