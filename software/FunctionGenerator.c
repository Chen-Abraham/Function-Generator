// BEE 425 Function Generator - first hardware test
// Original starter example: Pico 2 -> AD9833 DDS, 1 kHz sine wave.
// Pin assignments are EXAMPLES; update to match Digital.kicad_sch.
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define DDS_SPI spi0
#define PIN_SCK 18       // SPI0 SCK
#define PIN_MOSI 19      // SPI0 TX (MOSI)
#define PIN_FSYNC 17     // AD9833 FSYNC (manual chip select)
#define DDS_MCLK_HZ 25000000.0

// AD9833 control words. B28=bit 13; RESET=bit 8;
// OPBITEN=bit 5; DIV2=bit 3; MODE=bit 1.
#define DDS_B28 0x2000u
#define DDS_RESET 0x0100u
#define DDS_OPBITEN 0x0020u
#define DDS_DIV2 0x0008u
#define DDS_MODE 0x0002u

typedef enum { WAVE_SINE, WAVE_TRIANGLE, WAVE_SQUARE } waveform_t;

static void dds_write_word(uint16_t word) {
    uint8_t bytes[2] = {(uint8_t)(word >> 8), (uint8_t)word};
    gpio_put(PIN_FSYNC, 0);
    sleep_us(1);
    spi_write_blocking(DDS_SPI, bytes, 2);
    sleep_us(1);
    gpio_put(PIN_FSYNC, 1);
}

static uint16_t dds_wave_bits(waveform_t wave) {
    switch (wave) {
        case WAVE_TRIANGLE: return DDS_MODE;
        case WAVE_SQUARE:   return DDS_OPBITEN | DDS_DIV2;
        case WAVE_SINE:
        default:            return 0;
    }
}

static void dds_set_output(bool enable, waveform_t wave) {
    // RESET suppresses the waveform but holds the DAC at midscale;
    // it is NOT a guaranteed zero-volt output or physical disconnect.
    uint16_t ctrl = DDS_B28 | dds_wave_bits(wave);
    if (!enable) ctrl |= DDS_RESET;
    dds_write_word(ctrl);
}

static void dds_set_frequency(double hz, waveform_t wave) {
    if (hz < 10.0) hz = 10.0;
    if (hz > 100000.0) hz = 100000.0;
    uint32_t tuning = (uint32_t)(hz * 268435456.0 / DDS_MCLK_HZ + 0.5);
    dds_write_word(DDS_B28 | DDS_RESET | dds_wave_bits(wave));
    dds_write_word(0x4000u | (uint16_t)(tuning & 0x3fffu));
    dds_write_word(0x4000u | (uint16_t)((tuning >> 14) & 0x3fffu));
    dds_write_word(0xc000u); // PHASE0 = 0
    dds_set_output(true, wave);
}

int main(void) {
    stdio_init_all();
    gpio_init(PIN_FSYNC);
    gpio_set_dir(PIN_FSYNC, GPIO_OUT);
    gpio_put(PIN_FSYNC, 1);

    spi_init(DDS_SPI, 1000 * 1000); // Start conservatively at 1 MHz
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    // AD9833 uses falling SCLK edge; SPI mode 2 (CPOL=1, CPHA=0).
    spi_set_format(DDS_SPI, 8, SPI_CPOL_1, SPI_CPHA_0, SPI_MSB_FIRST);
    sleep_ms(20);

    dds_set_frequency(1000.0, WAVE_SINE);
    // The AD9833 continues generating independently of the Pico loop.
    while (true) {
        sleep_ms(1000);
    }
}
