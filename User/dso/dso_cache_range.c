/**
 * @file dso_cache_range.c
 * @brief cache range 对齐与 clean/invalidate 封装。
 */

#include "dso_cache_range.h"

#include <stddef.h>

/* Cortex-M7 D-Cache cache line 大小，SCB cache 维护接口要求地址和长度按此粒度覆盖。 */
#define DSO_CACHE_LINE_SIZE 32u

/*
*********************************************************************************************************
*   函 数 名: DSO_CacheRange_Align
*   功能说明: 将任意地址范围向外扩展为 32 字节对齐范围，供 D-Cache clean/invalidate 使用。
*   形    参: address   - 原始地址。
*             length    - 原始长度，单位字节。
*             out_range - 输出对齐后的地址和长度。
*   返 回 值: true 表示输出有效；false 表示参数非法、长度为 0 或地址溢出。
*********************************************************************************************************
*/
bool DSO_CacheRange_Align(const void *address,
                          uint32_t length,
                          DSO_CacheRange *out_range)
{
    uintptr_t start;
    uintptr_t end;
    uintptr_t mask;

    if ((address == NULL) || (length == 0u) || (out_range == NULL)) {
        if (out_range != NULL) {
            out_range->start = 0u;
            out_range->length = 0u;
        }
        return false;
    }

    start = (uintptr_t)address;
    mask = (uintptr_t)(DSO_CACHE_LINE_SIZE - 1u);
    end = start + (uintptr_t)length;
    if (end < start) {
        out_range->start = 0u;
        out_range->length = 0u;
        return false;
    }

    out_range->start = start & ~mask;
    end = (end + mask) & ~mask;
    out_range->length = (uint32_t)(end - out_range->start);
    return true;
}
