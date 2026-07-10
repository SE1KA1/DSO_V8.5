/**
 * @file dso_cache_range.h
 * @brief D-Cache 地址范围 32B 对齐辅助。
 */

#ifndef DSO_CACHE_RANGE_H
/* 宏定义: DSO_CACHE_RANGE_H - 头文件保护宏。 */
#define DSO_CACHE_RANGE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    /* 32 字节对齐后的起始地址。 */
    uintptr_t start;
    /* 32 字节对齐后的长度。 */
    uint32_t length;
} DSO_CacheRange;

/*
*********************************************************************************************************
*   函 数 名: DSO_CacheRange_Align
*   功能说明: 将任意地址范围扩展为 Cortex-M7 D-Cache 维护所需的 32 字节对齐范围。
*   形    参: address   - 原始起始地址。
*             length    - 原始长度，单位字节。
*             out_range - 输出对齐后的地址范围。
*   返 回 值: true 表示输出有效，false 表示参数无效或长度为 0。
*********************************************************************************************************
*/
bool DSO_CacheRange_Align(const void *address,
                          uint32_t length,
                          DSO_CacheRange *out_range);

#endif /* DSO_CACHE_RANGE_H */
