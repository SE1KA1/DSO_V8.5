/**
 * @file lcd_rotate_profile.h
 * @brief LCD 旋转各阶段 DWT profiling 宏。
 */

#ifndef LCD_ROTATE_PROFILE_H
/* 宏定义: LCD_ROTATE_PROFILE_H - 头文件保护宏。 */
#define LCD_ROTATE_PROFILE_H

#include <stdint.h>

/* 宏定义: LR_6A_BUCKET_COUNT - 计数、阈值、容量或深度配置常量。 */
#define LR_6A_BUCKET_COUNT  8U

/*
*********************************************************************************************************
*   函 数 名: LcdRotateProfile_ManualRotate_Enter
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LcdRotateProfile_ManualRotate_Enter(void);
/* 6a 子阶段：从上一次 tick（或 Enter）到本调用之间的 tick 记入 bucket[idx] */
void LcdRotateProfile_6a_SubTick(unsigned idx);
/*
*********************************************************************************************************
*   函 数 名: LcdRotateProfile_BeforeVsyncWait
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LcdRotateProfile_BeforeVsyncWait(void);
/*
*********************************************************************************************************
*   函 数 名: LcdRotateProfile_AfterVsyncWait
*   功能说明: 记录 LCD 旋转或 MainTask 渲染阶段的性能统计数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LcdRotateProfile_AfterVsyncWait(void);

/* 取出累计 tick 并清零；out_6a 可为 NULL；out_6a 为 LR_6A_BUCKET_COUNT 个 uint64 的和（未除帧数） */
void LcdRotateProfile_TakeRotateSplit(uint32_t frame_count,
    uint64_t *out_work_ticks, uint64_t *out_vsync_ticks,
    uint64_t *out_6a_buckets);

#endif /* LCD_ROTATE_PROFILE_H */
