/**
 * @file disp_spi_gc9a01.c
 * @brief Implementation of GC9A01 TFT LCD driver with SPI interface. This file
 *        provides hardware-specific control functions for GC9A01 series TFT
 *        displays, including initialization sequence, pixel data transfer,
 *        and display control commands through SPI communication.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include <stdio.h>
#include "disp_spi_driver.h"

/* GC9A01 commands */
#define GC9A01_NOP     0x00
#define GC9A01_SWRESET 0x01
#define GC9A01_RDDID   0x04
#define GC9A01_RDDST   0x09

#define GC9A01_RDDPM      0x0A // Read display power mode
#define GC9A01_RDD_MADCTL 0x0B // Read display MADCTL
#define GC9A01_RDD_COLMOD 0x0C // Read display pixel format
#define GC9A01_RDDIM      0x0D // Read display image mode
#define GC9A01_RDDSM      0x0E // Read display signal mode
#define GC9A01_RDDSR      0x0F // Read display self-diagnostic result (GC9A01)

#define GC9A01_SLPIN  0x10 // Enter Sleep Mode
#define GC9A01_SLPOUT 0x11 // Sleep out
#define GC9A01_PTLON  0x12
#define GC9A01_NORON  0x13

#define GC9A01_INVOFF  0x20
#define GC9A01_INVON   0x21
#define GC9A01_GAMSET  0x26 // Gamma set
#define GC9A01_DISPOFF 0x28
#define GC9A01_DISPON  0x29
#define GC9A01_CASET   0x2A // Column Address Set
#define GC9A01_RASET   0x2B // Row Address Set
#define GC9A01_RAMWR   0x2C
#define GC9A01_RGBSET  0x2D // Color setting for 4096, 64K and 262K colors
#define GC9A01_RAMRD   0x2E

#define GC9A01_PTLAR   0x30
#define GC9A01_VSCRDEF 0x33 // Vertical scrolling definition (GC9A01)
#define GC9A01_TEOFF   0x34 // Tearing effect line off
#define GC9A01_TEON    0x35 // Tearing effect line on
#define GC9A01_MADCTL  0x36 // Memory lcd_data access control
#define GC9A01_IDMOFF  0x38 // Idle mode off
#define GC9A01_IDMON   0x39 // Idle mode on
#define GC9A01_COLMOD  0x3A // Color mode
#define GC9A01_RAMWRC  0x3C // Memory write continue (GC9A01)
#define GC9A01_RAMRDC  0x3E // Memory read continue (GC9A01)

#define GC9A01_RAMCTRL   0xB0 // RAM control
#define GC9A01_RGBCTRL   0xB1 // RGB control
#define GC9A01_PORCTRL   0xB2 // Porch control
#define GC9A01_FRCTRL1   0xB3 // Frame rate control
#define GC9A01_PARCTRL   0xB5 // Partial mode control
#define GC9A01_GCTRL     0xB7 // Gate control
#define GC9A01_GTADJ     0xB8 // Gate on timing adjustment
#define GC9A01_DGMEN     0xBA // Digital gamma enable
#define GC9A01_VCOMS     0xBB // VCOMS setting
#define GC9A01_LCMCTRL   0xC0 // LCM control
#define GC9A01_IDSET     0xC1 // ID setting
#define GC9A01_VDVVRHEN  0xC2 // VDV and VRH command enable
#define GC9A01_VRHS      0xC3 // VRH set
#define GC9A01_VDVSET    0xC4 // VDV setting
#define GC9A01_VCMOFSET  0xC5 // VCOMS offset set
#define GC9A01_FRCTR2    0xC6 // FR Control 2
#define GC9A01_CABCCTRL  0xC7 // CABC control
#define GC9A01_REGSEL1   0xC8 // Register value section 1
#define GC9A01_REGSEL2   0xCA // Register value section 2
#define GC9A01_PWMFRSEL  0xCC // PWM frequency selection
#define GC9A01_PWCTRL1   0xD0 // Power control 1
#define GC9A01_VAPVANEN  0xD2 // Enable VAP/VAN signal output
#define GC9A01_CMD2EN    0xDF // Command 2 enable
#define GC9A01_PVGAMCTRL 0xE0 // Positive voltage gamma control
#define GC9A01_NVGAMCTRL 0xE1 // Negative voltage gamma control
#define GC9A01_DGMLUTR   0xE2 // Digital gamma look-up table for red
#define GC9A01_DGMLUTB   0xE3 // Digital gamma look-up table for blue
#define GC9A01_GATECTRL  0xE4 // Gate control
#define GC9A01_SPI2EN    0xE7 // SPI2 enable
#define GC9A01_PWCTRL2   0xE8 // Power control 2
#define GC9A01_EQCTRL    0xE9 // Equalize time control
#define GC9A01_PROMCTRL  0xEC // Program control
#define GC9A01_PROMEN    0xFA // Program mode enable
#define GC9A01_NVMSET    0xFC // NVM setting
#define GC9A01_PROMACT   0xFE // Program action

const uint8_t lcd_init_seq[] = {
    1,    0,    0xEF, 2,    0,    0xEB, 0x14, 1,    0,    0xFE, 1,    0,    0xEF, 2,    0,    0xEB, 0x14,

    2,    0,    0x84, 0x40, 2,    0,    0x85, 0xFF, 2,    0,    0x86, 0xFF, 2,    0,    0x87, 0xFF, 2,
    0,    0x88, 0x0A, 2,    0,    0x89, 0x21, 2,    0,    0x8A, 0x00, 2,    0,    0x8B, 0x80, 2,    0,
    0x8C, 0x01, 2,    0,    0x8D, 0x01, 2,    0,    0x8E, 0xFF, 2,    0,    0x8F, 0xFF,

    3,    0,    0xB6, 0x00, 0x00, 2,    0,    0x36, 0x48, 2,    0,    0x3A, 0x05, //
    5,    0,    0x90, 0x08, 0x08, 0x08, 0x08,

    2,    0,    0xBD, 0x06, 2,    0,    0xBC, 0x00, 4,    0,    0xFF, 0x60, 0x01, 0x04, 4,    0,    0xC3,
    0x13, 0xC4, 0x13, 2,    0,    0xC9, 0x22, 2,    0,    0xBE, 0x11, 3,    0,    0xE1, 0x10, 0x0E, 4,
    0,    0xDF, 0x31, 0x0c, 0x02,

    7,    0,    0xF0, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A, 7,    0,    0xF1, 0x43, 0x70, 0x72, 0x36, 0x37,
    0x6F, 7,    0,    0xF2, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A, 7,    0,    0xF3, 0x43, 0x70, 0x72, 0x36,
    0x37, 0x6F, 3,    0,    0xED, 0x1B, 0x0B,

    2,    0,    0xAE, 0x77,

    2,    0,    0xCD, 0x63,

    10,   0,    0x70, 0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03,

    2,    0,    0xE8, 0x34,

    13,   0,    0x62, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70,

    13,   0,    0x63, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70,

    8,    0,    0x64, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07,

    11,   0,    0x66, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00, 11,   0,    0x67, 0x00,
    0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98,

    8,    0,    0x74, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00,

    3,    0,    0x98, 0x3e, 0x07,

    1,    0,    0x35, 1,    0,    0x21,

    1,    120,  0x11, 1,    20,   0x29,

    0 // Terminate list
};

/**
 * @brief Set the display window for the GC9A01 LCD driver.
 *
 * This function sets the visible window on the LCD display by specifying the
 * start and end coordinates for both the X and Y axes.
 *
 * @param x_start The starting X coordinate of the window.
 * @param y_start The starting Y coordinate of the window.
 * @param x_end The ending X coordinate of the window.
 * @param y_end The ending Y coordinate of the window.
 */
void disp_driver_set_window(uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end)
{
    uint8_t lcd_data[5];

    lcd_data[0] = GC9A01_CASET;
    lcd_data[1] = x_start >> 8;
    lcd_data[2] = x_start;
    lcd_data[3] = x_end >> 8;
    lcd_data[4] = x_end;
    drv_lcd_write_cmd(lcd_data, 4);

    lcd_data[0] = GC9A01_RASET;
    lcd_data[1] = y_start >> 8;
    lcd_data[2] = y_start;
    lcd_data[3] = y_end >> 8;
    lcd_data[4] = y_end;
    drv_lcd_write_cmd(lcd_data, 4);

    lcd_data[0] = GC9A01_RAMWR;
    drv_lcd_write_cmd(lcd_data, 0);
}