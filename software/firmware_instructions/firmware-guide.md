# Firmware Guide

How to build, flash, and extend the firmware for the BEE 425 function generator (board rev v1.1). The firmware runs on a Raspberry Pi Pico 2 (RP2350) and is written in C with the Raspberry Pi Pico SDK.

The firmware does not generate the waveform itself. The AD9833 DDS chip makes the waveform on its own. The Pico only reads the controls, updates the display, and writes new settings to the AD9833 and the MCP42010 digital pot when something changes.

## Contents

1. [Setup](#setup)
2. [Build and Flash](#build-and-flash)
3. [Pin Map](#pin-map)
4. [Signal Chain](#signal-chain)
5. [AD9833 DDS](#ad9833-dds)
6. [MCP42010 Digital Pot](#mcp42010-digital-pot)
7. [Sharing the SPI Bus](#sharing-the-spi-bus)
8. [Amplitude and Offset Math](#amplitude-and-offset-math)
9. [Controls](#controls)
10. [OLED Display](#oled-display)
11. [Calibration](#calibration)
12. [Bring-Up Order](#bring-up-order)
13. [Troubleshooting](#troubleshooting)

## Setup

You need:

- Raspberry Pi Pico SDK **2.0 or newer**. Older SDKs do not support the Pico 2.
- The ARM GCC toolchain, CMake, and Ninja or Make.

The easiest way to get all of this is the **Raspberry Pi Pico extension for VS Code**. It installs the SDK, toolchain, and CMake for you. Open the `software/` folder with it and it will pick up the existing `CMakeLists.txt`.

If you set it up by hand instead, set `PICO_SDK_PATH` to your SDK folder before building.

The board is already set for the Pico 2 in `software/CMakeLists.txt`:

```cmake
set(PICO_BOARD pico2 CACHE STRING "Pico board")
```

When you add the OLED and buttons, add `hardware_i2c` to the link line:

```cmake
target_link_libraries(function_generator pico_stdlib hardware_spi hardware_i2c)
```

## Build and Flash

From the `software/` folder:

```
mkdir build
cd build
cmake ..
make
```

This produces `function_generator.uf2` in `build/`.

To flash:

1. Unplug the USB cable.
2. Hold the **BOOTSEL** button on the Pico.
3. Plug the USB cable back in, then release BOOTSEL.
4. The Pico shows up as a USB drive named `RP2350`. Drag `function_generator.uf2` onto it.
5. The Pico reboots and runs the new firmware.

The USB cable also powers the whole board, including the +/-12 V converter, so the analog section turns on while you flash. That is fine.

`printf` output goes over USB serial (`pico_enable_stdio_usb` is on). Open the serial port at any baud rate to see it.

## Pin Map

Checked against the v1.1 PCB.

| Signal | Pico pin | GPIO | Notes |
| --- | --- | --- | --- |
| OLED SDA | 6 | GP4 | I2C0 |
| OLED SCL | 7 | GP5 | I2C0 |
| Encoder CLK (A) | 14 | GP10 | Pull-up on |
| Encoder DT (B) | 15 | GP11 | Pull-up on |
| Encoder push (SW) | 16 | GP12 | Pull-up on, active low |
| Waveform button (WAVE1) | 19 | GP14 | Pull-up on, active low |
| Output on/off button (ON/OFF1) | 20 | GP15 | Pull-up on, active low |
| MCP42010 CS | 21 | GP16 | Active low |
| AD9833 FSYNC (CS) | 22 | GP17 | Active low |
| SPI SCK | 24 | GP18 | SPI0, shared by both chips |
| SPI TX (MOSI) | 25 | GP19 | SPI0, shared by both chips |
| Reset button (RESET1) | 30 | RUN | Hardware reset, no firmware needed |

There are **no external pull-up resistors** on the buttons or the encoder switch. Turn on the internal pull-ups:

```c
gpio_init(PIN);
gpio_set_dir(PIN, GPIO_IN);
gpio_pull_up(PIN);
```

Use these defines so every file agrees:

```c
#define PIN_OLED_SDA   4
#define PIN_OLED_SCL   5
#define PIN_ENC_A      10
#define PIN_ENC_B      11
#define PIN_ENC_SW     12
#define PIN_BTN_WAVE   14
#define PIN_BTN_OUT    15
#define PIN_POT_CS     16
#define PIN_DDS_FSYNC  17
#define PIN_SCK        18
#define PIN_MOSI       19
```

## Signal Chain

```
AD9833 --> MCP42010 pot 0 --> 10 uF coupling cap --> stage 1 (x4.3) --> stage 2 (x -4.55, adds offset) --> 51 ohm --> BNC
                                                                              ^
                                                     MCP42010 pot 1 ----------+
```

- **Pot 0 (amplitude):** a divider from the DDS output to AGND. Its wiper sets how much of the waveform goes on.
- **Coupling cap (C7):** removes the DDS DC level, so the signal is centered on 0 V.
- **Stage 1:** non-inverting, gain 1 + 3.3k / 1k = **4.3**.
- **Stage 2:** inverting summer. Gain for the signal is -15k / 3.3k = **-4.55**. It also adds the DC offset from pot 1 and a fixed bias from -12 V.
- **Total signal gain:** about **19.5**, and the output is **inverted**. You only notice the inversion on the square wave, and it does not matter for this project.

## AD9833 DDS

**SPI mode 2** (CPOL = 1, CPHA = 0), MSB first, 16-bit words. FSYNC goes low for each word.

The master clock on the module is **25 MHz**. The frequency register value is:

```
FREQREG = f_out x 2^28 / 25,000,000
```

That gives 0.093 Hz per step. For example, 1 kHz is 10737.

Control register bits used:

| Bit | Name | Value | Meaning |
| --- | --- | --- | --- |
| 13 | B28 | 0x2000 | Write the 28-bit frequency as two 14-bit halves |
| 8 | RESET | 0x0100 | Stop the output and hold the DAC at mid-scale |
| 5 | OPBITEN | 0x0020 | Square wave output |
| 3 | DIV2 | 0x0008 | With OPBITEN, square wave at the set frequency |
| 1 | MODE | 0x0002 | Triangle instead of sine |

Waveform bits:

| Waveform | Bits |
| --- | --- |
| Sine | 0 |
| Triangle | MODE |
| Square | OPBITEN or DIV2 |

`software/main.c` already has working functions for this: `dds_write_word()`, `dds_set_frequency()`, and `dds_set_output()`. Its pins (GP17, GP18, GP19) match the board.

Output levels to know:

- Sine and triangle are about **0.6 Vpp**.
- Square is a **logic-level signal, about 3.3 Vpp**. That is about 5.5 times bigger, so cap the amplitude pot much lower in square mode (see [Amplitude and Offset Math](#amplitude-and-offset-math)).

## MCP42010 Digital Pot

**SPI mode 0** (CPOL = 0, CPHA = 0), MSB first. Each write is **16 bits**: a command byte, then a data byte. Hold CS low for the full 16 bits.

| Command byte | Action |
| --- | --- |
| 0x11 | Write pot 0 (amplitude) |
| 0x12 | Write pot 1 (offset) |
| 0x13 | Write both pots |
| 0x21 / 0x22 / 0x23 | Shut down pot 0 / pot 1 / both |

The data byte is the wiper position, 0 to 255.

How each pot is wired on the board:

| Pot | Terminal A | Wiper | Terminal B | Job |
| --- | --- | --- | --- | --- |
| Pot 0 | DDS_OUT | POT_OUT | AGND | Amplitude |
| Pot 1 | +3V3 | POT_OFFSET | AGND | Offset voltage, 0 to 3.3 V |

The chip's SO pin is not connected, so you cannot read back the pot. Keep the last values in variables.

```c
static void pot_write(uint8_t pot, uint8_t value) {
    // pot = 0 for amplitude, 1 for offset
    uint8_t cmd[2] = { (uint8_t)(0x11 + pot), value };
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(PIN_POT_CS, 0);
    spi_write_blocking(spi0, cmd, 2);
    gpio_put(PIN_POT_CS, 1);
}
```

## Sharing the SPI Bus

Both chips use the same SCK and MOSI lines (GP18, GP19), with separate chip selects. Rules:

1. **Set both CS pins high at startup**, before the first transfer.
2. **Switch the SPI mode before each chip's transfer.** The AD9833 needs mode 2 and the MCP42010 needs mode 0. A chip ignores the bus while its CS is high.
3. **Keep the SPI clock at 1 MHz or slower.** The SPI lines cross the analog/digital ground split away from the tie point, so slower edges keep ringing and crosstalk low. 1 MHz is still far faster than this project needs.
4. **Only write when a setting changes.** While nothing is changing, the bus stays quiet and adds no noise to the analog section.

```c
spi_init(spi0, 1000 * 1000);
gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

gpio_init(PIN_DDS_FSYNC); gpio_set_dir(PIN_DDS_FSYNC, GPIO_OUT); gpio_put(PIN_DDS_FSYNC, 1);
gpio_init(PIN_POT_CS);    gpio_set_dir(PIN_POT_CS, GPIO_OUT);    gpio_put(PIN_POT_CS, 1);
```

In `dds_write_word()`, set the format back to mode 2 before writing:

```c
spi_set_format(spi0, 8, SPI_CPOL_1, SPI_CPHA_0, SPI_MSB_FIRST);
```

## Amplitude and Offset Math

### Amplitude

```
Vpp_out = Vpp_dds x (code / 256) x 19.5
```

| Waveform | Vpp_dds | Code for 10 Vpp (+/-5 V) | Output change per code step |
| --- | --- | --- | --- |
| Sine / triangle | about 0.6 V | about 218 | about 0.046 Vpp |
| Square | about 3.3 V | about 40 | about 0.25 Vpp |

These are starting points. Measure the real numbers during [Calibration](#calibration).

To set a target amplitude:

```c
// Vpp_dds is about 0.6 for sine/triangle and about 3.3 for square
uint8_t amp_code(float vpp_target, float vpp_dds) {
    float code = vpp_target * 256.0f / (vpp_dds * 19.5f);
    if (code > 255.0f) code = 255.0f;
    if (code < 0.0f)   code = 0.0f;
    return (uint8_t)(code + 0.5f);
}
```

### Offset

Stage 2 adds the offset like this:

```
V_offset_out = 5.45 - 3.19 x V_pot1
V_pot1       = 3.3 x code / 256
```

| Pot 1 code | V_pot1 | Output offset |
| --- | --- | --- |
| 0 | 0 V | about +5.45 V |
| about 133 | about 1.71 V | about 0 V |
| 255 | about 3.29 V | about -5.0 V |

Higher codes push the output **down**. Each step is about 0.04 V.

```c
uint8_t offset_code(float v_offset) {
    float v_pot = (5.45f - v_offset) / 3.19f;
    float code = v_pot * 256.0f / 3.3f;
    if (code > 255.0f) code = 255.0f;
    if (code < 0.0f)   code = 0.0f;
    return (uint8_t)(code + 0.5f);
}
```

### Limit the Total

The spec output range is +/-5 V. Keep the waveform plus the offset inside it:

```
|offset| + Vpp / 2 <= 5 V
```

When the user raises one setting, clamp the other so the sum stays inside the limit. The op-amps run on +/-12 V, so the output would not clip until about +/-10 V, but the spec is +/-5 V.

### Output Off

For the on/off button:

- **Off:** set pot 0 to code 0, then set the AD9833 RESET bit. The output then sits at the offset voltage only. Also set pot 1 to the 0 V offset code if you want a true 0 V output.
- **On:** clear RESET, then restore pot 0 and pot 1.

## Controls

### Rotary Encoder (KY-040)

- CLK = GP10, DT = GP11, push = GP12.
- Most KY-040 modules have 10k pull-ups on CLK and DT, but turn on the internal pull-ups anyway.
- Read both pins on any edge of either one (GPIO interrupt), and use a lookup table. A table rejects contact bounce and is more reliable than "read DT on the CLK edge."

```c
static volatile int32_t enc_count = 0;
static uint8_t enc_state = 0;
static const int8_t enc_table[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};

static void enc_irq(uint gpio, uint32_t events) {
    enc_state = (uint8_t)((enc_state << 2) | (gpio_get(PIN_ENC_A) << 1) | gpio_get(PIN_ENC_B));
    enc_count += enc_table[enc_state & 0x0f];
}

// in setup:
gpio_set_irq_enabled_with_callback(PIN_ENC_A, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &enc_irq);
gpio_set_irq_enabled(PIN_ENC_B, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
```

Most KY-040 encoders give **4 counts per click**, so divide `enc_count` by 4. If it turns the wrong way, swap the signs in the table or swap A and B in software.

The Pico SDK allows only one GPIO interrupt callback, so the buttons have to share it. Check the `gpio` argument to see which pin fired, or poll the buttons in the main loop instead.

### Buttons

- All three buttons pull to GND when pressed. They read **0 when pressed** and 1 when released.
- Debounce in software: accept a press only after the pin has read 0 for about **20 ms**.
- Suggested use:
  - **WAVE1 (GP14):** cycle sine, triangle, square.
  - **ON/OFF1 (GP15):** output on or off.
  - **Encoder push (GP12):** cycle which setting the knob changes (frequency, amplitude, offset), or the frequency step size (1 Hz, 10 Hz, 100 Hz, 1 kHz, 10 kHz).
- **RESET1** is wired to the Pico's RUN pin and resets the Pico in hardware. No code needed.

## OLED Display

- 0.96 in SSD1306, 128 x 64, I2C.
- I2C0 on GP4 (SDA) and GP5 (SCL), 400 kHz.
- Address is usually **0x3C**. Some modules use 0x3D, so try that if nothing shows.
- The module has its own I2C pull-ups.

The Pico SDK has no built-in SSD1306 driver. The `i2c/ssd1306_i2c` example in the official `pico-examples` repo works well as a starting point. Copy its driver functions and font into the project.

```c
i2c_init(i2c0, 400 * 1000);
gpio_set_function(PIN_OLED_SDA, GPIO_FUNC_I2C);
gpio_set_function(PIN_OLED_SCL, GPIO_FUNC_I2C);
```

Only redraw the screen when a setting changes. Redrawing all the time is wasted I2C traffic and adds digital noise near the analog section.

Suggested screen layout:

```
SINE        OUT ON
1000.0 Hz
Amp  5.00 Vpp
Ofs  +0.00 V
> step 100 Hz
```

## Calibration

The parts have tolerances, so measure the real board once and put the numbers in the code.

1. **Offset zero.** Turn the output off (pot 0 at code 0). With a multimeter on the BNC, step pot 1 until the output reads closest to 0 V. Save that code as `OFFSET_ZERO_CODE`. It should be near 133.
2. **Offset slope.** Read the output at pot 1 codes 0 and 255. Save `volts per code = (V_at_0 - V_at_255) / 255`. It should be near 0.041 V.
3. **Sine/triangle gain.** Set a 1 kHz sine, offset at zero, pot 0 at 128. Measure Vpp on a scope. Save `SINE_VPP_PER_CODE = measured Vpp / 128`. It should be near 0.046.
4. **Square gain.** Same as step 3 with a square wave at pot 0 code 20. Save `SQUARE_VPP_PER_CODE = measured Vpp / 20`. It should be near 0.25.

Use these saved values in place of the default numbers in `amp_code()` and `offset_code()`.

## Bring-Up Order

Test one piece at a time. Each step builds on the last.

1. **Blink.** Flash a blink program to prove the Pico and USB work.
2. **DDS.** Run the existing `main.c` (1 kHz sine). Probe the **DDS_OUT** test point. You should see about 0.6 Vpp centered around 0.3 V.
3. **Amplitude pot.** Write pot 0 to 128. Probe **POT_OUT**. You should see about half of DDS_OUT.
4. **Stage 1.** Probe **STAGE1_OUT**. You should see about 4.3 times POT_OUT, centered on 0 V.
5. **Offset pot.** Write pot 1 across its range. Probe **POT_OFFSET** (0 to 3.3 V) and the BNC (about +5.45 V down to -5 V).
6. **Full output.** Check the BNC across waveforms and frequencies, then run [Calibration](#calibration).
7. **UI.** Add the encoder, buttons, and OLED last.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No waveform at DDS_OUT | FSYNC idle high, SPI mode 2, and RESET cleared after setting the frequency |
| Wrong frequency | MCLK in the math must be 25 MHz, and the B28 bit must be set |
| Pot does nothing | SPI mode switched to 0 before the pot write, CS held low for all 16 bits |
| Output stuck near +5.4 V | Pot 1 at code 0. Set it to the 0 V offset code |
| Square wave clipped or flat-topped | Pot 0 code too high for square. Cap it at about 40 |
| Encoder skips or jumps | Use the table decoder, read both pins on both edges, pull-ups on |
| Button triggers several times | Add the 20 ms debounce |
| Blank OLED | Try address 0x3D, and check SDA/SCL are on GP4/GP5 |
| Noise on the output when turning the knob | Normal SPI activity. If it is bad, slow the SPI clock further |
| No +/-12 V | Check the DCP010512DBP on the +12V and -12V test points before debugging firmware |
