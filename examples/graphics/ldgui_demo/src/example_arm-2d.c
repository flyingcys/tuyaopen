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
// #include "tkl_spi.h"
#include "tkl_system.h"

#include "arm_2d_helper.h"
#include "arm_2d_scenes.h"
#include "arm_2d_disp_adapters.h"

#include "ldGui.h"

#include "ldScene0.h"
#include "ldScene1.h"

#ifdef RTE_Acceleration_Arm_2D_Extra_Benchmark
#   include "arm_2d_benchmark.h"
#endif

#include "arm_2d_scene_meter.h"
#include "arm_2d_scene_watch.h"
#include "arm_2d_scene_fitness.h"
#include "arm_2d_scene_alarm_clock.h"
#include "arm_2d_scene_histogram.h"
#include "arm_2d_scene_fan.h"
#include "arm_2d_scene_bubble_charging.h"
#include "arm_2d_scene_animate_background.h"
#include "arm_2d_scene_knob.h"

#include "arm_2d_demos.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#ifndef LCD_TARGET_FPS
#   define LCD_TARGET_FPS       30
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

void scene_meter_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_LEFT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 500);

    arm_2d_scene_meter_init(&DISP0_ADAPTER);
}

void scene_watch_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_RIGHT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 500);

    arm_2d_scene_watch_init(&DISP0_ADAPTER);
}

void scene_watch_face_01_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_FLY_IN_FROM_RIGHT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 500);

    arm_2d_scene_watch_face_01_init(&DISP0_ADAPTER);
}

void scene_fitness_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_RIGHT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 500);
    arm_2d_scene_fitness_init(&DISP0_ADAPTER);
}

void scene_alarm_clock_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_LEFT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 500);
    arm_2d_scene_alarm_clock_init(&DISP0_ADAPTER);
}

void scene_histogram_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_ERASE_LEFT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 1000);
    arm_2d_scene_histogram_init(&DISP0_ADAPTER);
}

void scene_audiomark_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_RIGHT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 500);

    arm_2d_scene_audiomark_init(&DISP0_ADAPTER);
}

void scene_atom_loader(void) 
{
    arm_2d_scene_atom_init(&DISP0_ADAPTER);
}

void scene_basics_loader(void) 
{
#if 0
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_FADE_WHITE);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 3000);
#endif 

    arm_2d_scene_basics_init(&DISP0_ADAPTER);
}

void scene_progress_status_loader(void) 
{
    arm_2d_scene_progress_status_init(&DISP0_ADAPTER);
}

void scene_panel_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_RIGHT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 500);

    arm_2d_scene_panel_init(&DISP0_ADAPTER);
}

void scene_gas_gauge_loader(void) 
{
    arm_2d_scene_gas_gauge_init(&DISP0_ADAPTER);
}

void scene_listview_loader(void) 
{
    arm_2d_scene_listview_init(&DISP0_ADAPTER);
}

void scene_menu_loader(void) 
{
    arm_2d_scene_menu_init(&DISP0_ADAPTER);
}

void scene_fan_loader(void) 
{
    arm_2d_scene_fan_init(&DISP0_ADAPTER);
}

void scene_console_window_loader(void)
{
    arm_2d_scene_console_window_init(&DISP0_ADAPTER);
}

void scene_bubble_charging_loader(void) 
{
    arm_2d_scene_bubble_charging_init(&DISP0_ADAPTER);
}

void scene_ruler_loader(void) 
{
    arm_2d_scene_ruler_init(&DISP0_ADAPTER);
}

void scene_hollow_out_list_loader(void) 
{
    arm_2d_scene_hollow_out_list_init(&DISP0_ADAPTER);
}

void scene_transform_loader(void) 
{
    arm_2d_scene_transform_init(&DISP0_ADAPTER);
}

void scene_filters_loader(void) 
{
    arm_2d_scene_filters_init(&DISP0_ADAPTER);
}

void scene_compass_loader(void) 
{
    arm_2d_scene_compass_init(&DISP0_ADAPTER);
}

void scene_knob_loader(void) 
{
    arm_2d_scene_knob_init(&DISP0_ADAPTER);
}

void scene_user_defined_opcode_loader(void) 
{
    arm_2d_scene_user_defined_opcode_init(&DISP0_ADAPTER);
}

void scene_mono_loading_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_LEFT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 300);

    arm_2d_scene_mono_loading_init(&DISP0_ADAPTER);
}

void scene_mono_histogram_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_LEFT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 300);

    arm_2d_scene_mono_histogram_init(&DISP0_ADAPTER);
}

void scene_mono_clock_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_LEFT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 300);

    arm_2d_scene_mono_clock_init(&DISP0_ADAPTER);
}

void scene_mono_list_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_LEFT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 300);

    arm_2d_scene_mono_list_init(&DISP0_ADAPTER);
}

void scene_mono_tracking_list_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_LEFT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 300);

    arm_2d_scene_mono_tracking_list_init(&DISP0_ADAPTER);
}

void scene_mono_icon_menu_loader(void) 
{
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_SLIDE_LEFT);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 300);

    arm_2d_scene_mono_icon_menu_init(&DISP0_ADAPTER);
}

#if __DISP0_CFG_VIRTUAL_RESOURCE_HELPER__
void scene_animate_background_loader(void) 
{
    arm_2d_scene_animate_background_init(&DISP0_ADAPTER);
}


void scene_virtual_resource_loader(void) 
{
    arm_2d_scene_virtual_resource_init(&DISP0_ADAPTER);
}
#endif

typedef struct demo_scene_t {
    int32_t nLastInMS;
    void (*fnLoader)(void);
} demo_scene_t;

static demo_scene_t const c_SceneLoaders[] = {

#if 1

#if defined(__DISP0_CFG_COLOR_SOLUTION__) && __DISP0_CFG_COLOR_SOLUTION__ == 1
    {
        13000,
        scene_mono_loading_loader,
    },
    {
        5000,
        scene_mono_histogram_loader,
    },
    {
        5000,
        scene_mono_clock_loader,
    },
    {
        7000,
        scene_mono_list_loader,
    },
    {
        15000,
        scene_mono_tracking_list_loader,
    },
    {
        12000,
        scene_mono_icon_menu_loader,
    }
#else
    {
        3000,
        scene_basics_loader,
    },
    {
        10000,
        scene_progress_status_loader,
    },
    {
        13000,
        scene_fan_loader,
    },
    {
        10000,
        scene_console_window_loader,
    },
    {
        15000,
        scene_meter_loader,
    },
    {
        30000,
        scene_watch_face_01_loader,
    },
    {
        30000,
        scene_watch_loader,
    },
//    {
//        20000,
//        scene_compass_loader,
//    },
    {
        10000,
        scene_alarm_clock_loader,
    },
    {
        5000,
        scene_atom_loader,
    },
    {
        10000,
        scene_histogram_loader,
    },
    {
        30000,
        scene_bubble_charging_loader,
    },
    {
        29000,
        scene_gas_gauge_loader,
    },
    {
        12000,
        scene_listview_loader,
    },
    {
        12000,
        scene_menu_loader,
    },
//    {
//        10000,
//        scene_ruler_loader,
//    },
//    {
//        10000,
//        scene_hollow_out_list_loader,
//    },
    {
        20000,
        scene_panel_loader,
    },
    {
        20000,
        scene_fitness_loader,
    },
//    {
//        15000,
//        scene_transform_loader,
//    },
    {
        50000,
        scene_filters_loader,
    },
//    {
//        10000,
//        scene_audiomark_loader,
//    },
    
#if __DISP0_CFG_VIRTUAL_RESOURCE_HELPER__
    {
        3000,
        scene_virtual_resource_loader,
    },
    {
        5000,
        scene_animate_background_loader,
    },
#endif

#endif

#else
    {
        .fnLoader = 
        scene_filters_loader,
        //scene_listview_loader,
        //scene_mono_tracking_list_loader
        //scene_mono_list_loader,
        //scene_gas_gauge_loader,
        //scene_meter_loader,
        //scene_compass_loader,
        //scene_basics_loader,
        //scene_fitness_loader,
        //scene_user_defined_opcode_loader,
        //scene_knob_loader,
        //scene_panel_loader,
    },
#endif


};

static
struct {
    int8_t chIndex;
    bool bIsTimeout;
    int32_t nDelay;
    int64_t lTimeStamp;
    
} s_tDemoCTRL = {
    .chIndex = -1,
    .bIsTimeout = true,
};


/* load scene one by one */
void before_scene_switching_handler(void *pTarget,
                                    arm_2d_scene_player_t *ptPlayer,
                                    arm_2d_scene_t *ptScene)
{
    printf("before_scene_switching_handler\r\n");
    switch (arm_2d_scene_player_get_switching_status(&DISP0_ADAPTER)) {
        case ARM_2D_SCENE_SWITCH_STATUS_MANUAL_CANCEL:
            s_tDemoCTRL.chIndex--;
            break;
        default:
            s_tDemoCTRL.chIndex++;
            break;
    }

    if (s_tDemoCTRL.chIndex >= dimof(c_SceneLoaders)) {
        s_tDemoCTRL.chIndex = 0;
    } else if (s_tDemoCTRL.chIndex < 0) {
        s_tDemoCTRL.chIndex += dimof(c_SceneLoaders);
    }
    
    /* call loader */
    arm_with(const demo_scene_t, &c_SceneLoaders[s_tDemoCTRL.chIndex]) {
        if (_->nLastInMS > 0) {
            s_tDemoCTRL.bIsTimeout = false;
            printf("bIsTimeout = false\r\n");
            s_tDemoCTRL.lTimeStamp = 0;
            s_tDemoCTRL.nDelay = _->nLastInMS;
        }
        _->fnLoader();
    }
}

extern void arm2d_disp_init(char *device);

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /*hardware register*/
    board_register_hardware();
    
    arm2d_disp_init(DISPLAY_NAME);

    arm_irq_safe {
        arm_2d_init();
    }

    disp_adapter0_init();

#if !defined(RTE_Acceleration_Arm_2D_Extra_Benchmark)
    arm_2d_scene_player_register_before_switching_event_handler(
            &DISP0_ADAPTER,
            before_scene_switching_handler);
    
    arm_2d_scene_player_set_switching_mode( &DISP0_ADAPTER,
                                            ARM_2D_SCENE_SWITCH_MODE_FADE_WHITE);
    arm_2d_scene_player_set_switching_period(&DISP0_ADAPTER, 3000);
    
    arm_2d_scene_player_switch_to_next_scene(&DISP0_ADAPTER);
#endif

    
#if defined(RTE_Acceleration_Arm_2D_Extra_Benchmark)
    arm_2d_run_benchmark();
#endif

    while (1) {
        while(arm_fsm_rt_cpl != disp_adapter0_task());

        if (!s_tDemoCTRL.bIsTimeout) {
            printf("444nDelay = %d\r\n", s_tDemoCTRL.nDelay);
            if (arm_2d_helper_is_time_out(s_tDemoCTRL.nDelay, &s_tDemoCTRL.lTimeStamp)) {
                s_tDemoCTRL.bIsTimeout = true;
                printf("bIsTimeout = true next scene\r\n");
                arm_2d_scene_player_switch_to_next_scene(&DISP0_ADAPTER);
            }
        }
        tal_system_sleep(1000 / LCD_TARGET_FPS);
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
    (void) arg;

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