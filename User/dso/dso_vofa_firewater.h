/**
 * @file dso_vofa_firewater.h
 * @brief VoFA+ FireWater 波形串口输出。
 */

#ifndef DSO_VOFA_FIREWATER_H
/* 宏定义: DSO_VOFA_FIREWATER_H - 头文件保护宏。 */
#define DSO_VOFA_FIREWATER_H

#include "dso_process.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*DSO_VofaFirewaterSendFn)(const uint8_t *data, uint16_t len);
typedef bool (*DSO_VofaFirewaterReadyFn)(void);

/*
*********************************************************************************************************
*   函 数 名: DSO_VofaFirewater_Init
*   功能说明: 处理 VoFA FireWater 波形数据输出和串口发送节流。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_VofaFirewater_Init(DSO_VofaFirewaterSendFn send_fn,
                            DSO_VofaFirewaterReadyFn ready_fn);
/*
*********************************************************************************************************
*   函 数 名: DSO_VofaFirewater_TrySendFrame
*   功能说明: 处理 VoFA FireWater 波形数据输出和串口发送节流。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_VofaFirewater_TrySendFrame(const DSO_Frame *frame, uint32_t now_ms);

/*
*********************************************************************************************************
*   函 数 名: DSO_VofaFirewater_FormatFrameLine
*   功能说明: 处理 VoFA FireWater 波形数据输出和串口发送节流。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_VofaFirewater_FormatFrameLine(const DSO_Frame *frame,
                                         char *buf,
                                         size_t buf_size);
/*
*********************************************************************************************************
*   函 数 名: DSO_VofaFirewater_ShouldSendNow
*   功能说明: 处理 VoFA FireWater 波形数据输出和串口发送节流。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_VofaFirewater_ShouldSendNow(uint32_t now_ms, uint32_t *last_sent_ms);

#endif /* DSO_VOFA_FIREWATER_H */
