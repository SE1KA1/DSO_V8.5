/**
 * @file dso_acq8_sample_unpack.c
 * @brief 8-bit unpack 与多 lane 拷贝实现。
 */

#include "dso_acq8_sample_unpack.h"

#include <stddef.h>

/* 自动识别 uint16_t 中 8-bit 样本位置时最多探测的样本数量。 */
#define DSO_ACQ8_U16_DECODE_PROBE_COUNT 64u

/*
*********************************************************************************************************
*   函 数 名: ring_start
*   功能说明: 计算任意 ring_size 环形缓冲中以 end_pos 结尾的窗口起点。
*   形    参: end_pos   - 窗口结束位置。
*             count     - 窗口长度。
*             ring_size - 环形缓冲长度。
*   返 回 值: 窗口起始位置。
*********************************************************************************************************
*/
static uint32_t ring_start(uint32_t end_pos, uint32_t count, uint32_t ring_size)
{
    return (end_pos >= count)
         ? (end_pos - count)
         : (ring_size - (count - end_pos));
}

/*
*********************************************************************************************************
*   函 数 名: detect_u16_decode_mode
*   功能说明: 根据探测统计结果判断 uint16_t 样本中 8-bit code 的存放方式。
*   形    参: nonzero_count      - 非零样本数量。
*             low_zero_count     - 低 8 位为 0 的样本数量。
*             low2_zero_count    - 低 2 位为 0 的样本数量。
*             shift2_range_count - 右移 2 位后可能落入 8-bit 范围的样本数量。
*             max_sample         - 探测窗口最大样本值。
*   返 回 值: 推荐的 DSO_Acq8U16DecodeMode。
*********************************************************************************************************
*/
static DSO_Acq8U16DecodeMode detect_u16_decode_mode(uint32_t nonzero_count,
                                                    uint32_t low_zero_count,
                                                    uint32_t low2_zero_count,
                                                    uint32_t shift2_range_count,
                                                    uint16_t max_sample)
{
    if (nonzero_count == 0u) {
        return DSO_ACQ8_U16_DECODE_LOW8;
    }

    if ((low_zero_count * 5u) >= (nonzero_count * 4u)) {
        return DSO_ACQ8_U16_DECODE_HIGH8;
    }

    if (((low2_zero_count * 5u) >= (nonzero_count * 4u)) &&
        ((shift2_range_count * 5u) >= (nonzero_count * 4u))) {
        return DSO_ACQ8_U16_DECODE_SHIFT2;
    }

    if (((low2_zero_count * 5u) >= (nonzero_count * 4u)) &&
        (max_sample > 0x03FCu)) {
        return DSO_ACQ8_U16_DECODE_HIGH8;
    }

    return DSO_ACQ8_U16_DECODE_LOW8;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_BuildRingSpans
*   功能说明: 将环形缓冲中 prev_pos 到 pos 的新增区间拆成最多两段连续区间。
*   形    参: ring_size - 环形缓冲长度。
*             prev_pos  - 上一次位置。
*             pos       - 当前位置。
*             out       - 输出区间描述。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_Acq8_BuildRingSpans(uint32_t ring_size,
                             uint32_t prev_pos,
                             uint32_t pos,
                             DSO_Acq8RingSpans *out)
{
    if (out == NULL) {
        return;
    }

    out->start0 = 0u;
    out->count0 = 0u;
    out->start1 = 0u;
    out->count1 = 0u;

    if (ring_size == 0u) {
        return;
    }

    prev_pos %= ring_size;
    pos %= ring_size;

    if (pos == prev_pos) {
        return;
    }

    if (pos > prev_pos) {
        out->start0 = prev_pos;
        out->count0 = pos - prev_pos;
        return;
    }

    out->start0 = prev_pos;
    out->count0 = ring_size - prev_pos;
    out->start1 = 0u;
    out->count1 = pos;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CopyAligned3Bytes
*   功能说明: 从三路 byte lane 中按 lane0/lane1/lane2 顺序交织生成 CH1 样本流。
*   形    参: start0/start1/start2 - 三个 lane 的起点。
*             lane_count           - 每个 lane 参与拷贝的样本数。
*             ring_size            - lane 环形缓冲长度。
*             lane0/lane1/lane2    - 三路 lane 缓冲。
*             out_samples          - 输出样本缓冲。
*             max_samples          - 输出缓冲最多可写样本数。
*   返 回 值: 实际写入的样本数量。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_CopyAligned3Bytes(uint32_t start0,
                                    uint32_t start1,
                                    uint32_t start2,
                                    uint32_t lane_count,
                                    uint32_t ring_size,
                                    const uint8_t *lane0,
                                    const uint8_t *lane1,
                                    const uint8_t *lane2,
                                    uint16_t *out_samples,
                                    uint32_t max_samples)
{
    uint32_t copied = 0u;

    if ((ring_size == 0u) || (lane0 == NULL) || (lane1 == NULL) ||
        (lane2 == NULL) || (out_samples == NULL)) {
        return 0u;
    }

    for (uint32_t i = 0u; i < lane_count; ++i) {
        if ((copied + 3u) > max_samples) {
            break;
        }

        out_samples[copied++] = lane0[(start0 + i) % ring_size];
        out_samples[copied++] = lane1[(start1 + i) % ring_size];
        out_samples[copied++] = lane2[(start2 + i) % ring_size];
    }

    return copied;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CopyAligned2Bytes
*   功能说明: 从两路 byte lane 中按 lane0/lane1 顺序交织生成样本流。
*   形    参: start0/start1 - 两个 lane 的起点。
*             lane_count    - 每个 lane 参与拷贝的样本数。
*             ring_size     - lane 环形缓冲长度。
*             lane0/lane1   - 两路 lane 缓冲。
*             out_samples   - 输出样本缓冲。
*             max_samples   - 输出缓冲最多可写样本数。
*   返 回 值: 实际写入的样本数量。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_CopyAligned2Bytes(uint32_t start0,
                                    uint32_t start1,
                                    uint32_t lane_count,
                                    uint32_t ring_size,
                                    const uint8_t *lane0,
                                    const uint8_t *lane1,
                                    uint16_t *out_samples,
                                    uint32_t max_samples)
{
    uint32_t copied = 0u;

    if ((ring_size == 0u) || (lane0 == NULL) || (lane1 == NULL) ||
        (out_samples == NULL)) {
        return 0u;
    }

    for (uint32_t i = 0u; i < lane_count; ++i) {
        if ((copied + 2u) > max_samples) {
            break;
        }

        out_samples[copied++] = lane0[(start0 + i) % ring_size];
        out_samples[copied++] = lane1[(start1 + i) % ring_size];
    }

    return copied;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_UnpackDualPacked8
*   功能说明: 将 packed 16-bit 数据拆为两个 8-bit lane 样本。
*   形    参: packed - packed 样本，低 8 位为 lane0，高 8 位为 lane1。
*             lane0  - lane0 输出指针，允许为 NULL。
*             lane1  - lane1 输出指针，允许为 NULL。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_Acq8_UnpackDualPacked8(uint16_t packed,
                                uint16_t *lane0,
                                uint16_t *lane1)
{
    if (lane0 != NULL) {
        *lane0 = (uint16_t)(packed & 0x00FFu);
    }
    if (lane1 != NULL) {
        *lane1 = (uint16_t)((packed >> 8) & 0x00FFu);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CopyDualPacked8
*   功能说明: 从 packed uint16_t 环形缓冲中拆包双 lane 样本并写入输出流。
*   形    参: start       - 环形缓冲起点。
*             pair_count  - packed 样本对数量。
*             ring_size   - 环形缓冲长度。
*             packed      - packed 样本缓冲。
*             out_samples - 输出样本缓冲。
*             max_samples - 输出缓冲最多可写样本数。
*   返 回 值: 实际写入的样本数量。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_CopyDualPacked8(uint32_t start,
                                  uint32_t pair_count,
                                  uint32_t ring_size,
                                  const uint16_t *packed,
                                  uint16_t *out_samples,
                                  uint32_t max_samples)
{
    uint32_t copied = 0u;

    if ((ring_size == 0u) || (packed == NULL) || (out_samples == NULL)) {
        return 0u;
    }

    for (uint32_t i = 0u; i < pair_count; ++i) {
        uint16_t lane0;
        uint16_t lane1;
        uint16_t word;

        if ((copied + 2u) > max_samples) {
            break;
        }

        word = packed[(start + i) % ring_size];
        DSO_Acq8_UnpackDualPacked8(word, &lane0, &lane1);
        out_samples[copied++] = lane0;
        out_samples[copied++] = lane1;
    }

    return copied;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_UnpackDualPackedLow16Word
*   功能说明: 从 uint32_t 低 16 位中拆出两个 8-bit lane 样本。
*   形    参: packed_word - packed word，低 16 位有效。
*             lane0       - lane0 输出指针。
*             lane1       - lane1 输出指针。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_Acq8_UnpackDualPackedLow16Word(uint32_t packed_word,
                                        uint16_t *lane0,
                                        uint16_t *lane1)
{
    DSO_Acq8_UnpackDualPacked8((uint16_t)(packed_word & 0x0000FFFFu),
                               lane0,
                               lane1);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CopyDualPackedLow16WordRing
*   功能说明: 从 uint32_t 环形缓冲低 16 位拆包双 lane 样本。
*   形    参: start_word   - 起始 word 位置。
*             word_count   - word 数量。
*             ring_size    - 环形缓冲长度。
*             packed_words - packed word 缓冲。
*             out_samples  - 输出样本缓冲。
*             max_samples  - 输出缓冲最多可写样本数。
*   返 回 值: 实际写入的样本数量。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_CopyDualPackedLow16WordRing(uint32_t start_word,
                                              uint32_t word_count,
                                              uint32_t ring_size,
                                              const uint32_t *packed_words,
                                              uint16_t *out_samples,
                                              uint32_t max_samples)
{
    uint32_t copied = 0u;

    if ((ring_size == 0u) || (packed_words == NULL) || (out_samples == NULL)) {
        return 0u;
    }

    for (uint32_t i = 0u; i < word_count; ++i) {
        uint16_t lane0;
        uint16_t lane1;

        if ((copied + 2u) > max_samples) {
            break;
        }

        DSO_Acq8_UnpackDualPackedLow16Word(packed_words[(start_word + i) % ring_size],
                                           &lane0,
                                           &lane1);
        out_samples[copied++] = lane0;
        out_samples[copied++] = lane1;
    }

    return copied;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CopyLinearBytes
*   功能说明: 将线性 uint8_t 样本扩展复制到 uint16_t 输出缓冲。
*   形    参: samples     - 输入 byte 样本。
*             count       - 输入样本数量。
*             out_samples - 输出样本缓冲。
*             max_samples - 输出缓冲最多可写样本数。
*   返 回 值: 实际写入的样本数量。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_CopyLinearBytes(const uint8_t *samples,
                                  uint32_t count,
                                  uint16_t *out_samples,
                                  uint32_t max_samples)
{
    uint32_t copied;

    if ((samples == NULL) || (out_samples == NULL) || (max_samples == 0u)) {
        return 0u;
    }

    copied = (count > max_samples) ? max_samples : count;
    for (uint32_t i = 0u; i < copied; ++i) {
        out_samples[i] = samples[i];
    }

    return copied;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_LowByteFromU16
*   功能说明: 从 uint16_t 样本中提取低 8 位 code。
*   形    参: sample - 输入样本。
*   返 回 值: 低 8 位样本值。
*********************************************************************************************************
*/
uint16_t DSO_Acq8_LowByteFromU16(uint16_t sample)
{
    return (uint16_t)(sample & 0x00FFu);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_DetectU16DecodeModeLinear
*   功能说明: 在线性 uint16_t 样本窗口中自动判断 8-bit 样本解码方式。
*   形    参: samples - 输入样本。
*             count   - 输入样本数量。
*   返 回 值: 推荐解码模式。
*********************************************************************************************************
*/
DSO_Acq8U16DecodeMode DSO_Acq8_DetectU16DecodeModeLinear(const uint16_t *samples,
                                                         uint32_t count)
{
    uint32_t probe_count;
    uint32_t nonzero_count = 0u;
    uint32_t low_zero_count = 0u;
    uint32_t low2_zero_count = 0u;
    uint32_t shift2_range_count = 0u;
    uint16_t max_sample = 0u;

    if ((samples == NULL) || (count == 0u)) {
        return DSO_ACQ8_U16_DECODE_LOW8;
    }

    probe_count = (count > DSO_ACQ8_U16_DECODE_PROBE_COUNT)
                ? DSO_ACQ8_U16_DECODE_PROBE_COUNT
                : count;

    for (uint32_t i = 0u; i < probe_count; ++i) {
        uint16_t sample = samples[i];

        if (sample == 0u) {
            continue;
        }

        ++nonzero_count;
        if ((sample & 0x00FFu) == 0u) {
            ++low_zero_count;
        }
        if ((sample & 0x0003u) == 0u) {
            ++low2_zero_count;
        }
        if (sample <= 0x03FCu) {
            ++shift2_range_count;
        }
        if (sample > max_sample) {
            max_sample = sample;
        }
    }

    return detect_u16_decode_mode(nonzero_count,
                                  low_zero_count,
                                  low2_zero_count,
                                  shift2_range_count,
                                  max_sample);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_DetectU16DecodeModeRing
*   功能说明: 在 uint16_t 环形窗口中自动判断 8-bit 样本解码方式。
*   形    参: samples      - 输入环形缓冲。
*             ring_size    - 环形缓冲长度。
*             end_pos      - 窗口结束位置。
*             window_count - 窗口长度。
*   返 回 值: 推荐解码模式。
*********************************************************************************************************
*/
DSO_Acq8U16DecodeMode DSO_Acq8_DetectU16DecodeModeRing(const uint16_t *samples,
                                                       uint32_t ring_size,
                                                       uint32_t end_pos,
                                                       uint32_t window_count)
{
    uint32_t probe_count;
    uint32_t start;
    uint32_t nonzero_count = 0u;
    uint32_t low_zero_count = 0u;
    uint32_t low2_zero_count = 0u;
    uint32_t shift2_range_count = 0u;
    uint16_t max_sample = 0u;

    if ((samples == NULL) || (ring_size == 0u) || (window_count == 0u)) {
        return DSO_ACQ8_U16_DECODE_LOW8;
    }

    if (window_count > ring_size) {
        window_count = ring_size;
    }

    probe_count = (window_count > DSO_ACQ8_U16_DECODE_PROBE_COUNT)
                ? DSO_ACQ8_U16_DECODE_PROBE_COUNT
                : window_count;
    start = ring_start(end_pos, probe_count, ring_size);

    for (uint32_t i = 0u; i < probe_count; ++i) {
        uint16_t sample = samples[(start + i) % ring_size];

        if (sample == 0u) {
            continue;
        }

        ++nonzero_count;
        if ((sample & 0x00FFu) == 0u) {
            ++low_zero_count;
        }
        if ((sample & 0x0003u) == 0u) {
            ++low2_zero_count;
        }
        if (sample <= 0x03FCu) {
            ++shift2_range_count;
        }
        if (sample > max_sample) {
            max_sample = sample;
        }
    }

    return detect_u16_decode_mode(nonzero_count,
                                  low_zero_count,
                                  low2_zero_count,
                                  shift2_range_count,
                                  max_sample);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_DecodeU16Sample
*   功能说明: 按指定解码模式从 uint16_t 样本中取出 8-bit code。
*   形    参: sample - 输入 uint16_t 样本。
*             mode   - 解码模式。
*   返 回 值: 解码后的 8-bit code，存放在 uint16_t 中。
*********************************************************************************************************
*/
uint16_t DSO_Acq8_DecodeU16Sample(uint16_t sample,
                                  DSO_Acq8U16DecodeMode mode)
{
    switch (mode) {
    case DSO_ACQ8_U16_DECODE_SHIFT2:
        return (uint16_t)((sample >> 2) & 0x00FFu);
    case DSO_ACQ8_U16_DECODE_HIGH8:
        return (uint16_t)((sample >> 8) & 0x00FFu);
    case DSO_ACQ8_U16_DECODE_LOW8:
    default:
        return DSO_Acq8_LowByteFromU16(sample);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CopyLinearLowByte16
*   功能说明: 自动识别 uint16_t 样本编码方式，并复制为 8-bit code 输出流。
*   形    参: samples     - 输入 uint16_t 样本。
*             count       - 输入样本数量。
*             out_samples - 输出样本缓冲。
*             max_samples - 输出缓冲最多可写样本数。
*   返 回 值: 实际写入的样本数量。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_CopyLinearLowByte16(const uint16_t *samples,
                                      uint32_t count,
                                      uint16_t *out_samples,
                                      uint32_t max_samples)
{
    uint32_t copied;
    DSO_Acq8U16DecodeMode mode;

    if ((samples == NULL) || (out_samples == NULL) || (max_samples == 0u)) {
        return 0u;
    }

    copied = (count > max_samples) ? max_samples : count;
    mode = DSO_Acq8_DetectU16DecodeModeLinear(samples, copied);
    for (uint32_t i = 0u; i < copied; ++i) {
        out_samples[i] = DSO_Acq8_DecodeU16Sample(samples[i], mode);
    }

    return copied;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_SummarizeRingBytes
*   功能说明: 统计 uint8_t 环形窗口中的最小值、最大值和均值。
*   形    参: samples      - 输入环形缓冲。
*             ring_size    - 环形缓冲长度。
*             end_pos      - 窗口结束位置。
*             window_count - 窗口长度。
*             out_vmin     - 输出最小值，允许为 NULL。
*             out_vmax     - 输出最大值，允许为 NULL。
*             out_mean     - 输出均值，允许为 NULL。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_Acq8_SummarizeRingBytes(const uint8_t *samples,
                                 uint32_t ring_size,
                                 uint32_t end_pos,
                                 uint32_t window_count,
                                 uint16_t *out_vmin,
                                 uint16_t *out_vmax,
                                 uint16_t *out_mean)
{
    uint32_t sum = 0u;
    uint32_t start;
    uint16_t vmin;
    uint16_t vmax;

    if ((samples == NULL) || (ring_size == 0u) || (window_count == 0u)) {
        if (out_vmin != NULL) {
            *out_vmin = 0u;
        }
        if (out_vmax != NULL) {
            *out_vmax = 0u;
        }
        if (out_mean != NULL) {
            *out_mean = 0u;
        }
        return;
    }

    if (window_count > ring_size) {
        window_count = ring_size;
    }

    start = ring_start(end_pos, window_count, ring_size);
    vmin = samples[start];
    vmax = samples[start];

    for (uint32_t i = 0u; i < window_count; ++i) {
        uint16_t v = samples[(start + i) % ring_size];
        if (v < vmin) {
            vmin = v;
        }
        if (v > vmax) {
            vmax = v;
        }
        sum += v;
    }

    if (out_vmin != NULL) {
        *out_vmin = vmin;
    }
    if (out_vmax != NULL) {
        *out_vmax = vmax;
    }
    if (out_mean != NULL) {
        *out_mean = (uint16_t)(sum / window_count);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_SummarizeLinearBytes
*   功能说明: 统计 uint8_t 线性窗口中的最小值、最大值和均值。
*   形    参: samples  - 输入样本。
*             count    - 输入样本数量。
*             out_vmin - 输出最小值，允许为 NULL。
*             out_vmax - 输出最大值，允许为 NULL。
*             out_mean - 输出均值，允许为 NULL。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_Acq8_SummarizeLinearBytes(const uint8_t *samples,
                                   uint32_t count,
                                   uint16_t *out_vmin,
                                   uint16_t *out_vmax,
                                   uint16_t *out_mean)
{
    uint32_t sum = 0u;
    uint16_t vmin;
    uint16_t vmax;

    if ((samples == NULL) || (count == 0u)) {
        if (out_vmin != NULL) {
            *out_vmin = 0u;
        }
        if (out_vmax != NULL) {
            *out_vmax = 0u;
        }
        if (out_mean != NULL) {
            *out_mean = 0u;
        }
        return;
    }

    vmin = samples[0];
    vmax = samples[0];
    for (uint32_t i = 0u; i < count; ++i) {
        uint16_t v = samples[i];
        if (v < vmin) {
            vmin = v;
        }
        if (v > vmax) {
            vmax = v;
        }
        sum += v;
    }

    if (out_vmin != NULL) {
        *out_vmin = vmin;
    }
    if (out_vmax != NULL) {
        *out_vmax = vmax;
    }
    if (out_mean != NULL) {
        *out_mean = (uint16_t)(sum / count);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_SummarizeRingLowByte16
*   功能说明: 自动解码 uint16_t 环形窗口中的 8-bit 样本，并统计最小值、最大值和均值。
*   形    参: samples      - 输入 uint16_t 环形缓冲。
*             ring_size    - 环形缓冲长度。
*             end_pos      - 窗口结束位置。
*             window_count - 窗口长度。
*             out_vmin     - 输出最小值，允许为 NULL。
*             out_vmax     - 输出最大值，允许为 NULL。
*             out_mean     - 输出均值，允许为 NULL。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_Acq8_SummarizeRingLowByte16(const uint16_t *samples,
                                     uint32_t ring_size,
                                     uint32_t end_pos,
                                     uint32_t window_count,
                                     uint16_t *out_vmin,
                                     uint16_t *out_vmax,
                                     uint16_t *out_mean)
{
    uint32_t sum = 0u;
    uint32_t start;
    DSO_Acq8U16DecodeMode mode;
    uint16_t vmin;
    uint16_t vmax;

    if ((samples == NULL) || (ring_size == 0u) || (window_count == 0u)) {
        if (out_vmin != NULL) {
            *out_vmin = 0u;
        }
        if (out_vmax != NULL) {
            *out_vmax = 0u;
        }
        if (out_mean != NULL) {
            *out_mean = 0u;
        }
        return;
    }

    if (window_count > ring_size) {
        window_count = ring_size;
    }

    start = ring_start(end_pos, window_count, ring_size);
    mode = DSO_Acq8_DetectU16DecodeModeRing(samples, ring_size, end_pos, window_count);
    vmin = DSO_Acq8_DecodeU16Sample(samples[start], mode);
    vmax = vmin;

    for (uint32_t i = 0u; i < window_count; ++i) {
        uint16_t v = DSO_Acq8_DecodeU16Sample(samples[(start + i) % ring_size], mode);
        if (v < vmin) {
            vmin = v;
        }
        if (v > vmax) {
            vmax = v;
        }
        sum += v;
    }

    if (out_vmin != NULL) {
        *out_vmin = vmin;
    }
    if (out_vmax != NULL) {
        *out_vmax = vmax;
    }
    if (out_mean != NULL) {
        *out_mean = (uint16_t)(sum / window_count);
    }
}
