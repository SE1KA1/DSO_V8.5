/**
 * @file dso_vofa_firewater.c
 * @brief VoFA 协议打包与发送。
 */

#include "dso_vofa_firewater.h"
#include "dso_config.h"

#include <limits.h>
#include <stdio.h>

/* 宏定义: DSO_RAM_D1 - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RAM_D1 __attribute__((section(".RAM_D1")))

static DSO_VofaFirewaterSendFn s_send_fn = 0;
static DSO_VofaFirewaterReadyFn s_ready_fn = 0;
static uint32_t s_last_sent_ms = UINT32_MAX;
#if DSO_VOFA_FIREWATER_ENABLE
static size_t s_pending_len = 0u;
static size_t s_pending_off = 0u;
static char s_line_buf[DSO_VOFA_FIREWATER_LINE_BUF_SIZE] DSO_RAM_D1;

/*
*********************************************************************************************************
*   函 数 名: sender_ready
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool sender_ready(void)
{
    return (s_ready_fn == 0) ? true : s_ready_fn();
}

/*
*********************************************************************************************************
*   函 数 名: send_pending_chunk
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void send_pending_chunk(void)
{
    size_t remaining;
    size_t chunk_len;

    if ((s_send_fn == 0) || (s_pending_len == 0u) || !sender_ready()) {
        return;
    }

    remaining = s_pending_len - s_pending_off;
    chunk_len = (remaining > DSO_VOFA_FIREWATER_CHUNK_BYTES)
              ? DSO_VOFA_FIREWATER_CHUNK_BYTES
              : remaining;

    if (chunk_len > UINT16_MAX) {
        chunk_len = UINT16_MAX;
    }

    s_send_fn((const uint8_t *)&s_line_buf[s_pending_off], (uint16_t)chunk_len);
    s_pending_off += chunk_len;

    if (s_pending_off >= s_pending_len) {
        s_pending_len = 0u;
        s_pending_off = 0u;
    }
}
#endif

/*
*********************************************************************************************************
*   函 数 名: DSO_VofaFirewater_Init
*   功能说明: 处理 VoFA FireWater 波形数据输出和串口发送节流。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_VofaFirewater_Init(DSO_VofaFirewaterSendFn send_fn,
                            DSO_VofaFirewaterReadyFn ready_fn)
{
    s_send_fn = send_fn;
    s_ready_fn = ready_fn;
    s_last_sent_ms = UINT32_MAX;
#if DSO_VOFA_FIREWATER_ENABLE
    s_pending_len = 0u;
    s_pending_off = 0u;
#endif
}

/*
*********************************************************************************************************
*   函 数 名: DSO_VofaFirewater_ShouldSendNow
*   功能说明: 处理 VoFA FireWater 波形数据输出和串口发送节流。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_VofaFirewater_ShouldSendNow(uint32_t now_ms, uint32_t *last_sent_ms)
{
    if (last_sent_ms == NULL) {
        return false;
    }

    if (*last_sent_ms == UINT32_MAX) {
        *last_sent_ms = now_ms;
        return true;
    }

    if ((uint32_t)(now_ms - *last_sent_ms) < DSO_VOFA_FIREWATER_INTERVAL_MS) {
        return false;
    }

    *last_sent_ms = now_ms;
    return true;
}

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
                                         size_t buf_size)
{
    size_t used = 0u;

    if ((frame == NULL) || (buf == NULL) || (buf_size == 0u)) {
        return 0u;
    }

    buf[0] = '\0';

    for (uint32_t i = 0; i < DSO_DISPLAY_WIDTH; i++) {
        int written = snprintf(&buf[used],
                               buf_size - used,
                               (i + 1u < DSO_DISPLAY_WIDTH) ? "%d," : "%d\n",
                               (int)frame->y_coords[i]);

        if ((written < 0) || ((size_t)written >= (buf_size - used))) {
            buf[0] = '\0';
            return 0u;
        }

        used += (size_t)written;
    }

    return used;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_VofaFirewater_TrySendFrame
*   功能说明: 处理 VoFA FireWater 波形数据输出和串口发送节流。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_VofaFirewater_TrySendFrame(const DSO_Frame *frame, uint32_t now_ms)
{
#if DSO_VOFA_FIREWATER_ENABLE
    size_t len;

    if (s_pending_len > 0u) {
        send_pending_chunk();
        return;
    }

    if ((frame == NULL) || (s_send_fn == 0)) {
        return;
    }

    if (!DSO_VofaFirewater_ShouldSendNow(now_ms, &s_last_sent_ms)) {
        return;
    }

    len = DSO_VofaFirewater_FormatFrameLine(frame, s_line_buf, sizeof(s_line_buf));
    if (len == 0u) {
        return;
    }

    s_pending_len = len;
    s_pending_off = 0u;
    send_pending_chunk();
#else
    (void)frame;
    (void)now_ms;
#endif
}
