/**
 * @file dso_sht40.h
 * @brief 过桥�? SHT40-AD1B-R2 温湿度传感器驱动（软�? I2C，PB7=SDA/PB8=SCL，与 PCAL6524 同总线）�?
 *
 * 1s 周期读一次温度，缓存最新值供协议应答；温�? > DSO_SHT40_ALARM_HI_CENTI �?
 * 通过 DSO_Sht40_HiAlarm() 置位 LED3(bit2) ALARM�?
 */
#ifndef DSO_SHT40_H
#define DSO_SHT40_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** SHT40 I2C 7-bit 地址 */
#define DSO_SHT40_I2C_ADDR          0x44u

/** 高精度测量命令：�? T+RH，t_meas typ 8.2ms（预�? 15ms�? */
#define DSO_SHT40_CMD_MEASURE_HIGH  0xFDu

/** 测量等待毫秒�?> SHT40 高精�? t_meas max�? */
#define DSO_SHT40_MEASURE_DELAY_MS  15u

/** 采样周期 */
#define DSO_SHT40_POLL_MS           1000u

/** 高温报警阈值（百分之一 ℃）�?6000 = 60.00 ℃。温度严格大于此值报警�? */
#define DSO_SHT40_ALARM_HI_CENTI    6000

/** 连续失败超过该次数标记无效（temp_ok=0），此后每次成功自动恢复 */
#define DSO_SHT40_FAIL_INVALID_N    10u

/**
 * 创建 1s 采样任务（须在调度器启动前调用，�? DSO_Switch_Cmd_Init 同级）�?
 */
void DSO_Sht40_Init(void);

/**
 * 最新温度（十分之一 ℃，�? 245 = 24.5 ℃）。传感器从未成功时为 0�?
 */
int32_t DSO_Sht40_GetTempDezi(void);

/**
 * 传感器有效（最近一次读取成功且未超连续失败阈值）�?
 */
uint8_t DSO_Sht40_IsValid(void);

/**
 * 高温报警：有效且温度 > DSO_SHT40_ALARM_HI_CENTI。返�? 1=报警�?
 */
uint8_t DSO_Sht40_HiAlarm(void);

#ifdef __cplusplus
}
#endif

#endif /* DSO_SHT40_H */
