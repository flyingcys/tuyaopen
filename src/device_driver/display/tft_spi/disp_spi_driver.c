/**
 * @file disp_spi_driver.c
 * @brief Implementation of SPI display driver for TFT screen control. This file
 *        provides low-level SPI communication interfaces and display control
 *        functions for TFT LCD/OLED screens, including screen initialization,
 *        data transmission, and display parameter configuration.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include <stdio.h>
#include "disp_spi_driver.h"

static uint8_t bright_control = 0;
extern const uint8_t lcd_init_seq[];

static LCD_FLUSH_CB g_lcd_flush_cb;

#define SPITFT_SET_CS_HIGH() tkl_gpio_write(DISPLAY_SPI_CS_PIN, TUYA_GPIO_LEVEL_HIGH)
#define SPITFT_SET_CS_LOW()  tkl_gpio_write(DISPLAY_SPI_CS_PIN, TUYA_GPIO_LEVEL_LOW)

#define SPITFT_SET_DC_HIGH() tkl_gpio_write(DISPLAY_SPI_DC_PIN, TUYA_GPIO_LEVEL_HIGH)
#define SPITFT_SET_DC_LOW()  tkl_gpio_write(DISPLAY_SPI_DC_PIN, TUYA_GPIO_LEVEL_LOW)

#define SPITFT_SET_RST_HIGH() tkl_gpio_write(DISPLAY_SPI_RST_PIN, TUYA_GPIO_LEVEL_HIGH)
#define SPITFT_SET_RST_LOW()  tkl_gpio_write(DISPLAY_SPI_RST_PIN, TUYA_GPIO_LEVEL_LOW)

#if DISPLAY_SPI_BL_PIN_POLARITY_HIGH
#define SPITFT_SET_BL_HIGH() tkl_gpio_write(DISPLAY_SPI_BL_PIN, TUYA_GPIO_LEVEL_HIGH)
#define SPITFT_SET_BL_LOW()  tkl_gpio_write(DISPLAY_SPI_BL_PIN, TUYA_GPIO_LEVEL_LOW)
#else
#define SPITFT_SET_BL_HIGH() tkl_gpio_write(DISPLAY_SPI_BL_PIN, TUYA_GPIO_LEVEL_LOW)
#define SPITFT_SET_BL_LOW()  tkl_gpio_write(DISPLAY_SPI_BL_PIN, TUYA_GPIO_LEVEL_HIGH)
#endif

#define DISP_SPI_DRIVER_ASYNC

#ifdef PLATFORM_T3
#define TFT_SPI_TX_MAX_SIZE 65535
#elif PLATFORM_T5
#define TFT_SPI_TX_MAX_SIZE 65535
#else
#error "Please set spi tx max size"
#endif

#define PWM_DUTY 5000 // 50% duty
static void drv_lcd_spi_irq_cb(TUYA_SPI_NUM_E port, TUYA_SPI_IRQ_EVT_E event)
{
    tkl_spi_irq_disable(DISPLAY_SPI_PORT);

    if (g_lcd_flush_cb)
        g_lcd_flush_cb();
}

static void drv_lcd_gpio_init(void)
{
    TUYA_GPIO_BASE_CFG_T pin_cfg;
    OPERATE_RET rt = OPRT_OK;

    pin_cfg.mode = TUYA_GPIO_PUSH_PULL;
    pin_cfg.direct = TUYA_GPIO_OUTPUT;
    pin_cfg.level = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_LOG(tkl_gpio_init(DISPLAY_SPI_CS_PIN, &pin_cfg));
    TUYA_CALL_ERR_LOG(tkl_gpio_init(DISPLAY_SPI_DC_PIN, &pin_cfg));
    TUYA_CALL_ERR_LOG(tkl_gpio_init(DISPLAY_SPI_RST_PIN, &pin_cfg));
    TUYA_CALL_ERR_LOG(tkl_gpio_init(DISPLAY_SPI_BL_PIN, &pin_cfg));
}

static int drv_lcd_spi_init(LCD_FLUSH_CB cb)
{
    OPERATE_RET rt = OPRT_OK;
    /*spi init*/
    TUYA_SPI_BASE_CFG_T spi_cfg = {.mode = TUYA_SPI_MODE0,
                                   .freq_hz = DISPLAY_SPI_CLK,
                                   .databits = TUYA_SPI_DATA_BIT8,
                                   .bitorder = TUYA_SPI_ORDER_MSB2LSB,
                                   .role = TUYA_SPI_ROLE_MASTER,
                                   .type = TUYA_SPI_AUTO_TYPE,
                                   .spi_dma_flags = 1};

    PR_DEBUG("spi init %d", spi_cfg.freq_hz);
    TUYA_CALL_ERR_LOG(tkl_spi_init(DISPLAY_SPI_PORT, &spi_cfg));

    g_lcd_flush_cb = cb;
    tkl_spi_irq_init(DISPLAY_SPI_PORT, drv_lcd_spi_irq_cb);

    return rt;
}

static void drv_lcd_reset(void)
{
    SPITFT_SET_RST_HIGH();
    tal_system_sleep(100);

    SPITFT_SET_RST_LOW();
    tal_system_sleep(100);

    SPITFT_SET_RST_HIGH();
    tal_system_sleep(100);
}
static void drv_lcd_bl_pwm_init(void)
{
#if CONFIG_DISPLAY_SPI_BL_PWM
    uint32_t default_duty = PWM_DUTY;
    if (bright_control != 0)
        default_duty = bright_control * 100;

    /* pwm init */
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .duty = default_duty, /* 1-10000 */
        .frequency = DISPLAY_SPI_BL_PWM_FREQ,
        .polarity = TUYA_PWM_NEGATIVE,
    };

    OPERATE_RET rt = tkl_pwm_init(DISPLAY_SPI_BL_PWM_ID, &pwm_cfg);
    if (rt != OPRT_OK)
        PR_ERR("pwm init failed");
#endif
}
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
void drv_lcd_write_cmd(uint8_t *cmd, uint8_t data_count)
{
    OPERATE_RET rt = OPRT_OK;

    SPITFT_SET_CS_LOW();
    SPITFT_SET_DC_LOW();

    // send cmd
    TUYA_CALL_ERR_LOG(tkl_spi_send(DISPLAY_SPI_PORT, cmd, 1));

    // send lcd_data
    if (data_count > 0) {
        SPITFT_SET_DC_HIGH();

        TUYA_CALL_ERR_LOG(tkl_spi_send(DISPLAY_SPI_PORT, cmd + 1, data_count));
    }

    SPITFT_SET_CS_HIGH();
}
/**
 * @brief Sets the brightness of the display.
 *
 * This function adjusts the brightness of the display based on the input value.
 * The brightness value should be between 0 and 100 inclusive.
 *
 * @param bright The brightness level to set (0-100).
 */
void disp_driver_set_bright(uint8_t bright)
{
    if (bright > 100 || bright < 0) {
        PR_DEBUG("bright %d invalid", bright);
        return;
    }

    if (bright == 0)
        SPITFT_SET_BL_LOW();
    else if (bright == 100)
        SPITFT_SET_BL_HIGH();
    else {
#if DISPLAY_SPI_BL_PWM
        PR_DEBUG("set bright %d", bright);
        tkl_pwm_start(DISPLAY_SPI_BL_PWM_ID, bright * 100);
        tkl_pwm_start(DISPLAY_SPI_BL_PWM_ID);
#else
        SPITFT_SET_BL_HIGH();
#endif
    }

    bright_control = bright;
}

/**
 * @brief Gets the current brightness level of the display.
 *
 * This function retrieves the current brightness setting of the display.
 *
 * @return The current brightness level (0-100).
 */
uint8_t disp_driver_get_bright(void)
{
    return bright_control;
}

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
void disp_driver_flush(uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end, uint8_t *image)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t image_pos = 0;
    uint32_t image_len = (x_end - x_start + 1) * (y_end - y_start + 1) * 2;

    disp_driver_set_window(x_start, y_start, x_end, y_end);

    SPITFT_SET_CS_LOW();
    SPITFT_SET_DC_HIGH();

#ifdef DISP_SPI_DRIVER_ASYNC
    if (image_len <= TFT_SPI_TX_MAX_SIZE) {
        tkl_spi_irq_enable(DISPLAY_SPI_PORT);
    }
#endif

    while (image_len > TFT_SPI_TX_MAX_SIZE) {
        TUYA_CALL_ERR_LOG(tkl_spi_send(DISPLAY_SPI_PORT, (uint8_t *)image + image_pos, TFT_SPI_TX_MAX_SIZE));

        image_len -= TFT_SPI_TX_MAX_SIZE;
        image_pos += TFT_SPI_TX_MAX_SIZE;
    }

    if (image_len > 0) {
        TUYA_CALL_ERR_LOG(tkl_spi_send(DISPLAY_SPI_PORT, (uint8_t *)image + image_pos, image_len));
    }

#ifndef DISP_SPI_DRIVER_ASYNC
    SPITFT_SET_CS_HIGH();
    if (g_lcd_flush_cb)
        g_lcd_flush_cb();
#else
    if (image_len > TFT_SPI_TX_MAX_SIZE)
        SPITFT_SET_CS_HIGH();
#endif
}

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
void disp_driver_set_color(uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end, uint16_t color)
{
    uint16_t i;
    OPERATE_RET rt = OPRT_OK;
    uint32_t image_len = (x_end - x_start + 1) * (y_end - y_start + 1) * 2;

    disp_driver_set_window(x_start, y_start, x_end, y_end);

    SPITFT_SET_CS_LOW();
    SPITFT_SET_DC_HIGH();

    uint8_t *buffer = tal_malloc(image_len);
    if (!buffer) {
        PR_ERR("%s: malloc failed", __func__);
        return;
    }

    for (i = 0; i < (x_end - x_start + 1); i++) {
        buffer[i * 2] = color >> 8;
        buffer[i * 2 + 1] = color & 0xff;
    }

    for (i = 0; i < (y_end - y_start + 1); i++) {
        TUYA_CALL_ERR_LOG(tkl_spi_send(DISPLAY_SPI_PORT, (uint8_t *)buffer, (y_end - y_start + 1) * 2));
    }

    if (buffer)
        tal_free(buffer);

    SPITFT_SET_CS_HIGH();
}

/**
 * @brief Initializes the display driver.
 *
 * This function initializes the display driver and sets the callback function
 * for flushing the LCD display.
 *
 * @param cb The callback function to be called for LCD flushing.
 */
void disp_driver_init(LCD_FLUSH_CB cb)
{
    if (cb == NULL) {
        PR_ERR("Invalid parameter");
        return;
    }

    disp_driver_set_bright(0);

    drv_lcd_spi_init(cb);

    drv_lcd_gpio_init();

    SPITFT_SET_BL_LOW();
    drv_lcd_bl_pwm_init();

    drv_lcd_reset();

    // init lcd
    const uint8_t *cmd = lcd_init_seq;
    while (*cmd) {
        drv_lcd_write_cmd((uint8_t *)(cmd + 2), *cmd - 1);
        tal_system_sleep(*(cmd + 1));
        cmd += *cmd + 2;
    }

    PR_DEBUG("disp_driver_init success!");
}
