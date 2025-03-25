/**
 * @file disp_spi_st7789.c
 * @brief Implementation of ST7789 TFT LCD driver with SPI interface. This file
 *        provides hardware-specific control functions for ST7789 series TFT
 *        displays, including initialization sequence, pixel data transfer,
 *        and display control commands through SPI communication.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include <stdio.h>
#include "disp_spi_driver.h"

/* ST7789 commands */
#define ST7789_NOP     0x00
#define ST7789_SWRESET 0x01
#define ST7789_RDDID   0x04
#define ST7789_RDDST   0x09

#define ST7789_RDDPM      0x0A // Read display power mode
#define ST7789_RDD_MADCTL 0x0B // Read display MADCTL
#define ST7789_RDD_COLMOD 0x0C // Read display pixel format
#define ST7789_RDDIM      0x0D // Read display image mode
#define ST7789_RDDSM      0x0E // Read display signal mode
#define ST7789_RDDSR      0x0F // Read display self-diagnostic result (ST7789V)

#define ST7789_SLPIN  0x10 // Enter Sleep Mode
#define ST7789_SLPOUT 0x11 // Sleep out
#define ST7789_PTLON  0x12
#define ST7789_NORON  0x13

#define ST7789_INVOFF  0x20
#define ST7789_INVON   0x21
#define ST7789_GAMSET  0x26 // Gamma set
#define ST7789_DISPOFF 0x28
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A // Column Address Set
#define ST7789_RASET   0x2B // Row Address Set
#define ST7789_RAMWR   0x2C
#define ST7789_RGBSET  0x2D // Color setting for 4096, 64K and 262K colors
#define ST7789_RAMRD   0x2E

#define ST7789_PTLAR   0x30
#define ST7789_VSCRDEF 0x33 // Vertical scrolling definition (ST7789V)
#define ST7789_TEOFF   0x34 // Tearing effect line off
#define ST7789_TEON    0x35 // Tearing effect line on
#define ST7789_MADCTL  0x36 // Memory lcd_data access control
#define ST7789_IDMOFF  0x38 // Idle mode off
#define ST7789_IDMON   0x39 // Idle mode on
#define ST7789_COLMOD  0x3A
#define ST7789_RAMWRC  0x3C // Memory write continue (ST7789V)
#define ST7789_RAMRDC  0x3E // Memory read continue (ST7789V)

#define ST7789_RAMCTRL   0xB0 // RAM control
#define ST7789_RGBCTRL   0xB1 // RGB control
#define ST7789_PORCTRL   0xB2 // Porch control
#define ST7789_FRCTRL1   0xB3 // Frame rate control
#define ST7789_PARCTRL   0xB5 // Partial mode control
#define ST7789_GCTRL     0xB7 // Gate control
#define ST7789_GTADJ     0xB8 // Gate on timing adjustment
#define ST7789_DGMEN     0xBA // Digital gamma enable
#define ST7789_VCOMS     0xBB // VCOMS setting
#define ST7789_LCMCTRL   0xC0 // LCM control
#define ST7789_IDSET     0xC1 // ID setting
#define ST7789_VDVVRHEN  0xC2 // VDV and VRH command enable
#define ST7789_VRHS      0xC3 // VRH set
#define ST7789_VDVSET    0xC4 // VDV setting
#define ST7789_VCMOFSET  0xC5 // VCOMS offset set
#define ST7789_FRCTR2    0xC6 // FR Control 2
#define ST7789_CABCCTRL  0xC7 // CABC control
#define ST7789_REGSEL1   0xC8 // Register value section 1
#define ST7789_REGSEL2   0xCA // Register value section 2
#define ST7789_PWMFRSEL  0xCC // PWM frequency selection
#define ST7789_PWCTRL1   0xD0 // Power control 1
#define ST7789_VAPVANEN  0xD2 // Enable VAP/VAN signal output
#define ST7789_CMD2EN    0xDF // Command 2 enable
#define ST7789_PVGAMCTRL 0xE0 // Positive voltage gamma control
#define ST7789_NVGAMCTRL 0xE1 // Negative voltage gamma control
#define ST7789_DGMLUTR   0xE2 // Digital gamma look-up table for red
#define ST7789_DGMLUTB   0xE3 // Digital gamma look-up table for blue
#define ST7789_GATECTRL  0xE4 // Gate control
#define ST7789_SPI2EN    0xE7 // SPI2 enable
#define ST7789_PWCTRL2   0xE8 // Power control 2
#define ST7789_EQCTRL    0xE9 // Equalize time control
#define ST7789_PROMCTRL  0xEC // Program control
#define ST7789_PROMEN    0xFA // Program mode enable
#define ST7789_NVMSET    0xFC // NVM setting
#define ST7789_PROMACT   0xFE // Program action

const uint8_t lcd_init_seq[] = {
    1,    100,  ST7789_SWRESET,                                 // Software reset
    1,    50,   ST7789_SLPOUT,                                  // Exit sleep mode
    2,    10,   ST7789_COLMOD,    0x55,                         // Set colour mode to 16 bit
    2,    0,    ST7789_VCMOFSET,  0x1a,                         // VCOM
    6,    0,    ST7789_PORCTRL,   0x05, 0x05, 0x00, 0x33, 0x33, // Porch Setting
    2,    0,    ST7789_GCTRL,     0x05,                         // Gate Control
    2,    0,    ST7789_VCOMS,     0x3f,                         // VCOMS setting
    2,    0,    ST7789_LCMCTRL,   0x2c,                         // LCM control
    2,    0,    ST7789_VDVVRHEN,  0x01,                         // VDV and VRH command enable
    2,    0,    ST7789_VRHS,      0x0f,                         // VRH set
    2,    0,    ST7789_VDVSET,    0x20,                         // VDV setting
    2,    0,    ST7789_FRCTR2,    0x01,                         // FR Control 2
    3,    0,    ST7789_PWCTRL1,   0xa4, 0xa1,                   // Power control 1
    2,    0,    ST7789_PWCTRL2,   0x03,                         // Power control 2
    4,    0,    ST7789_EQCTRL,    0x09, 0x09, 0x08,             // Equalize time control

    2,    0,    ST7789_MADCTL,    0x00, // Set MADCTL: row then column, refresh is bottom to top
    15,   0,    ST7789_PVGAMCTRL, 0xd0, 0x05, 0x09, 0x09, 0x08, 0x14, 0x28, 0x33, 0x3f, 0x07, 0x13, 0x14,
    0x28, 0x30, // Positive voltage gamma control
    15,   0,    ST7789_NVGAMCTRL, 0xd0, 0x05, 0x09, 0x09, 0x08, 0x03, 0x24, 0x32, 0x32, 0x3b, 0x14, 0x13,
    0x28, 0x2f,                // Negative voltage gamma control
    1,    10,   ST7789_NORON,  // Normal display on, then 10 ms delay
    1,    10,   ST7789_DISPON, // Main screen turn on, then wait 500 ms
    0                          // Terminate list
};

/**
 * @brief Set the display window for the ST7789 LCD driver.
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

    lcd_data[0] = ST7789_CASET;
    lcd_data[1] = x_start >> 8;
    lcd_data[2] = x_start;
    lcd_data[3] = x_end >> 8;
    lcd_data[4] = x_end;
    drv_lcd_write_cmd(lcd_data, 4);

    lcd_data[0] = ST7789_RASET;
    lcd_data[1] = y_start >> 8;
    lcd_data[2] = y_start;
    lcd_data[3] = y_end >> 8;
    lcd_data[4] = y_end;
    drv_lcd_write_cmd(lcd_data, 4);

    lcd_data[0] = ST7789_RAMWR;
    drv_lcd_write_cmd(lcd_data, 0);
}