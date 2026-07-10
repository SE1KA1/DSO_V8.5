/**
 * @file dso_acq8_sample_unpack.h
 * @brief 8-bit 样本解包、lane 检测与 copy helper。
 */

#ifndef DSO_ACQ8_SAMPLE_UNPACK_H
/* 宏定义: DSO_ACQ8_SAMPLE_UNPACK_H - 头文件保护宏。 */
#define DSO_ACQ8_SAMPLE_UNPACK_H

#include <stdint.h>

typedef struct
{
    /* 第一段连续窗口起点。 */
    uint32_t start0;
    /* 第一段连续窗口长度。 */
    uint32_t count0;
    /* 回绕后的第二段窗口起点。 */
    uint32_t start1;
    /* 回绕后的第二段窗口长度。 */
    uint32_t count1;
} DSO_Acq8RingSpans;

typedef enum
{
    /* 从 uint16_t 的低 8 位取 8-bit 样本。 */
    DSO_ACQ8_U16_DECODE_LOW8 = 0,
    /* 样本左对齐或带低 2 位空位时，右移 2 位再取 8-bit。 */
    DSO_ACQ8_U16_DECODE_SHIFT2,
    /* 从 uint16_t 的高 8 位取 8-bit 样本。 */
    DSO_ACQ8_U16_DECODE_HIGH8,
} DSO_Acq8U16DecodeMode;

/* 按 lane0/lane1/lane2 顺序交织拷贝三路 8-bit 样本。 */
uint32_t DSO_Acq8_CopyAligned3Bytes(uint32_t start0,
                                    uint32_t start1,
                                    uint32_t start2,
                                    uint32_t lane_count,
                                    uint32_t ring_size,
                                    const uint8_t *lane0,
                                    const uint8_t *lane1,
                                    const uint8_t *lane2,
                                    uint16_t *out_samples,
                                    uint32_t max_samples);

/* 按 lane0/lane1 顺序交织拷贝两路 8-bit 样本。 */
uint32_t DSO_Acq8_CopyAligned2Bytes(uint32_t start0,
                                    uint32_t start1,
                                    uint32_t lane_count,
                                    uint32_t ring_size,
                                    const uint8_t *lane0,
                                    const uint8_t *lane1,
                                    uint16_t *out_samples,
                                    uint32_t max_samples);

/* 将一个 packed uint16_t 拆成两个 8-bit lane 样本。 */
void DSO_Acq8_UnpackDualPacked8(uint16_t packed,
                                uint16_t *lane0,
                                uint16_t *lane1);

/* 从 packed uint16_t 环形缓冲中拷贝双 lane 8-bit 样本。 */
uint32_t DSO_Acq8_CopyDualPacked8(uint32_t start,
                                  uint32_t pair_count,
                                  uint32_t ring_size,
                                  const uint16_t *packed,
                                  uint16_t *out_samples,
                                  uint32_t max_samples);

/* 从 uint32_t 低 16 位中拆出两个 packed 8-bit lane 样本。 */
void DSO_Acq8_UnpackDualPackedLow16Word(uint32_t packed_word,
                                        uint16_t *lane0,
                                        uint16_t *lane1);

/* 从 uint32_t word 环形缓冲的低 16 位中拷贝双 lane 8-bit 样本。 */
uint32_t DSO_Acq8_CopyDualPackedLow16WordRing(uint32_t start_word,
                                              uint32_t word_count,
                                              uint32_t ring_size,
                                              const uint32_t *packed_words,
                                              uint16_t *out_samples,
                                              uint32_t max_samples);

/* 将线性 uint8_t 样本扩展为 uint16_t 输出。 */
uint32_t DSO_Acq8_CopyLinearBytes(const uint8_t *samples,
                                  uint32_t count,
                                  uint16_t *out_samples,
                                  uint32_t max_samples);

/* 统计 uint8_t 环形窗口的最小值、最大值和均值。 */
void DSO_Acq8_SummarizeRingBytes(const uint8_t *samples,
                                 uint32_t ring_size,
                                 uint32_t end_pos,
                                 uint32_t window_count,
                                 uint16_t *out_vmin,
                                 uint16_t *out_vmax,
                                 uint16_t *out_mean);

/* 统计 uint8_t 线性窗口的最小值、最大值和均值。 */
void DSO_Acq8_SummarizeLinearBytes(const uint8_t *samples,
                                   uint32_t count,
                                   uint16_t *out_vmin,
                                   uint16_t *out_vmax,
                                   uint16_t *out_mean);

/* 从 uint16_t 样本中取低 8 位。 */
uint16_t DSO_Acq8_LowByteFromU16(uint16_t sample);

/* 在线性 uint16_t 样本中自动判断 8-bit 样本解码方式。 */
DSO_Acq8U16DecodeMode DSO_Acq8_DetectU16DecodeModeLinear(const uint16_t *samples,
                                                         uint32_t count);

/* 在 uint16_t 环形窗口中自动判断 8-bit 样本解码方式。 */
DSO_Acq8U16DecodeMode DSO_Acq8_DetectU16DecodeModeRing(const uint16_t *samples,
                                                       uint32_t ring_size,
                                                       uint32_t end_pos,
                                                       uint32_t window_count);

/* 按指定模式从 uint16_t 中解出 8-bit 样本。 */
uint16_t DSO_Acq8_DecodeU16Sample(uint16_t sample,
                                  DSO_Acq8U16DecodeMode mode);

/* 拷贝线性 uint16_t 样本并自动解码为 8-bit code。 */
uint32_t DSO_Acq8_CopyLinearLowByte16(const uint16_t *samples,
                                      uint32_t count,
                                      uint16_t *out_samples,
                                      uint32_t max_samples);

/* 统计 uint16_t 环形窗口解码后的最小值、最大值和均值。 */
void DSO_Acq8_SummarizeRingLowByte16(const uint16_t *samples,
                                     uint32_t ring_size,
                                     uint32_t end_pos,
                                     uint32_t window_count,
                                     uint16_t *out_vmin,
                                     uint16_t *out_vmax,
                                     uint16_t *out_mean);

/* 将 prev_pos 到 pos 的环形增量拆成最多两段连续窗口。 */
void DSO_Acq8_BuildRingSpans(uint32_t ring_size,
                             uint32_t prev_pos,
                             uint32_t pos,
                             DSO_Acq8RingSpans *out);

#endif /* DSO_ACQ8_SAMPLE_UNPACK_H */
