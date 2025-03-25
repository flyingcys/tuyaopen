/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "tuya_iot_config.h"
#include "lv_port_disp.h"
#include <stdbool.h>
#include "tal_log.h"

#include "tkl_display.h"

/*********************
 *      DEFINES
 *********************/
#ifndef LV_DISP_HOR_RES
#warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
#define MY_DISP_HOR_RES 128
#else
#define MY_DISP_HOR_RES LV_DISP_HOR_RES
#endif

#ifndef LV_DISP_VER_RES
#warning Please define or replace the macro MY_DISP_VER_RES with the actual screen height, default value 240 is used for now.
#define MY_DISP_VER_RES 128
#else
#define MY_DISP_VER_RES LV_DISP_VER_RES
#endif

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

/**********************
 *  STATIC VARIABLES
 **********************/
static TKL_DISP_DEVICE_S lcd;
static TKL_DISP_EVENT_HANDLER_S event_handle;
static lv_display_t *disp_drv_backup = NULL;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t *disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);

    /* Example 2
     * Two buffers for partial rendering
     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/
    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_1[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL / 20];

    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_2[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL / 20];
    lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void disp_flush_ready_cb(TKL_DISP_PORT_E port, int64_t timestamp)
{
    if (disp_drv_backup) {
        lv_disp_flush_ready(disp_drv_backup);
        disp_drv_backup = NULL;
    }
}

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_DISP_RECT_S rect;
    TKL_DISP_COLOR_U color;
    int brightness;

    memset(&lcd, 0, sizeof(lcd));
    memset(&event_handle, 0, sizeof(event_handle));

    event_handle.vsync_cb = disp_flush_ready_cb;
    event_handle.hotplug_cb = NULL;
    TUYA_CALL_ERR_RETURN(tkl_disp_init(&lcd, &event_handle));

    rect.x = 0;
    rect.y = 0;
    rect.width = MY_DISP_HOR_RES - rect.x + 1;
    rect.height = MY_DISP_VER_RES - rect.y + 1;

    color.full = 0x0000;

    tkl_disp_fill(&lcd, &rect, color);

    if (tkl_disp_get_brightness(&lcd, &brightness) != OPRT_OK) {
        brightness = 255;
    }

    if (brightness == 0)
        brightness = 255;
    tkl_disp_set_brightness(&lcd, brightness);
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_DISP_FRAMEBUFFER_S buf;
    TKL_DISP_RECT_S rect;

    disp_drv_backup = disp_drv;
    if (disp_flush_enabled) {
        buf.buffer = (void *)px_map;
        buf.format = TKL_DISP_PIXEL_FMT_RGB565;
        rect.x = area->x1;
        rect.y = area->y1;
        rect.width = area->x2 - area->x1 + 1;
        rect.height = area->y2 - area->y1 + 1;

        TUYA_CALL_ERR_RETURN(tkl_disp_blit(&lcd, &buf, &rect));

        TUYA_CALL_ERR_RETURN(tkl_disp_flush(&lcd));
    }
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
