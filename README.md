# Function Generator

A microcontroller-based function generator built for BEE 425 at UW Bothell (Fall 2026). A Raspberry Pi Pico 2 controls an AD9833 DDS chip, and a two-stage op-amp output amplifier delivers sine, triangle, and square waves from 10 Hz to 100 kHz at up to +/-5 V. Everything runs on a custom 2-layer PCB powered from a USB wall adapter.
<img width="1277" height="607" alt="Screenshot 2026-11-16 at 9 58 31 PM" src="https://github.com/user-attachments/assets/7ff39d4d-00aa-4fa7-a449-1f045bc8d442" />

## Features

- Sine, triangle, and square waveforms
- 10 Hz to 100 kHz frequency range (0.1 Hz resolution)
- Adjustable amplitude, 0 to +/-5 V
- Adjustable DC offset
- OLED display with rotary encoder and push-button controls
- Runs standalone from a 5 V USB wall adapter

## How It Works

The Pico 2 reads the encoder and buttons, updates the OLED, and programs the AD9833 over SPI. The AD9833 generates the waveform on its own, so the firmware only writes registers when a setting changes. An MCP42010 digital potentiometer sets the amplitude and DC offset, and two TL072 gain stages running on +/-12 V rails bring the signal up to +/-5 V at the BNC output.

| Block | Part | Function |
| --- | --- | --- |
| Controller | Raspberry Pi Pico 2 (RP2350) | User interface and SPI control |
| Waveform | AD9833 DDS module (25 MHz) | Generates sine, triangle, square |
| Amplitude / offset | MCP42010 dual digital pot | 256-step amplitude and offset control |
| Output stage | TL072 dual op-amp | Two gain stages, ~19.5x total |
| Power | DCP010512DBP | Isolated 5 V to +/-12 V for the op-amps |
| Display | SSD1306 0.96" OLED (I2C) | Shows waveform, frequency, amplitude |
| Controls | KY-040 encoder + 3 buttons | Adjust settings, waveform, on/off, reset |
| Output | BNC jack, 51 ohm series | Signal output |

## PCB

2-layer board, 135 x 72 mm, all components on the top side. Analog and digital ground pours are separated and joined at a single point next to the AD9833.

<img width="952" height="574" alt="Screenshot 2026-11-16 at 10 12 11 PM" src="https://github.com/user-attachments/assets/7c8fd9c0-b5e9-4b69-87b0-0c0166042d5e" />

<img width="1255" height="609" alt="Screenshot 2026-11-16 at 9 58 42 PM" src="https://github.com/user-attachments/assets/e7c37efa-eeb0-4a1c-b27f-1e411825d1d8" />


## Pico Pinout

| Signal | Pico Pin | GPIO |
| --- | --- | --- |
| OLED SDA / SCL | 6 / 7 | GP4 / GP5 |
| Encoder A / B / Switch | 14 / 15 / 16 | GP10 / GP11 / GP12 |
| Waveform button | 19 | GP14 |
| Output on/off button | 20 | GP15 |
| Digital pot CS | 21 | GP16 |
| AD9833 FSYNC | 22 | GP17 |
| SPI SCK | 24 | GP18 |
| SPI TX | 25 | GP19 |
| Reset | 30 | RUN |

## Test Points

| Test Point | Signal | Expected |
| --- | --- | --- |
| TP1 / TP2 | +5V / +3V3 | 5 V / 3.3 V |
| TP3 / TP4 | +12V / -12V | ~+/-12 V |
| TP5 / TP6 | GND / AGND | Ground |
| TP7 | DDS output | ~0.6 Vpp |
| TP8 | Amplitude pot output | Scaled DDS signal |
| TP9 | Stage 1 output | ~4.3x TP8 |
| TP10 | Offset level | 0 to 3.3 V DC |

## Building the Firmware

Requires the Raspberry Pi Pico SDK.

```
cd software
mkdir build && cd build
cmake ..
make
```

Hold BOOTSEL while plugging in the Pico, then copy `function_generator.uf2` onto it.

## Repository Structure

```
hardware/      KiCad schematic and PCB
software/      Pico firmware (C, Pico SDK)
calculations/  Design calculations
prototype/     Breadboard prototype
mechanical/    Enclosure CAD
docs/          Documentation and images
```

## Team

- **Hardware:** Cris R. Martinez Hernandez
- **Firmware:** Abraham Yin Chen

## License

MIT License. See [LICENSE](LICENSE).
