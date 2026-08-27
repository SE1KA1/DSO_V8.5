/**
 * @file fmc16_crc.h
 * @brief FMC16 协议 CRC 计算 —— CRC-16/CCITT-FALSE 和 CRC-32/MPEG-2。
 */
#ifndef FMC16_CRC_H
#define FMC16_CRC_H

#include "fmc16.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CRC-16/CCITT-FALSE
 *   Poly = 0x1021, Init = 0xFFFF
 *   RefIn = false, RefOut = false, XorOut = 0x0000
 *
 * 用于包头 CRC (W2~W11)。
 *
 * @param data   16-bit word 数组
 * @param count  数组长度 (word 数)
 * @return 16-bit CRC
 */
uint16_t fmc16_crc16(const uint16_t *data, uint16_t count);

/**
 * @brief CRC-32/MPEG-2
 *   Poly = 0x04C11DB7, Init = 0xFFFFFFFF
 *   RefIn = false, RefOut = false, XorOut = 0x00000000
 *
 * 用于整包 CRC (W2~最后一个 payload word, 含 W12 的 Header CRC16)。
 * 每个 16-bit word 按 bit15→bit0 顺序输入 CRC 引擎。
 *
 * @param data   16-bit word 数组
 * @param count  数组长度 (word 数)
 * @return 32-bit CRC
 */
uint32_t fmc16_crc32(const uint16_t *data, uint16_t count);

/**
 * @brief 使用协议 §5 的 ACK golden vector 自检。
 *
 * Golden vector（19 words）:
 *   0xA55A 0x5AA5 0x100D 0x0013 0x0300 0x0100
 *   0x0000 0x0004 0x0000 0x0001 0x0000 0x0001 0x0141
 *   0x0030 0x0001 0x0001 0x0000 0xF1E8 0x829E
 *
 * 验证项:
 *   - Header CRC16(W2~W11) = 0x0141
 *   - Packet CRC32(W2~W17) = 0xF1E8829E
 *
 * @return 1=通过, 0=失败（串口输出详细诊断）
 */
int fmc16_crc_selftest(void);

#ifdef __cplusplus
}
#endif
#endif /* FMC16_CRC_H */
