/**
 * @file bsp_pcal6524.h
 * @brief PCAL6524HEAZ 24-bit I2C IO 扩展器驱动（开关面板，PB7/PB8 软件 I2C）。
 */
#ifndef BSP_PCAL6524_H
#define BSP_PCAL6524_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** PCAL6524 7-bit I2C 地址（云桥模块实测 0x22；I2C 扫描确认，非默认 0x20）。 */
#define PCAL6524_I2C_ADDR7  0x22u

/**
 * 初始化：先写 P0/P1 输出寄存器为全 1（开关全 OFF），再设为输出方向，防上电毛刺。
 * @return 0 成功；非 0 器件无应答（地址/总线问题）。
 */
int BSP_PCAL6524_Init(void);

/**
 * 写 P0+P1 共 16 位输出。bits[0..7]=P0，bits[8..15]=P1。
 * 开关 ON=低电平：调用方负责取反。
 * @return 0 成功；非 0 写失败。
 */
int BSP_PCAL6524_WriteP0P1(uint16_t bits);

/**
 * 写 P2_0..P2_4（YDS1..YDS5 硬件 LED，低电平点亮）。
 * @param on_mask 5 位点亮掩码（bit_i=1 → YDS(i+1) 亮）；内部取反后写 OUT2。
 */
int BSP_PCAL6524_WriteP2Leds(uint8_t on_mask);

#ifdef __cplusplus
}
#endif

#endif /* BSP_PCAL6524_H */
