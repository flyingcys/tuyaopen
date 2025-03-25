/**
 * @file example_lvgl.c
 * @brief LVGL (Light and Versatile Graphics Library) example for SDK.
 *
 * This file provides an example implementation of using the LVGL library with the Tuya SDK.
 * It demonstrates the initialization and usage of LVGL for graphical user interface (GUI) development.
 * The example covers setting up the display port, initializing LVGL, and running a demo application.
 *
 * The LVGL example aims to help developers understand how to integrate LVGL into their Tuya IoT projects for
 * creating graphical user interfaces on embedded devices. It includes detailed examples of setting up LVGL,
 * handling display updates, and integrating these functionalities within a multitasking environment.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental LVGL
 * operations critical for GUI development on embedded systems.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_spi.h"
#include "tkl_system.h"
#include "tkl_display.h"

#include "tuya_lcd_device.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "lv_port_disp.h"
#if defined(LVGL_ENABLE_TOUCH) || defined(LVGL_ENABLE_ENCODER)
#include "lv_port_indev.h"
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TKL_DISP_DEVICE_S sg_lcd_dev = {
    .device_id = 0,
    .device_port = TKL_DISP_LCD,
    .device_info = NULL,
};
static THREAD_HANDLE sg_display_thrd_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
lv_obj_t *clock_label;
char buf[64];

void my_printf(lv_log_level_t level, const char *file, uint32_t line, const char *dsc)
{
    PR_DEBUG_RAW("level:%d,file:%s,line:%d,dsc:%s\n", level, file, line, dsc);
}
void update_clock(lv_timer_t *timer)
{
    static uint32_t cnt = 0;

    cnt++;
    memset(buf, 0, sizeof(buf));
    itoa(cnt, buf, 10);

    lv_label_set_text(clock_label, buf);
}

static ui_init(void)
{
    clock_label = lv_label_create(lv_screen_active());

    lv_label_set_text(clock_label, "Hello World!");
    lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, 0);

    // lv_timer_create(update_clock, 1000, NULL);
}
static void __chat_display_task(void *args)
{
    uint32_t tick = 0;

    while (1) {
        tick = tkl_system_get_millisecond();
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "Hello World %u", tick);
        // printf("buf: %s\n", buf);
        // if (cnt % 2 == 0)
        //     lv_label_set_text(clock_label, "test");
        // else
        //     lv_label_set_text(clock_label, "Hello World!");

        lv_label_set_text(clock_label, buf);

        tal_system_sleep(100);
    }
}
static uint32_t lv_tick_get_cb(void)
{
    return (uint32_t)tkl_system_get_millisecond();
}

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main()
{
    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    // register lcd device
    tuya_lcd_device_register(sg_lcd_dev.device_id);

    lv_init();
    lv_tick_set_cb(lv_tick_get_cb);
    lv_port_disp_init(&sg_lcd_dev);
#ifdef LVGL_ENABLE_TOUCH
    lv_port_indev_init();
#endif

    /*Create a Demo*/
    // lv_demo_widgets();
    lv_log_register_print_cb(my_printf);

    ui_init();
    THREAD_CFG_T cfg = {
        .thrdname = "chat_display",
        .priority = THREAD_PRIO_1,
        .stackDepth = 1024 * 2,
    };

    tal_thread_create_and_start(&sg_display_thrd_hdl, NULL, NULL, __chat_display_task, NULL, &cfg);

    while (1) {
        lv_timer_handler();
        tal_system_sleep(5);
    }
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {1024 * 4, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif