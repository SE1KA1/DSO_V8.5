/**
 * @file MainTask_profile.h
 * @brief MainTask 分段耗时统计宏（mtp_acc/mtp_tick）。
 */

/*
 * MainTask 内 emWin/PlotBuf/旋转 分段耗时（可选）
 *
 * 与 FreeRTOS 任务统计使用同一时间基：portGET_RUN_TIME_COUNTER_VALUE()
 * （SysInfoTest TIM6，约 50µs / tick，见 User/SysInfoTest.c）
 *
 * 用法：profile_opts.h 中 MAINTASK_PROFILE_EMWIN_SEGMENTS=1（LCDConf 与 MainTask 共用），全编。
 *
 * 注意：
 * - 统计的是「两次读计数器之间的 tick 差」，MainTask 优先级内若被抢占，差值会混入其它任务时间。
 * - 段 [6] ManualRotateToPhysical 内含 VSYNC 等待里的 vTaskDelay，差值含「等待墙钟时间」，
 *   不是纯 CPU；其它段在无主动阻塞时近似 CPU+DMA 等待（如在 DMA2D while(START) 内仍算本任务时间）。
 */
#ifndef MAINTASK_PROFILE_H
/* 宏定义: MAINTASK_PROFILE_H - 头文件保护宏。 */
#define MAINTASK_PROFILE_H

#include <stdint.h>
#include "profile_opts.h"

#ifndef MAINTASK_PROFILE_ACCUM_FRAMES
/* 宏定义: MAINTASK_PROFILE_ACCUM_FRAMES - 当前模块使用的配置常量。 */
#define MAINTASK_PROFILE_ACCUM_FRAMES  50U
#endif

#if MAINTASK_PROFILE_EMWIN_SEGMENTS

#include "FreeRTOS.h"
#include "task.h"
#include "lcd_rotate_profile.h"
#include <stdio.h>

#if (configGENERATE_RUN_TIME_STATS != 1)
#error MAINTASK_PROFILE_EMWIN_SEGMENTS 需要 configGENERATE_RUN_TIME_STATS==1 与 portGET_RUN_TIME_COUNTER_VALUE
#endif

/* 宏定义: MTP_SEG_COUNT - 计数、阈值、容量或深度配置常量。 */
#define MTP_SEG_COUNT  8

static uint64_t s_mtp_acc[MTP_SEG_COUNT];
static uint32_t s_mtp_prof_t;
static uint32_t s_mtp_frames;

/*
*********************************************************************************************************
*   函 数 名: mtp_mark_start
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline void mtp_mark_start(void)
{
    s_mtp_prof_t = portGET_RUN_TIME_COUNTER_VALUE();
}

/*
*********************************************************************************************************
*   函 数 名: mtp_acc
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline void mtp_acc(int seg)
{
    uint32_t n = portGET_RUN_TIME_COUNTER_VALUE();
    s_mtp_acc[seg] += (uint64_t)(uint32_t)(n - s_mtp_prof_t);
    s_mtp_prof_t = n;
}

/*
*********************************************************************************************************
*   函 数 名: mtp_tick_end_of_frame
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline void mtp_tick_end_of_frame(void)
{
    s_mtp_frames++;
    if (s_mtp_frames < MAINTASK_PROFILE_ACCUM_FRAMES)
        return;

    {
        uint32_t n = s_mtp_frames ? s_mtp_frames : 1U;
        uint64_t tot = 0;
        int i;
        for (i = 0; i < MTP_SEG_COUNT; i++)
            tot += s_mtp_acc[i];

        printf("\r\n======== MainTask profile (TIM6 ticks, ~50us/tick), avg over %lu frames ========\r\n",
               (unsigned long)n);
        printf("[0] UpdateWaveformData        : %6lu tick  (~%lu us)\r\n",
               (unsigned long)(s_mtp_acc[0] / n),
               (unsigned long)((s_mtp_acc[0] / n) * 50U));
        printf("[1] MULTIBUF_Begin + Header   : %6lu tick\r\n", (unsigned long)(s_mtp_acc[1] / n));
        printf("[2] PlotBuf_StartFrame        : %6lu tick\r\n", (unsigned long)(s_mtp_acc[2] / n));
        printf("[3] DrawWaveforms(ToBuf/GUI)  : %6lu tick\r\n", (unsigned long)(s_mtp_acc[3] / n));
        printf("[4] Logical plot Fill+Ticks.. : %6lu tick\r\n", (unsigned long)(s_mtp_acc[4] / n));
        printf("[5] MULTIBUF_End + EndFrame   : %6lu tick\r\n", (unsigned long)(s_mtp_acc[5] / n));
        printf("[6] ManualRotate (total)      : %6lu tick\r\n", (unsigned long)(s_mtp_acc[6] / n));
        {
            uint64_t rw, rv;
            uint64_t b6a[LR_6A_BUCKET_COUNT];
            LcdRotateProfile_TakeRotateSplit(n, &rw, &rv, b6a);
            printf("    [6a] rotate work (pre-VSYNC): %6lu tick  sum below + gap\r\n",
                   (unsigned long)(rw / n));
            printf("    [6b] VSYNC wait (wall)    : %6lu tick  vTaskDelay loop\r\n",
                   (unsigned long)(rv / n));
            printf("    [6a0] phys M2M 480x800    : %6lu  front->dst\r\n",
                   (unsigned long)(b6a[0] / n));
            printf("    [6a1] DCache hdr/body/plot: %6lu  CleanInv/Clean\r\n",
                   (unsigned long)(b6a[1] / n));
            printf("    [6a2] Header CPU rotate    : %6lu\r\n",
                   (unsigned long)(b6a[2] / n));
            printf("    [6a3] Body CPU rotate      : %6lu  PlotBuf path\r\n",
                   (unsigned long)(b6a[3] / n));
            printf("    [6a4] Plot DMA / plot rot  : %6lu  PlotBuf M2M or GUI rot\r\n",
                   (unsigned long)(b6a[4] / n));
            printf("    [6a5] Init frame 0-1       : %6lu  full screen path\r\n",
                   (unsigned long)(b6a[5] / n));
            printf("    [6a6] (unused)             : %6lu\r\n",
                   (unsigned long)(b6a[6] / n));
            printf("    [6a7] no PARTIAL_ROTATE    : %6lu  full clean+rot\r\n",
                   (unsigned long)(b6a[7] / n));
        }
        printf("[7] (reserved)                : %6lu tick\r\n", (unsigned long)(s_mtp_acc[7] / n));
        printf("TOTAL                         : %6lu tick  (~%lu us/frame)\r\n",
               (unsigned long)(tot / n),
               (unsigned long)((tot / n) * 50U));
        printf("================================================================================\r\n\r\n");
    }

    {
        int j;
        for (j = 0; j < MTP_SEG_COUNT; j++)
            s_mtp_acc[j] = 0;
    }
    s_mtp_frames = 0;
}

#else /* !MAINTASK_PROFILE_EMWIN_SEGMENTS */

static inline void mtp_mark_start(void) { }
static inline void mtp_acc(int seg) { (void)seg; }
static inline void mtp_tick_end_of_frame(void) { }

#endif /* MAINTASK_PROFILE_EMWIN_SEGMENTS */

#endif /* MAINTASK_PROFILE_H */
