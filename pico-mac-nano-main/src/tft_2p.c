/* Functions to output to 2.0" 480x640 (Portrait) SPI & RBG565 TFT panel:
 *
 * Copyright 2025 Nick Gillard
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include "pico/time.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#if TFT_ST7789_SPI
#include "hardware/spi.h"
#endif
#include "hw.h"
#include "ws2812.h" /* Needed for neo-pixel LED */
#include "tft_2p.h"

#if TFT_ST7789_SPI
#ifndef TFT_WIDTH
#define TFT_WIDTH 240
#endif
#ifndef TFT_HEIGHT
#define TFT_HEIGHT 320
#endif
#ifndef TFT_SPI_MHZ
#define TFT_SPI_MHZ 40
#endif
#ifndef TFT_MADCTL
#define TFT_MADCTL 0x00
#endif
#ifndef TFT_X_OFFSET
#define TFT_X_OFFSET 0
#endif
#ifndef TFT_Y_OFFSET
#define TFT_Y_OFFSET 0
#endif

#define FB_WIDTH 512
#define FB_HEIGHT 342

static inline void tft_spi_select(bool select) {
    gpio_put(TFT_SPI_CS, select ? 0 : 1);
}

static void tft_spi_write(const uint8_t *data, size_t len) {
    spi_write_blocking(spi0, data, len);
}

static void tft_write_command(uint8_t command) {
    gpio_put(TFT_DC, 0);
    tft_spi_select(true);
    tft_spi_write(&command, 1);
    tft_spi_select(false);
}

static void tft_write_data(const uint8_t *data, size_t len) {
    gpio_put(TFT_DC, 1);
    tft_spi_select(true);
    tft_spi_write(data, len);
    tft_spi_select(false);
}

static void tft_write_data_u8(uint8_t value) {
    tft_write_data(&value, 1);
}

static void tft_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    tft_write_command(0x2A);
    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    tft_write_data(data, sizeof(data));

    tft_write_command(0x2B);
    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    tft_write_data(data, sizeof(data));
}

static void tft_begin_pixels(void) {
    uint8_t ramwr = 0x2C;

    gpio_put(TFT_DC, 0);
    tft_spi_select(true);
    tft_spi_write(&ramwr, 1);
    gpio_put(TFT_DC, 1);
}

static void tft_end_pixels(void) {
    tft_spi_select(false);
}

static uint16_t st7789_sample_pixel(const uint32_t *framebuffer, uint32_t src_x, uint32_t src_y, uint32_t src_stride_words) {
    const uint32_t *line = framebuffer + (src_y * src_stride_words);
    uint32_t word = line[src_x >> 5];
    uint32_t bit = 31 - (src_x & 31);
    bool pixel_on = (word >> bit) & 0x1;
    return pixel_on ? 0xFFFF : 0x0000;
}

void st7789_spi_render_frame(const uint32_t *framebuffer) {
    static uint8_t line_bytes[TFT_WIDTH * 2];
    const uint32_t src_stride_words = FB_WIDTH / 32;

    uint32_t active_w = TFT_WIDTH;
    uint32_t active_h = (TFT_WIDTH * FB_HEIGHT) / FB_WIDTH;
    if (active_h > TFT_HEIGHT) {
        active_h = TFT_HEIGHT;
        active_w = (TFT_HEIGHT * FB_WIDTH) / FB_HEIGHT;
    }

    uint32_t x_pad = (TFT_WIDTH - active_w) / 2;
    uint32_t y_pad = (TFT_HEIGHT - active_h) / 2;

    tft_set_addr_window(TFT_X_OFFSET, TFT_Y_OFFSET,
                        TFT_X_OFFSET + TFT_WIDTH - 1, TFT_Y_OFFSET + TFT_HEIGHT - 1);
    tft_begin_pixels();

    for (uint32_t y = 0; y < TFT_HEIGHT; y++) {
        memset(line_bytes, 0, sizeof(line_bytes));

        if (y >= y_pad && y < (y_pad + active_h)) {
            uint32_t src_y = ((y - y_pad) * FB_HEIGHT) / active_h;
            for (uint32_t x = 0; x < active_w; x++) {
                uint32_t src_x = (x * FB_WIDTH) / active_w;
                uint16_t color = st7789_sample_pixel(framebuffer, src_x, src_y, src_stride_words);
                uint32_t out_x = x + x_pad;
                line_bytes[out_x * 2] = color >> 8;
                line_bytes[out_x * 2 + 1] = color & 0xFF;
            }
        }

        tft_spi_write(line_bytes, sizeof(line_bytes));
    }

    tft_end_pixels();
}
#endif

// TFT SPI Write command
static void tft_write_com(unsigned char comm) {
	static int i;

    put_pixel_red(1);
    gpio_put(TFT_SPI_CS, 0); // Chip select enabled
    gpio_put(TFT_SPI_MOSI, 0);
    gpio_put(TFT_SPI_CLK, 0);
    sleep_us(TFT_SPI_PAUSE);
    gpio_put(TFT_SPI_CLK, 1);

    for(i=0;i<8;i++) {
        if(comm & 0x80) {
            gpio_put(TFT_SPI_MOSI, 1);
        } else {
            gpio_put(TFT_SPI_MOSI, 0);
        }
        gpio_put(TFT_SPI_CLK, 0);
        sleep_us(TFT_SPI_PAUSE);
        gpio_put(TFT_SPI_CLK, 1);
        comm<<=1;
    }
    gpio_put(TFT_SPI_CS, 1); // Chip select disabled
    put_pixel_red(0);
}

// SPI Write Data
static void tft_write_dat(unsigned char datt) {
    static int j;

    put_pixel_red(1);
    gpio_put(TFT_SPI_CS, 0); // Chip select enabled

    // First send a 1 for bit 9
    gpio_put(TFT_SPI_MOSI, 1);
    gpio_put(TFT_SPI_CLK, 0);
    sleep_us(TFT_SPI_PAUSE);
    gpio_put(TFT_SPI_CLK, 1);

    for(j=0;j<8;j++) {
        if(datt & 0x80) { // true if left most bit is a 1. Bitwisecompare of datt with binary 10000000
            gpio_put(TFT_SPI_MOSI, 1);
        } else {
            gpio_put(TFT_SPI_MOSI, 0);
        }
        gpio_put(TFT_SPI_CLK, 0);
        sleep_us(TFT_SPI_PAUSE);
        gpio_put(TFT_SPI_CLK, 1);
        datt<<=1; // Bit shift left 1
    }

    gpio_put(TFT_SPI_CS, 1); // Chip select disabled
    put_pixel_red(0);
}

void tft_init() {
#if TFT_ST7789_SPI
    gpio_init(TFT_RESET);
    gpio_set_dir(TFT_RESET, GPIO_OUT);
    gpio_init(TFT_SPI_CS);
    gpio_set_dir(TFT_SPI_CS, GPIO_OUT);
    gpio_put(TFT_SPI_CS, 1);
    gpio_init(TFT_DC);
    gpio_set_dir(TFT_DC, GPIO_OUT);
    gpio_init(TFT_SPI_CLK);
    gpio_set_function(TFT_SPI_CLK, GPIO_FUNC_SPI);
    gpio_init(TFT_SPI_MOSI);
    gpio_set_function(TFT_SPI_MOSI, GPIO_FUNC_SPI);

    spi_init(spi0, TFT_SPI_MHZ * 1000 * 1000);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_put(TFT_RESET, 1);
    sleep_ms(5);
    gpio_put(TFT_RESET, 0);
    sleep_ms(20);
    gpio_put(TFT_RESET, 1);
    sleep_ms(120);

    tft_write_command(0x01);
    sleep_ms(120);

    tft_write_command(0x11);
    sleep_ms(120);

    tft_write_command(0x36);
    tft_write_data_u8(TFT_MADCTL);

    tft_write_command(0x3A);
    tft_write_data_u8(0x55);

    tft_write_command(0xB2);
    tft_write_data_u8(0x0C);
    tft_write_data_u8(0x0C);
    tft_write_data_u8(0x00);
    tft_write_data_u8(0x33);
    tft_write_data_u8(0x33);

    tft_write_command(0xB7);
    tft_write_data_u8(0x35);

    tft_write_command(0xBB);
    tft_write_data_u8(0x1A);

    tft_write_command(0xC0);
    tft_write_data_u8(0x2C);

    tft_write_command(0xC2);
    tft_write_data_u8(0x01);

    tft_write_command(0xC3);
    tft_write_data_u8(0x0B);

    tft_write_command(0xC4);
    tft_write_data_u8(0x20);

    tft_write_command(0xC6);
    tft_write_data_u8(0x0F);

    tft_write_command(0xD0);
    tft_write_data_u8(0xA4);
    tft_write_data_u8(0xA1);

    tft_write_command(0x21);

    tft_set_addr_window(TFT_X_OFFSET, TFT_Y_OFFSET,
                        TFT_X_OFFSET + TFT_WIDTH - 1, TFT_Y_OFFSET + TFT_HEIGHT - 1);

    tft_write_command(0x13);
    tft_write_command(0x29);
    sleep_ms(20);
#else
    gpio_init(TFT_RESET);
    gpio_set_dir(TFT_RESET, GPIO_OUT);

    // Initialize SPI pins
    gpio_init(TFT_SPI_CS);
    gpio_set_dir(TFT_SPI_CS, GPIO_OUT);
    gpio_put(TFT_SPI_CS, 1);// Initialize TFT CS pin high
    gpio_init(TFT_SPI_CLK);
    gpio_set_dir(TFT_SPI_CLK, GPIO_OUT);
    gpio_init(TFT_SPI_MOSI);
    gpio_set_dir(TFT_SPI_MOSI, GPIO_OUT);

    // sleep_ms(5);

    gpio_put(TFT_RESET, 1); // Set TFT Reset pin high
    sleep_ms(1);
    gpio_put(TFT_RESET, 0);  // Set TFT Reset pin low
    sleep_ms(1);
    gpio_put(TFT_RESET, 1);  // Set TFT Reset pin high
    sleep_ms(1);

    tft_write_com(0xFF);    // Command2 BKx Selection - 12.3.1
    tft_write_dat(0x77);    // 
    tft_write_dat(0x01);
    tft_write_dat(0x00);
    tft_write_dat(0x00);
    tft_write_dat(0x13);

    tft_write_com(0xEF);    // ??
    tft_write_dat(0x08);

    tft_write_com(0xFF);    // Command2 BKx Selection - 12.3.1
    tft_write_dat(0x77);
    tft_write_dat(0x01);
    tft_write_dat(0x00);
    tft_write_dat(0x00);
    tft_write_dat(0x10);

    tft_write_com(0xC0);    // Command 2 BK0: LNESET (C0h/C000h): Display Line Setting - 12.3.2.7
    tft_write_dat(0x4F);
    tft_write_dat(0x00);

    tft_write_com(0xC1);    // Command 2 BK0: PORCTRL (C1h/C100h):Porch Control - 12.3.2.8
    tft_write_dat(0x11);    // VBP[7:0]: Back-Porch Vertical line setting for display.
    tft_write_dat(0x0C);    // VFP[7:0]: Front-Porch Vertical line setting for display.

    tft_write_com(0xC2);    // Command 2 BK0: INVSET (C2h/C200h):Inversion selection & Frame Rate Control - 12.3.2.9
    tft_write_dat(0x07);
    tft_write_dat(0x0A);

    tft_write_com(0xC3);    // Command 2 BK0: RGBCTRL (C3h/C300h):RGB control - 12.3.2.10
    tft_write_dat(0x83);    // DE/HV - - - VSP HSP DP EP : VS Polarity, HS Pol, Dot/pixel Clk Pol, Enable Pin Pol. 0x83 = 10000011 VS & HS active when low, DP input on falling edge, EP Data written when enable is 0.
    tft_write_dat(0x33);    // HBP_HVRGB[7:0]. 0x33 = 51
    tft_write_dat(0x1b);    // VBP_HVRGB[7:0]. 0x1b = 27


    tft_write_com(0xCC);    // ??
    tft_write_dat(0x10);

    tft_write_com(0xB0);    // Command 2 BK0: PVGAMCTRL (B0h/B000h): Positive Voltage Gamma Control - 12.3.2.1
    tft_write_dat(0x00);
    tft_write_dat(0x0F);
    tft_write_dat(0x18);
    tft_write_dat(0x0D);
    tft_write_dat(0x12);
    tft_write_dat(0x07);
    tft_write_dat(0x05);
    tft_write_dat(0x08);
    tft_write_dat(0x07);
    tft_write_dat(0x21);
    tft_write_dat(0x03);
    tft_write_dat(0x10);
    tft_write_dat(0x0F);
    tft_write_dat(0x26);
    tft_write_dat(0x2F);
    tft_write_dat(0x1F);

    tft_write_com(0xB1);    // NVGAMCTRL (B1h/B100h): Negative Voltage Gamma Control - 12.3.2.2
    tft_write_dat(0x00);
    tft_write_dat(0x1B);
    tft_write_dat(0x20);
    tft_write_dat(0x0C);
    tft_write_dat(0x0E);
    tft_write_dat(0x03);
    tft_write_dat(0x08);
    tft_write_dat(0x08);
    tft_write_dat(0x08);
    tft_write_dat(0x22);
    tft_write_dat(0x05);
    tft_write_dat(0x11);
    tft_write_dat(0x0F);
    tft_write_dat(0x2A);
    tft_write_dat(0x32);
    tft_write_dat(0x1F);

    tft_write_com(0xFF);    // Command2 BKx Selection - 12.3.1
    tft_write_dat(0x77);
    tft_write_dat(0x01);
    tft_write_dat(0x00);
    tft_write_dat(0x00);
    tft_write_dat(0x11);

    tft_write_com(0xB0);    // Command 2 BK0: PVGAMCTRL (B0h/B000h): Positive Voltage Gamma Control - 12.3.2.1
    tft_write_dat(0x35);

    tft_write_com(0xB1);    // NVGAMCTRL (B1h/B100h): Negative Voltage Gamma Control - 12.3.2.2
    tft_write_dat(0x6a);

    tft_write_com(0xB2);    // VGHSS (B2h/B200h):VGH Voltage setting - 12.3.3.3
    tft_write_dat(0x81);

    tft_write_com(0xB3);    // TESTCMD (B3h/B300h):TEST Command Setting - 12.3.3.4
    tft_write_dat(0x80);

    tft_write_com(0xB5);    // VGLS (B5h/B500h):VGL Voltage setting - 12.3.3.5
    tft_write_dat(0x4E);

    tft_write_com(0xB7);    // PWCTRL1 (B7h/B700h):Power Control 1 - 12.3.3.6
    tft_write_dat(0x85);

    tft_write_com(0xB8);    // Command 2 BK0: DGMEN (B8h/B800h): Digital Gamma Enable - 12.3.2.3
    tft_write_dat(0x21);

    tft_write_com(0xC0);    // Command 2 BK0: LNESET (C0h/C000h): Display Line Setting - 12.3.2.7
    tft_write_dat(0x09);

    tft_write_com(0xC1);    // Command 2 BK0: PORCTRL (C1h/C100h):Porch Control - 12.3.2.8
    tft_write_dat(0x78);

    tft_write_com(0xC2);    // Command 2 BK0: INVSET (C2h/C200h):Inversion selection & Frame Rate Control - 12.3.2.9
    tft_write_dat(0x78);

    tft_write_com(0xD0);    // MIPISET1 (D0h/D000h):MIPI Setting 1 - 12.3.3.14
    tft_write_dat(0x88);

    tft_write_com(0xE0);    // SECTRL (E0h/E000h):Sunlight Readable Enhancement - 12.3.2.16
    tft_write_dat(0x00);
    tft_write_dat(0xA0);
    tft_write_dat(0x02);

    tft_write_com(0xE1);    // NRCTRL (E1h/E100h):Noise Reduce Control - 12.3.2.17
    tft_write_dat(0x06);
    tft_write_dat(0xA0);
    tft_write_dat(0x08);
    tft_write_dat(0xA0);
    tft_write_dat(0x05);
    tft_write_dat(0xA0);
    tft_write_dat(0x07);
    tft_write_dat(0xA0);
    tft_write_dat(0x00);
    tft_write_dat(0x44);
    tft_write_dat(0x44);

    tft_write_com(0xE2);    // SECTRL (E2h/E200h):Sharpness Control - 12.3.2.18
    tft_write_dat(0x20);
    tft_write_dat(0x20);
    tft_write_dat(0x40);
    tft_write_dat(0x40);
    tft_write_dat(0x96);
    tft_write_dat(0xA0);
    tft_write_dat(0x00);
    tft_write_dat(0x00);
    tft_write_dat(0x96);
    tft_write_dat(0xA0);
    tft_write_dat(0x00);
    tft_write_dat(0x00);
    tft_write_dat(0x00);

    tft_write_com(0xE3);    // CCCTRL (E3h/E300h):Color Calibration Control - 12.3.2.19
    tft_write_dat(0x00);
    tft_write_dat(0x00);
    tft_write_dat(0x22);
    tft_write_dat(0x22);

    tft_write_com(0xE4);    // SKCTRL (E4h/E400h):Skin Tone Preservation Control - 12.3.2.20
    tft_write_dat(0x44);
    tft_write_dat(0x44);

    tft_write_com(0xE5);    // ??
    tft_write_dat(0x0E);
    tft_write_dat(0x97);
    tft_write_dat(0x10);
    tft_write_dat(0xA0);
    tft_write_dat(0x10);
    tft_write_dat(0x99);
    tft_write_dat(0x10);
    tft_write_dat(0xA0);
    tft_write_dat(0x0A);
    tft_write_dat(0x93);
    tft_write_dat(0x10);
    tft_write_dat(0xA0);
    tft_write_dat(0x0C);
    tft_write_dat(0x95);
    tft_write_dat(0x10);
    tft_write_dat(0xA0);

    tft_write_com(0xE6);    // ??
    tft_write_dat(0x00);
    tft_write_dat(0x00);
    tft_write_dat(0x22);
    tft_write_dat(0x22);

    tft_write_com(0xE7);    // ??
    tft_write_dat(0x44);
    tft_write_dat(0x44);

    tft_write_com(0xE8);    // ??
    tft_write_dat(0x0D);
    tft_write_dat(0x96);
    tft_write_dat(0x10);
    tft_write_dat(0xA0);
    tft_write_dat(0x0F);
    tft_write_dat(0x98);
    tft_write_dat(0x10);
    tft_write_dat(0xA0);
    tft_write_dat(0x09);
    tft_write_dat(0x92);
    tft_write_dat(0x10);
    tft_write_dat(0xA0);
    tft_write_dat(0x0B);
    tft_write_dat(0x94);
    tft_write_dat(0x10);
    tft_write_dat(0xA0);

    tft_write_com(0xEB);    // ??
    tft_write_dat(0x00);
    tft_write_dat(0x01);
    tft_write_dat(0x4E);
    tft_write_dat(0x4E);
    tft_write_dat(0x44);
    tft_write_dat(0x88);
    tft_write_dat(0x40);

    tft_write_com(0xEC);    // ??
    tft_write_dat(0x78);
    tft_write_dat(0x00);

    tft_write_com(0xED);    // ??
    tft_write_dat(0xFF);
    tft_write_dat(0xFA);
    tft_write_dat(0x2F);
    tft_write_dat(0x89);
    tft_write_dat(0x76);
    tft_write_dat(0x54);
    tft_write_dat(0x01);
    tft_write_dat(0xFF);
    tft_write_dat(0xFF);
    tft_write_dat(0x10);
    tft_write_dat(0x45);
    tft_write_dat(0x67);
    tft_write_dat(0x98);
    tft_write_dat(0xF2);
    tft_write_dat(0xAF);
    tft_write_dat(0xFF);

    tft_write_com(0xEF);    // ??
    tft_write_dat(0x08);
    tft_write_dat(0x08);
    tft_write_dat(0x08);
    tft_write_dat(0x45);
    tft_write_dat(0x3F);
    tft_write_dat(0x54);

    tft_write_com(0xFF);    // Command2 BKx Selection - 12.3.1
    tft_write_dat(0x77);
    tft_write_dat(0x01);
    tft_write_dat(0x00);
    tft_write_dat(0x00);
    tft_write_dat(0x13);

    tft_write_com(0xE8);    // ??
    tft_write_dat(0x00);
    tft_write_dat(0x0E);


    tft_write_com(0xE8);    // ??
    tft_write_dat(0x00);
    tft_write_dat(0x0C);
    sleep_ms(10);

    tft_write_com(0xE8);    // ??
    tft_write_dat(0x00);
    tft_write_dat(0x00);

    tft_write_com(0xFF);    // Command2 BKx Selection - 12.3.1
    tft_write_dat(0x77);     
    tft_write_dat(0x01);
    tft_write_dat(0x00);
    tft_write_dat(0x00);
    tft_write_dat(0x00);

    tft_write_com(0x3A);    // COLMOD (3Ah/3A00h): Interface Pixel Format - 12.2.30
    tft_write_dat(0x55);    // 0x55   RGB565   //0x77  RGB888

    tft_write_com(0x29);    // DISPON (29h/2900h): Display On - 12.2.24
    tft_write_dat(0x00);

    tft_write_com(0x11);    // SLPOUT (11h/1100h): Sleep Out - 12.2.15
 //   sleep_ms(120);

//   tft_write_com(0x23); // All pixels on (white)
 //   sleep_ms(120);
 //   tft_write_com(0x22); // All pixels off (black)
#endif
}
