/**
 * @file lcd_rotate_profile.c
 * @brief LCD 旋转 profiling 累计与串口输出。
 */

#include "profile_opts.h"
#include "lcd_rotate_profile.h"

#if MAINTASK_PROFILE_EMWIN_SEGMENTS

#include "FreeRTOS.h"
#include "task.h"

#if (configGENERATE_RUN_TIME_STATS != 1)
#error lcd_rotate_profile 需要 configGENERATE_RUN_TIME_STATS 与 portGET_RUN_TIME_COUNTER_VALUE
#endif

static uint32_t s_rp_t;
static uint64_t s_rp_sum_work;
static uint64_t s_rp_sum_vsync;

static uint32_t s_6a_sub_t;
static uint64_t s_6a_bucket[LR_6A_BUCKET_COUNT];

/*
*********************************************************************************************************
*   函 数 名: LcdRotateProfile_ManualRotate_Enter
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LcdRotateProfile_ManualRotate_Enter(void)
{
    uint32_t t = portGET_RUN_TIME_COUNTER_VALUE();
    s_rp_t     = t;
    s_6a_sub_t = t;
}

/*
*********************************************************************************************************
*   函 数 名: LcdRotateProfile_6a_SubTick
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LcdRotateProfile_6a_SubTick(unsigned idx)
{
    unsigned i = idx;
    if (i >= LR_6A_BUCKET_COUNT)
        i = LR_6A_BUCKET_COUNT - 1U;
    {
        uint32_t n = portGET_RUN_TIME_COUNTER_VALUE();
        s_6a_bucket[i] += (uint64_t)(uint32_t)(n - s_6a_sub_t);
        s_6a_sub_t = n;
    }
}

/*
*********************************************************************************************************
*   函 数 名: LcdRotateProfile_BeforeVsyncWait
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LcdRotateProfile_BeforeVsyncWait(void)
{
    uint32_t n = portGET_RUN_TIME_COUNTER_VALUE();
    s_rp_sum_work += (uint64_t)(uint32_t)(n - s_rp_t);
    s_rp_t = n;
}

/*
*********************************************************************************************************
*   函 数 名: LcdRotateProfile_AfterVsyncWait
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LcdRotateProfile_AfterVsyncWait(void)
{
    uint32_t n = portGET_RUN_TIME_COUNTER_VALUE();
    s_rp_sum_vsync += (uint64_t)(uint32_t)(n - s_rp_t);
}

/*
*********************************************************************************************************
*   函 数 名: LcdRotateProfile_TakeRotateSplit
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LcdRotateProfile_TakeRotateSplit(uint32_t frame_count,
    uint64_t *out_work_ticks, uint64_t *out_vsync_ticks,
    uint64_t *out_6a_buckets)
{
    unsigned j;
    (void)frame_count;
    if (out_work_ticks)
        *out_work_ticks = s_rp_sum_work;
    if (out_vsync_ticks)
        *out_vsync_ticks = s_rp_sum_vsync;
    s_rp_sum_work  = 0;
    s_rp_sum_vsync = 0;

    if (out_6a_buckets) {
        for (j = 0U; j < LR_6A_BUCKET_COUNT; j++) {
            out_6a_buckets[j] = s_6a_bucket[j];
            s_6a_bucket[j]    = 0;
        }
    }
}

#else /* !MAINTASK_PROFILE_EMWIN_SEGMENTS */

void LcdRotateProfile_ManualRotate_Enter(void) { }
void LcdRotateProfile_BeforeVsyncWait(void) { }
void LcdRotateProfile_AfterVsyncWait(void) { }

void LcdRotateProfile_6a_SubTick(unsigned idx) { (void)idx; }

/*
*********************************************************************************************************
*   函 数 名: LcdRotateProfile_TakeRotateSplit
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LcdRotateProfile_TakeRotateSplit(uint32_t frame_count,
    uint64_t *out_work_ticks, uint64_t *out_vsync_ticks,
    uint64_t *out_6a_buckets)
{
    unsigned j;
    (void)frame_count;
    if (out_work_ticks)
        *out_work_ticks = 0;
    if (out_vsync_ticks)
        *out_vsync_ticks = 0;
    if (out_6a_buckets) {
        for (j = 0U; j < LR_6A_BUCKET_COUNT; j++)
            out_6a_buckets[j] = 0;
    }
}

#endif /* MAINTASK_PROFILE_EMWIN_SEGMENTS */
