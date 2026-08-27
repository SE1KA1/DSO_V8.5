/**
 * @file bsp_i2c_ui.h
 * @brief 面板软件 I2C 总线（PB8=SCL, PB7=SDA，开漏 + 外部上拉）。
 *
 * 由 MCP23017(0x26) 与 PCAL6524(0x20) 共享。FreeRTOS 互斥锁串行化
 * 两个任务（EC 轮询每 8ms 读 MCP23017；开关命令任务写 PCAL6524）对总线的访问。
 */
#ifndef BSP_I2C_UI_H
#define BSP_I2C_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化 PB7/PB8 开漏 GPIO 与总线互斥锁。幂等，可多次调用。 */
void BSP_I2C_UI_Init(void);

/** 总线互斥锁：调度器未启动或锁未创建时为空操作（仿 DebugUartLock）。 */
void BSP_I2C_UI_Lock(void);
void BSP_I2C_UI_Unlock(void);

/**
 * 向 7-bit 地址器件写单寄存器。
 * @return 0 成功；-1 器件无应答 / 参数错误。
 */
int BSP_I2C_UI_WriteReg(uint8_t addr7, uint8_t reg, uint8_t val);

/**
 * 从 7-bit 地址器件读单寄存器（含 restart）。
 * @return 0 成功；-1 器件无应答 / 参数错误。
 */
int BSP_I2C_UI_ReadReg(uint8_t addr7, uint8_t reg, uint8_t *val);

/**
 * 地址探测：仅发送 addr+W 并检查 ACK，无寄存器访问、无副作用。
 * @return 0 器件应答；-1 无应答。
 */
int BSP_I2C_UI_Probe(uint8_t addr7);

#ifdef __cplusplus
}
#endif

#endif /* BSP_I2C_UI_H */
