/**
 * @file disp_spi_driver.h
 * @brief Implementation of SPI display driver for TFT screen control. This file
 *        provides low-level SPI communication interfaces and display control
 *        functions for TFT LCD/OLED screens, including screen initialization,
 *        data transmission, and display parameter configuration.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __DISP_DRIVER_H__
#define __DISP_DRIVER_H__

#include <stdint.h>
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_gpio.h"
#include "tkl_spi.h"

typedef void (*LCD_FLUSH_CB)(void);

/**
 * @brief Set the display window for the SPI driver.
 *
 * This function sets the visible area of the display by specifying the start and end
 * coordinates for both the x and y axes.
 *
 * @param x_start The starting x-coordinate of the window.
 * @param y_start The starting y-coordinate of the window.
 * @param x_end The ending x-coordinate of the window.
 * @param y_end The ending y-coordinate of the window.
 */
void disp_driver_set_window(uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end);

/**
 * @brief Writes a command and its associated data to the LCD display using the SPI interface.
 *
 * This function sends a command byte followed by a specified number of data bytes to the LCD.
 * It is used to control the LCD's operations, such as initializing, setting the cursor position,
 * or changing the display settings.
 *
 * @param cmd Pointer to the command byte to be sent to the LCD.
 * @param data_count Number of data bytes to be sent after the command byte.
 */
void drv_lcd_write_cmd(uint8_t *cmd, uint8_t data_count);

/**
 * @brief Sets the brightness of the display.
 *
 * This function adjusts the brightness of the display based on the input value.
 * The brightness value should be between 0 and 100 inclusive.
 *
 * @param bright The brightness level to set (0-100).
 */
void disp_driver_set_bright(uint8_t bright);

/**
 * @brief Gets the current brightness level of the display.
 *
 * This function retrieves the current brightness setting of the display.
 *
 * @return The current brightness level (0-100).
 */
uint8_t disp_driver_get_bright(void);

/**
 * @brief Flushes a rectangular area of the display with pixel data from a provided image buffer.
 *
 * This function sends a portion of the image buffer to the display over the SPI interface.
 * It is used to update only a specific region of the display rather than the entire screen,
 * which can be more efficient for applications that only need to update a small part of the display
 * at any given time.
 *
 * @param x_start The starting x-coordinate of the rectangular area to be updated.
 * @param y_start The starting y-coordinate of the rectangular area to be updated.
 * @param x_end The ending x-coordinate of the rectangular area to be updated.
 * @param y_end The ending y-coordinate of the rectangular area to be updated.
 * @param image A pointer to the image buffer containing the pixel data to be displayed.
 */
void disp_driver_flush(uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end, uint8_t *image);

/**
 * @brief Sets a color for a rectangular area on the display.
 *
 * This function sets a specific color for a rectangular region defined by
 * its top-left corner (x_start, y_start) and bottom-right corner (x_end, y_end).
 *
 * @param x_start The x-coordinate of the top-left corner of the rectangle.
 * @param y_start The y-coordinate of the top-left corner of the rectangle.
 * @param x_end The x-coordinate of the bottom-right corner of the rectangle.
 * @param y_end The y-coordinate of the bottom-right corner of the rectangle.
 * @param color The 16-bit color value to set for the specified area.
 */
void disp_driver_set_color(uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end, uint16_t color);

/**
 * @brief Initializes the display driver.
 *
 * This function initializes the display driver and sets the callback function
 * for flushing the LCD display.
 *
 * @param cb The callback function to be called for LCD flushing.
 */
void disp_driver_init(LCD_FLUSH_CB cb);

#endif /* __DISPLAY_DRIVER_H__ */
