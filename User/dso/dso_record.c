/**
 * @file dso_record.c
 * @brief Record ingest、single capture freeze、view 窗口管理。
 */

#include "dso_record.h"

#include <string.h>

#if defined(STM32H743xx) || defined(STM32H750xx) || defined(STM32H753xx)
#include "bsp_sdram_layout.h"
#endif

/* 宏定义: DSO_RAM_D1 - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RAM_D1 __attribute__((section(".RAM_D1")))

/* 宏定义: DSO_RECORD_SHORT_SAMPLES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RECORD_SHORT_SAMPLES      (128u * 1024u)
/* 宏定义: DSO_RECORD_MEDIUM_SAMPLES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RECORD_MEDIUM_SAMPLES     (1024u * 1024u)
/* 宏定义: DSO_RECORD_SHORT_CH2_SAMPLES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RECORD_SHORT_CH2_SAMPLES  (DSO_RECORD_SHORT_SAMPLES / 2u)
/* 宏定义: DSO_RECORD_MEDIUM_CH2_SAMPLES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RECORD_MEDIUM_CH2_SAMPLES (DSO_RECORD_MEDIUM_SAMPLES / 2u)
#if defined(STM32H743xx) || defined(STM32H750xx) || defined(STM32H753xx)
/* 宏定义: DSO_RECORD_TOTAL_WORDS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RECORD_TOTAL_WORDS        (SDRAM_RECORD_SIZE / sizeof(uint16_t))
#else
/* 宏定义: DSO_RECORD_TOTAL_WORDS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RECORD_TOTAL_WORDS        (6u * 1024u * 1024u)
#endif
/* 宏定义: DSO_RECORD_DEEP_CH1_SAMPLES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RECORD_DEEP_CH1_SAMPLES   ((DSO_RECORD_TOTAL_WORDS * 2u) / 3u)
/* 宏定义: DSO_RECORD_DEEP_CH2_SAMPLES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RECORD_DEEP_CH2_SAMPLES   (DSO_RECORD_TOTAL_WORDS - DSO_RECORD_DEEP_CH1_SAMPLES)

typedef struct {
    uint32_t capacity_samples;
    uint32_t valid_count;
    uint32_t write_index;
    uint32_t sample_rate_hz;
    uint64_t total_written;
    uint64_t last_view_start_total;
} DSO_RecordChannelState;

typedef struct {
    uint8_t depth_index;
    uint8_t pretrigger_pct;
    bool trigger_valid;
    bool frozen;
    bool single_pending_stop;
    uint64_t trigger_total_index;
    uint64_t stop_after_total;
    DSO_RecordChannelState ch1;
    DSO_RecordChannelState ch2;
} DSO_RecordState;

static DSO_RecordState s_record DSO_RAM_D1;

#if defined(STM32H743xx) || defined(STM32H750xx) || defined(STM32H753xx)
/*
*********************************************************************************************************
*   函 数 名: record_storage_ch1
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t *record_storage_ch1(void)
{
    return (uint16_t *)(uintptr_t)SDRAM_RECORD_BUF;
}

/*
*********************************************************************************************************
*   函 数 名: record_storage_ch2
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t *record_storage_ch2(void)
{
    return (uint16_t *)(uintptr_t)(SDRAM_RECORD_BUF + (DSO_RECORD_DEEP_CH1_SAMPLES * sizeof(uint16_t)));
}
#else
static uint16_t s_host_storage_ch1[DSO_RECORD_DEEP_CH1_SAMPLES];
static uint16_t s_host_storage_ch2[DSO_RECORD_DEEP_CH2_SAMPLES];

/*
*********************************************************************************************************
*   函 数 名: record_storage_ch1
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t *record_storage_ch1(void)
{
    return s_host_storage_ch1;
}

/*
*********************************************************************************************************
*   函 数 名: record_storage_ch2
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t *record_storage_ch2(void)
{
    return s_host_storage_ch2;
}
#endif

/*
*********************************************************************************************************
*   函 数 名: clamp_pct
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t clamp_pct(uint8_t pct)
{
    if (pct < 10u) {
        return 10u;
    }
    if (pct > 90u) {
        return 90u;
    }
    return pct;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_GetDepthSamples
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Record_GetDepthSamples(DSO_RecordDepth depth)
{
    switch (depth) {
    case DSO_RECORD_DEPTH_SHORT:
        return DSO_RECORD_SHORT_SAMPLES;
    case DSO_RECORD_DEPTH_DEEP:
        return DSO_RECORD_DEEP_CH1_SAMPLES;
    case DSO_RECORD_DEPTH_MEDIUM:
    default:
        return DSO_RECORD_MEDIUM_SAMPLES;
    }
}

/*
*********************************************************************************************************
*   函 数 名: record_depth_samples_ch2
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t record_depth_samples_ch2(DSO_RecordDepth depth)
{
    switch (depth) {
    case DSO_RECORD_DEPTH_SHORT:
        return DSO_RECORD_SHORT_CH2_SAMPLES;
    case DSO_RECORD_DEPTH_DEEP:
        return DSO_RECORD_DEEP_CH2_SAMPLES;
    case DSO_RECORD_DEPTH_MEDIUM:
    default:
        return DSO_RECORD_MEDIUM_CH2_SAMPLES;
    }
}

/*
*********************************************************************************************************
*   函 数 名: oldest_total_index
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint64_t oldest_total_index(const DSO_RecordChannelState *channel)
{
    return channel->total_written - (uint64_t)channel->valid_count;
}

/*
*********************************************************************************************************
*   函 数 名: oldest_slot_index
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t oldest_slot_index(const DSO_RecordChannelState *channel)
{
    if (channel->valid_count >= channel->capacity_samples) {
        return channel->write_index;
    }
    return 0u;
}

/*
*********************************************************************************************************
*   函 数 名: slot_from_total_index
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t slot_from_total_index(const DSO_RecordChannelState *channel,
                                      uint64_t total_index)
{
    uint64_t rel = total_index - oldest_total_index(channel);
    return (oldest_slot_index(channel) + (uint32_t)rel) % channel->capacity_samples;
}

/*
*********************************************************************************************************
*   函 数 名: ingest_into_channel
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ingest_into_channel(DSO_RecordChannelState *channel,
                                uint16_t *storage,
                                const uint16_t *samples,
                                uint32_t count,
                                uint32_t sample_rate_hz)
{
    uint32_t original_count;
    uint32_t first;
    uint32_t remain;

    if ((channel == NULL) || (storage == NULL) || (samples == NULL) ||
        (count == 0u) || (channel->capacity_samples == 0u)) {
        return;
    }

    channel->sample_rate_hz = sample_rate_hz;
    original_count = count;

    if (count > channel->capacity_samples) {
        channel->write_index = (channel->write_index + (count % channel->capacity_samples)) %
                               channel->capacity_samples;
        samples += count - channel->capacity_samples;
        count = channel->capacity_samples;
    }

    first = channel->capacity_samples - channel->write_index;
    if (first > count) {
        first = count;
    }

    memcpy(&storage[channel->write_index], samples, first * sizeof(uint16_t));
    remain = count - first;
    if (remain > 0u) {
        memcpy(storage, &samples[first], remain * sizeof(uint16_t));
    }

    channel->write_index += count;
    if (channel->write_index >= channel->capacity_samples) {
        channel->write_index -= channel->capacity_samples;
    }
    if ((channel->capacity_samples - channel->valid_count) < count) {
        channel->valid_count = channel->capacity_samples;
    } else {
        channel->valid_count += count;
    }
    channel->total_written += (uint64_t)original_count;
}

/*
*********************************************************************************************************
*   函 数 名: scale_signed_offset
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int32_t scale_signed_offset(int32_t offset_samples,
                                   uint32_t src_rate_hz,
                                   uint32_t dst_rate_hz)
{
    int64_t scaled;

    if ((offset_samples == 0) || (src_rate_hz == 0u) || (dst_rate_hz == 0u)) {
        return 0;
    }

    scaled = (int64_t)offset_samples * (int64_t)dst_rate_hz;
    if (scaled >= 0) {
        return (int32_t)(scaled / (int64_t)src_rate_hz);
    }
    return -(int32_t)((-scaled) / (int64_t)src_rate_hz);
}

/*
*********************************************************************************************************
*   函 数 名: scale_total_index
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint64_t scale_total_index(uint64_t total_index,
                                  uint32_t src_rate_hz,
                                  uint32_t dst_rate_hz)
{
    if ((src_rate_hz == 0u) || (dst_rate_hz == 0u)) {
        return 0u;
    }
    return (total_index * (uint64_t)dst_rate_hz) / (uint64_t)src_rate_hz;
}

/*
*********************************************************************************************************
*   函 数 名: copy_view_window_channel
*   功能说明: 在深存储模块中完成记录缓冲、触发位置、视图窗口或状态维护。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t copy_view_window_channel(DSO_RecordChannelState *channel,
                                         uint16_t *storage,
                                         uint32_t window_samples,
                                         bool anchor_on_trigger,
                                         uint8_t trigger_position_pct,
                                         int32_t pan_offset_samples,
                                         uint16_t *out_samples,
                                         uint32_t max_samples,
                                         int32_t *out_trigger_index)
{
    uint64_t oldest_total;
    uint64_t newest_total;
    uint64_t start_total;
    uint64_t trigger_total = 0u;
    bool trigger_in_channel = false;

    if (out_trigger_index != NULL) {
        *out_trigger_index = -1;
    }

    if ((channel == NULL) || (storage == NULL) || (out_samples == NULL) ||
        (max_samples == 0u) || (channel->valid_count == 0u)) {
        return 0u;
    }

    if (window_samples == 0u) {
        window_samples = 1u;
    }
    if (window_samples > max_samples) {
        window_samples = max_samples;
    }
    if (window_samples > channel->valid_count) {
        window_samples = channel->valid_count;
    }

    oldest_total = oldest_total_index(channel);
    newest_total = channel->total_written;

    if (anchor_on_trigger && s_record.trigger_valid &&
        (s_record.ch1.sample_rate_hz != 0u) &&
        (channel->sample_rate_hz != 0u)) {
        trigger_total = scale_total_index(s_record.trigger_total_index,
                                          s_record.ch1.sample_rate_hz,
                                          channel->sample_rate_hz);
        trigger_in_channel = (trigger_total >= oldest_total) &&
                             (trigger_total < newest_total);
    }

    if (trigger_in_channel) {
        uint32_t left = (window_samples * clamp_pct(trigger_position_pct)) / 100u;
        if (left >= window_samples) {
            left = window_samples - 1u;
        }
        start_total = (trigger_total > (uint64_t)left)
                    ? (trigger_total - (uint64_t)left)
                    : oldest_total;
    } else {
        start_total = newest_total - (uint64_t)window_samples;
    }

    if (pan_offset_samples != 0) {
        int64_t shifted = (int64_t)start_total + (int64_t)pan_offset_samples;
        if (shifted < (int64_t)oldest_total) {
            shifted = (int64_t)oldest_total;
        }
        start_total = (uint64_t)shifted;
    }

    if ((start_total + (uint64_t)window_samples) > newest_total) {
        start_total = newest_total - (uint64_t)window_samples;
    }
    if (start_total < oldest_total) {
        start_total = oldest_total;
    }

    for (uint32_t i = 0; i < window_samples; i++) {
        out_samples[i] = storage[slot_from_total_index(channel, start_total + (uint64_t)i)];
    }

    channel->last_view_start_total = start_total;

    if (out_trigger_index != NULL &&
        trigger_in_channel &&
        (trigger_total >= start_total) &&
        (trigger_total < (start_total + (uint64_t)window_samples))) {
        *out_trigger_index = (int32_t)(trigger_total - start_total);
    }

    return window_samples;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_Init
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Record_Init(void)
{
    memset(&s_record, 0, sizeof(s_record));
    DSO_Record_Reset(DSO_RECORD_DEPTH_MEDIUM, 50u, 0u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_Reset
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Record_Reset(DSO_RecordDepth depth, uint8_t pretrigger_pct, uint32_t sample_rate_hz)
{
    memset(&s_record, 0, sizeof(s_record));
    s_record.depth_index = (uint8_t)depth;
    s_record.ch1.capacity_samples = DSO_Record_GetDepthSamples(depth);
    s_record.ch1.sample_rate_hz = sample_rate_hz;
    s_record.ch2.capacity_samples = record_depth_samples_ch2(depth);
    s_record.pretrigger_pct = (uint8_t)clamp_pct(pretrigger_pct);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_ClearTrigger
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Record_ClearTrigger(void)
{
    s_record.trigger_valid = false;
    s_record.single_pending_stop = false;
    s_record.stop_after_total = 0u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_Ingest
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Record_Ingest(const uint16_t *samples, uint32_t count, uint32_t sample_rate_hz)
{
    if ((samples == NULL) || (count == 0u) || s_record.frozen || (s_record.ch1.capacity_samples == 0u)) {
        return;
    }

    ingest_into_channel(&s_record.ch1,
                        record_storage_ch1(),
                        samples,
                        count,
                        sample_rate_hz);

    if (s_record.single_pending_stop && (s_record.ch1.total_written >= s_record.stop_after_total)) {
        s_record.frozen = true;
        s_record.single_pending_stop = false;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_IngestCh2
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Record_IngestCh2(const uint16_t *samples, uint32_t count, uint32_t sample_rate_hz)
{
    if ((samples == NULL) || (count == 0u) || s_record.frozen || (s_record.ch2.capacity_samples == 0u)) {
        return;
    }

    ingest_into_channel(&s_record.ch2,
                        record_storage_ch2(),
                        samples,
                        count,
                        sample_rate_hz);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_SetTrigger
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Record_SetTrigger(uint64_t trigger_total_index)
{
    s_record.trigger_valid = true;
    s_record.trigger_total_index = trigger_total_index;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_BeginSingleCapture
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Record_BeginSingleCapture(uint64_t trigger_total_index, uint8_t pretrigger_pct)
{
    uint32_t posttrigger_pct;
    uint64_t posttrigger_needed;

    DSO_Record_SetTrigger(trigger_total_index);
    s_record.pretrigger_pct = (uint8_t)clamp_pct(pretrigger_pct);
    posttrigger_pct = 100u - (uint32_t)s_record.pretrigger_pct;
    posttrigger_needed = ((uint64_t)s_record.ch1.capacity_samples * (uint64_t)posttrigger_pct) / 100u;
    if (posttrigger_needed == 0u) {
        posttrigger_needed = 1u;
    }

    s_record.single_pending_stop = true;
    s_record.stop_after_total = trigger_total_index + posttrigger_needed;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_IsFrozen
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Record_IsFrozen(void)
{
    return s_record.frozen;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_GetTotalWritten
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint64_t DSO_Record_GetTotalWritten(void)
{
    return s_record.ch1.total_written;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_GetTotalWrittenCh2
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint64_t DSO_Record_GetTotalWrittenCh2(void)
{
    return s_record.ch2.total_written;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_CopyViewWindow
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Record_CopyViewWindow(uint32_t window_samples,
                                   bool anchor_on_trigger,
                                   uint8_t trigger_position_pct,
                                   int32_t pan_offset_samples,
                                   uint16_t *out_samples,
                                   uint32_t max_samples,
                                   int32_t *out_trigger_index)
{
    return copy_view_window_channel(&s_record.ch1,
                                    record_storage_ch1(),
                                    window_samples,
                                    anchor_on_trigger,
                                    trigger_position_pct,
                                    pan_offset_samples,
                                    out_samples,
                                    max_samples,
                                    out_trigger_index);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_CopyViewWindowCh2
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Record_CopyViewWindowCh2(uint32_t window_samples,
                                      bool anchor_on_trigger,
                                      uint8_t trigger_position_pct,
                                      int32_t pan_offset_samples,
                                      uint16_t *out_samples,
                                      uint32_t max_samples,
                                      int32_t *out_trigger_index)
{
    int32_t scaled_pan = scale_signed_offset(pan_offset_samples,
                                             s_record.ch1.sample_rate_hz,
                                             s_record.ch2.sample_rate_hz);

    return copy_view_window_channel(&s_record.ch2,
                                    record_storage_ch2(),
                                    window_samples,
                                    anchor_on_trigger,
                                    trigger_position_pct,
                                    scaled_pan,
                                    out_samples,
                                    max_samples,
                                    out_trigger_index);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Record_GetInfo
*   功能说明: 维护深存储记录缓冲、触发索引、窗口读取和记录状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Record_GetInfo(DSO_RecordInfo *out)
{
    uint64_t oldest_total;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->depth_index = s_record.depth_index;
    out->depth_samples = s_record.ch1.capacity_samples;
    out->total_samples = s_record.ch1.valid_count;
    out->sample_rate_hz = s_record.ch1.sample_rate_hz;
    out->trigger_valid = false;
    out->frozen = s_record.frozen;

    if (s_record.ch1.valid_count == 0u) {
        return;
    }

    oldest_total = oldest_total_index(&s_record.ch1);

    if (s_record.ch1.last_view_start_total >= oldest_total) {
        out->view_start = (uint32_t)(s_record.ch1.last_view_start_total - oldest_total);
    }

    if (s_record.trigger_valid &&
        (s_record.trigger_total_index >= oldest_total) &&
        (s_record.trigger_total_index < s_record.ch1.total_written)) {
        out->trigger_valid = true;
        out->trigger_index = (uint32_t)(s_record.trigger_total_index - oldest_total);
    }
}
