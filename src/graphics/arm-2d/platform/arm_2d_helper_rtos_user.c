/*
 * Copyright (c) 2009-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*============================ INCLUDES ======================================*/

#include "tkl_memory.h"
#include "arm_2d_helper.h"
#include "arm_2d_disp_adapters.h"
#include "tal_api.h"

#if defined(__PERF_COUNTER__) && defined(__PERF_CNT_USE_RTOS__)
#   include "perf_counter.h"
#endif

#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wunknown-warning-option"
#   pragma clang diagnostic ignored "-Wreserved-identifier"
#   pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#   pragma clang diagnostic ignored "-Wsign-conversion"
#   pragma clang diagnostic ignored "-Wpadded"
#   pragma clang diagnostic ignored "-Wcast-qual"
#   pragma clang diagnostic ignored "-Wcast-align"
#   pragma clang diagnostic ignored "-Wmissing-field-initializers"
#   pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#   pragma clang diagnostic ignored "-Wmissing-prototypes"
#   pragma clang diagnostic ignored "-Wunused-variable"
#   pragma clang diagnostic ignored "-Wgnu-statement-expression"
#   pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#   pragma clang diagnostic ignored "-Wbad-function-cast"
#   pragma clang diagnostic ignored "-Wunreachable-code-break"
#   pragma clang diagnostic ignored "-Wshorten-64-to-32"
#   pragma clang diagnostic ignored "-Wdouble-promotion"
#elif __IS_COMPILER_ARM_COMPILER_5__
#elif __IS_COMPILER_GCC__
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wformat="
#   pragma GCC diagnostic ignored "-Wpedantic"
#endif

// #define ENABLE_LVGL_DMA2D       1
/*============================ MACROS ========================================*/
/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/
/*============================ GLOBAL VARIABLES ==============================*/
/*============================ PROTOTYPES ====================================*/
extern
void *__arm_2d_helper_perf_counter_get(arm_2d_perfc_type_t tType);
/*============================ LOCAL VARIABLES ===============================*/

/*============================ IMPLEMENTATION ================================*/

__OVERRIDE_WEAK
arm_2d_runtime_feature_t ARM_2D_RUNTIME_FEATURE = {
    .TREAT_OUT_OF_RANGE_AS_COMPLETE         = 1,
    .HAS_DEDICATED_THREAD_FOR_2D_TASK       = __ARM_2D_HAS_ASYNC__,
};


/*----------------------------------------------------------------------------*
 * RTOS Port                                                                  *
 *----------------------------------------------------------------------------*/
__OVERRIDE_WEAK
uintptr_t arm_2d_port_new_semaphore(void)
{
    // rt_sem_t pSemaphore = RT_NULL;
    // pSemaphore = rt_sem_create("dsem", 0, RT_IPC_FLAG_PRIO);    
    // assert(NULL != pSemaphore);

    // return (uintptr_t)pSemaphore;
    SEM_HANDLE example_sem_hdl = NULL;
    tal_semaphore_create_init(&example_sem_hdl, 0, 1);

    return (uintptr_t)example_sem_hdl;
}

__OVERRIDE_WEAK
void arm_2d_port_free_semaphore(uintptr_t pSemaphore)
{
    SEM_HANDLE evtFlag = (SEM_HANDLE)pSemaphore;
    if (NULL != evtFlag) {
        tal_semaphore_release(evtFlag);
    }
}

__OVERRIDE_WEAK
bool arm_2d_port_wait_for_semaphore(uintptr_t pSemaphore)
{
    SEM_HANDLE evtFlag = (SEM_HANDLE)pSemaphore;
    if (NULL != evtFlag) {
        tal_semaphore_wait(evtFlag, SEM_WAIT_FOREVER);
    }

    return true;
}

__OVERRIDE_WEAK
void arm_2d_port_set_semaphoret(uintptr_t pSemaphore)
{ 
    SEM_HANDLE evtFlag = (SEM_HANDLE)pSemaphore;
    if (NULL != evtFlag) {
        tal_semaphore_post(evtFlag);
    }
}


#include "tdl_display_manage.h"
#include "tkl_timer.h"
#define DELAY_TIME  1 * 100 // us
#define TIMER_ID TUYA_TIMER_NUM_3

static uint64_t sg_count = 0;
/**
 * @brief Timer callback function
 *
 * @param[in] arg:parameters
 * @return none
 */
static void __timer_callback(void *args)
{
    // tkl_log_output("\r\n------------- Timer Callback --------------\r\n");
    sg_count ++;
}

int64_t arm_2d_helper_get_system_timestamp(void)
{
    return (int64_t)sg_count;
}

uint32_t arm_2d_helper_get_reference_clock_frequency(void)
{
    return 10 * 1000; // 10KHz
}

typedef uint8_t lv_color_format_t;
static TDL_DISP_HANDLE_T sg_tdl_disp_hdl = NULL;
static TDL_DISP_DEV_INFO_T sg_display_info;
static TDL_DISP_FRAME_BUFF_T sg_display_fb;
static uint8_t *sg_frame_1 = NULL;

#define DISP_DRAW_BUF_ALIGN    4

/** Represents an area of the screen.*/
typedef struct {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
} disp_area_t;

#if defined(ENABLE_LVGL_DMA2D) && (ENABLE_LVGL_DMA2D == 1)
#include "tkl_dma2d.h"
static void __disp_dma2d_init(void);

static SEM_HANDLE sg_dma2d_finish_sem = NULL;
static bool sg_is_wait_dma2d = false;
static void __disp_dma2d_event_cb(TUYA_DMA2D_IRQ_E type, VOID_T *args)
{
    tal_semaphore_post(sg_dma2d_finish_sem);
}

static void __disp_dma2d_init(void)
{
    tal_semaphore_create_init(&sg_dma2d_finish_sem, 0, 1);

    TUYA_DMA2D_BASE_CFG_T cfg = {
        .cb = __disp_dma2d_event_cb,
    };

    tkl_dma2d_init(&cfg);
}

static void __wait_dma2d_trans_finish(void)
{
    OPERATE_RET ret = OPRT_OK;

    if(sg_dma2d_finish_sem && sg_is_wait_dma2d) {
        ret = tal_semaphore_wait(sg_dma2d_finish_sem, 1000);
        if(ret != OPRT_OK) {
            PR_ERR("wait dma2d finish failed, rt: %d", ret);
        }
        sg_is_wait_dma2d = false;
    }
}

static void __dma2d_drawbuffer_memcpy_syn(const disp_area_t * area, uint8_t * px_map, \
                                          lv_color_format_t cf, TDL_DISP_FRAME_BUFF_T *fb)
{
    TKL_DMA2D_FRAME_INFO_T in_frame = {0};
    TKL_DMA2D_FRAME_INFO_T out_frame = {0};

    if (area == NULL || px_map == NULL || fb == NULL) {
        PR_ERR("Invalid parameter");
        return;
    }

    // Perform memory copy based on color format
    // switch (cf) {
    //     case LV_COLOR_FORMAT_RGB565:
    //         in_frame.type  = TUYA_FRAME_FMT_RGB565;
    //         out_frame.type = TUYA_FRAME_FMT_RGB565;
    //         break;
    //     case LV_COLOR_FORMAT_RGB888:
    //         in_frame.type  = TUYA_FRAME_FMT_RGB888;
    //         out_frame.type = TUYA_FRAME_FMT_RGB888;
    //         break;
    //     default:
    //         PR_ERR("Unsupported color format");
    //         return;
    // }
    in_frame.type  = TUYA_FRAME_FMT_RGB565;
    out_frame.type = TUYA_FRAME_FMT_RGB565;

    in_frame.width  = area->x2 - area->x1 + 1;
    in_frame.height = area->y2 - area->y1 + 1;
    in_frame.pbuf   = px_map;
    in_frame.axis.x_axis   = 0;
    in_frame.axis.y_axis   = 0;
    in_frame.width_cp      = 0;
    in_frame.height_cp     = 0;

    out_frame.width  = fb->width;
    out_frame.height = fb->height;
    out_frame.pbuf   = fb->frame;
    out_frame.axis.x_axis   = area->x1;
    out_frame.axis.y_axis   = area->y1;

    tkl_dma2d_memcpy(&in_frame, &out_frame);

    sg_is_wait_dma2d = true;

    __wait_dma2d_trans_finish();
}

#if defined(ENABLE_LVGL_DUAL_DISP_BUFF) && (ENABLE_LVGL_DUAL_DISP_BUFF == 1)
static void __dma2d_framebuffer_memcpy_async(TDL_DISP_DEV_INFO_T *dev_info,\
                                             uint8_t *dst_frame,\
                                             uint8_t *src_frame)
{
    TKL_DMA2D_FRAME_INFO_T in_frame = {0};
    TKL_DMA2D_FRAME_INFO_T out_frame = {0};

    switch (dev_info->fmt) {
        case TUYA_PIXEL_FMT_RGB565:
            in_frame.type  = TUYA_FRAME_FMT_RGB565;
            out_frame.type = TUYA_FRAME_FMT_RGB565;
            break;
        case TUYA_PIXEL_FMT_RGB888:
            in_frame.type  = TUYA_FRAME_FMT_RGB888;
            out_frame.type = TUYA_FRAME_FMT_RGB888;
            break;
        default:
            PR_ERR("Unsupported color format");
            return;
    }


    in_frame.type  = TUYA_FRAME_FMT_RGB565;
    in_frame.width  = dev_info->width;
    in_frame.height = dev_info->height;
    in_frame.pbuf   = src_frame;
    in_frame.axis.x_axis   = 0;
    in_frame.axis.y_axis   = 0;
    in_frame.width_cp      = 0;
    in_frame.height_cp     = 0;
    
    out_frame.type = TUYA_FRAME_FMT_RGB565;
    out_frame.width  = dev_info->width;
    out_frame.height = dev_info->height;
    out_frame.pbuf   = dst_frame;
    out_frame.axis.x_axis   = 0;
    out_frame.axis.y_axis   = 0;
    out_frame.width_cp      = 0;
    out_frame.height_cp     = 0;

    tkl_dma2d_memcpy(&in_frame, &out_frame);

    sg_is_wait_dma2d = true;
}
#endif
#endif




static uint8_t *__disp_draw_buf_align_alloc(uint32_t size_bytes)
{
    uint8_t *buf_u8= NULL;
    /*Allocate larger memory to be sure it can be aligned as needed*/
    size_bytes += DISP_DRAW_BUF_ALIGN - 1;
    // buf_u8 = (uint8_t *)tkl_system_psram_malloc(size_bytes);
    buf_u8 = (uint8_t *)tal_malloc(size_bytes);
    if (buf_u8) {
        buf_u8 += DISP_DRAW_BUF_ALIGN - 1;
        buf_u8 = (uint8_t *)((uint32_t) buf_u8 & ~(DISP_DRAW_BUF_ALIGN - 1));
    }

    return buf_u8;
}

static uint8_t __disp_get_pixels_size_bytes(TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt)
{
    switch (pixel_fmt) {
        case TUYA_PIXEL_FMT_RGB565:
            return 2;
        case TUYA_PIXEL_FMT_RGB666:
            return 3;
        case TUYA_PIXEL_FMT_RGB888:
            return 3;
        default:
            return 0;
    }
}

void arm2d_disp_init(char *device)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t per_pixel_byte = 0;
    uint32_t frame_len = 0;

    memset(&sg_display_info, 0, sizeof(TDL_DISP_DEV_INFO_T));

    sg_tdl_disp_hdl = tdl_disp_find_dev(device);
    if(NULL == sg_tdl_disp_hdl) {
        PR_ERR("display dev %s not found", device);
        return;
    }

    rt = tdl_disp_dev_get_info(sg_tdl_disp_hdl, &sg_display_info);
    if(rt != OPRT_OK) {
        PR_ERR("get display dev info failed, rt: %d", rt);
        return;
    }

    rt = tdl_disp_dev_open(sg_tdl_disp_hdl);
    if(rt != OPRT_OK) {
            PR_ERR("open display dev failed, rt: %d", rt);
            return;
    }

    tdl_disp_set_brightness(sg_tdl_disp_hdl, 100); // Set brightness to 100%

    if(sg_display_info.fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        frame_len = (sg_display_info.width + 7) / 8 * sg_display_info.height;
    } else if(sg_display_info.fmt == TUYA_PIXEL_FMT_I2){
        frame_len = (sg_display_info.width + 3) / 4 * sg_display_info.height;
    }else {
        per_pixel_byte = __disp_get_pixels_size_bytes(sg_display_info.fmt);
        frame_len = sg_display_info.width * sg_display_info.height * per_pixel_byte;
    }

    sg_display_fb.fmt    = sg_display_info.fmt;
    sg_display_fb.width  = sg_display_info.width;
    sg_display_fb.height = sg_display_info.height;
    sg_display_fb.len    = frame_len;

    sg_frame_1 = __disp_draw_buf_align_alloc(frame_len);
    if(NULL == sg_frame_1) {
        PR_ERR("create display frame buff 1 failed");
        return;
    }
    sg_display_fb.frame  = sg_frame_1;


#if defined(ENABLE_LVGL_DMA2D) && (ENABLE_LVGL_DMA2D == 1)
    __disp_dma2d_init();
#endif

    /* timer init */
    TUYA_TIMER_BASE_CFG_T sg_timer_cfg = {.mode = TUYA_TIMER_MODE_PERIOD, .args = NULL, .cb = __timer_callback};
    TUYA_CALL_ERR_RETURN(tkl_timer_init(TIMER_ID, &sg_timer_cfg));

    /*start timer*/
    TUYA_CALL_ERR_RETURN(tkl_timer_start(TIMER_ID, DELAY_TIME));
    PR_NOTICE("timer %d is start", TIMER_ID);

}

static void __disp_fill_display_framebuffer(const disp_area_t * area, uint8_t * px_map, \
                                            lv_color_format_t cf, TDL_DISP_FRAME_BUFF_T *fb)
{
    uint32_t offset = 0, x = 0, y = 0;

    if (NULL == area || NULL == px_map || NULL == fb) {
        PR_ERR("Invalid parameters: area or px_map or fb is NULL");
        return;
    }
    
    // if(LV_COLOR_FORMAT_RGB565 == cf) {
    //     if(sg_display_info.is_swap) {
    //         lv_draw_sw_rgb565_swap(px_map, lv_area_get_width(area) * lv_area_get_height(area));
    //     }
    // }
#if defined(ENABLE_LVGL_DMA2D) && (ENABLE_LVGL_DMA2D == 1)
        __wait_dma2d_trans_finish();

        __dma2d_drawbuffer_memcpy_syn(area, px_map, cf, fb);
#else
    uint8_t *color_ptr = px_map;
    uint8_t per_pixel_byte = __disp_get_pixels_size_bytes(fb->fmt);
    int32_t width = (int32_t)(area->x2 - area->x1 + 1);

    offset = (area->y1 * fb->width + area->x1) * per_pixel_byte;
    for (y = area->y1; y <= area->y2 && y < fb->height; y++) {
        memcpy(fb->frame + offset, color_ptr, width * per_pixel_byte);
        offset += fb->width * per_pixel_byte; // Move to the next line in the display buffer
        color_ptr += width * per_pixel_byte;
    }
#endif
}

int32_t Disp0_DrawBitmap(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *bitmap)
{
    // printf("Disp0_DrawBitmap: x=%d, y=%d, width=%d, height=%d\r\n", x, y, width, height);
    uint8_t *color_ptr = bitmap;
    disp_area_t target_area = { .x1 = x, .y1 = y, .x2 = x + width - 1, .y2 = y + height - 1 };

    __disp_fill_display_framebuffer(&target_area, color_ptr, 0, &sg_display_fb);

    tdl_disp_dev_flush(sg_tdl_disp_hdl, &sg_display_fb);
    return 0;
}