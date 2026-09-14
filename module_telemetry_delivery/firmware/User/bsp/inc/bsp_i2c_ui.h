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

/** 诊断：设置总线位延迟 µs（0=恢复默认 2µs）。慢速用于排除 ACK 采样过早误判。 */
void BSP_I2C_UI_SetBitDelayUs(uint32_t us);

/**
 * ATECC608B 等睡眠唤醒脉冲：SDA 拉低 ≥100µs 后释放（SCL 不动），等 ~2ms。
 * 睡眠中的器件对 I2C 地址 NACK，唤醒后才应答。共享总线上对其他从机无地址字节，安全。
 */
void BSP_I2C_UI_WakePulse(void);

/** 参数化唤醒：SDA 拉低 low_us µs（钳位 60..20000）后释放并等 2ms。 */
void BSP_I2C_UI_WakePulseUs(uint32_t low_us);

/** 诊断：持续拉低 SDA 约 ms 毫秒（阻塞），供万用表实测芯片 SDA 脚电平。 */
void BSP_I2C_UI_SdaLowMs(uint32_t ms);

/** 自检：驱动 SDA 低并回读。@return 1=引脚确实被拉低；0=读仍高（OD 配置/接线问题）。 */
int BSP_I2C_UI_SdaLowProbe(void);

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

/**
 * 原始写：START + addr+W + len 字节（逐字节收 ACK）+ STOP。
 * 用于无寄存器地址的器件（如 SHT40 命令字节）。
 * @return 0 成功；-1 无应答 / 参数错误。
 */
int BSP_I2C_UI_WriteRaw(uint8_t addr7, const uint8_t *buf, uint8_t len);

/**
 * 逐字节 ACK 记录写（ATECC 诊断用）：START + addr+W + len 字节，
 * 每个字节（含地址）的 ACK 结果记录到 ack_log[i]（1=ACK 0=NACK），
 * 中途 NACK 也继续发完剩余字节（诊断需要），最后 STOP。
 * @param ack_log 长度须 ≥ len+1：ack_log[0]=地址、ack_log[1+i]=buf[i]
 * @return 0=地址 ACK；-1=地址 NACK/参数错误
 */
int BSP_I2C_UI_DiagTxBytes(uint8_t addr7, const uint8_t *buf, uint8_t len,
                           uint8_t *ack_log);

/**
 * 原始读：START + addr+R + len 字节（前 len-1 字节主 ACK、最后 NACK）+ STOP。
 * @return 0 成功；-1 无应答 / 参数错误。
 */
int BSP_I2C_UI_ReadRaw(uint8_t addr7, uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* BSP_I2C_UI_H */
