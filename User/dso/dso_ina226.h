/**
 * @file dso_ina226.h
 * @brief 过桥板 INA226AIDGSR 电流监测（软件 I2C，PB7=SDA/PB8=SCL，与 PCAL6524/SHT40 同总线）。
 *
 * 每 1s 读一次分流电压，判过流：电流异常(>阈值)时 ①芯片 ALERT 引脚输出低
 * （SOL 告警 + 开漏低有效）；②DSO_Ina226_PowerOk() 返回 0 → get_switch 应答置
 * LED3(bit2) 电流报警（红）。
 */
#ifndef DSO_INA226_H
#define DSO_INA226_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** INA226 I2C 7-bit 地址 */
#define DSO_INA226_I2C_ADDR          0x41u

/** 检流电阻（mΩ） */
#define DSO_INA226_SHUNT_MOHM        100

/**
 * 过流告警阈值（分流电压 LSB，1 LSB = 2.5µV）。
 * Rshunt=100mΩ，正常 0.5-0.8mV(5-8mA)；600 LSB = 1.5mV ≈ 15mA。
 */
#define DSO_INA226_ALERT_LSB         600

/** 恢复迟滞下限 = 阈值×3/4（防临界抖动）。调阈值会自动跟随 */
#define DSO_INA226_RESTORE_LSB       (DSO_INA226_ALERT_LSB * 3 / 4)

/** 采样周期 */
#define DSO_INA226_POLL_MS           1000u

/** 连续失败超过该次数才当"无数据"处理（保留上次值，不影响 power_ok 判定） */
#define DSO_INA226_FAIL_MAX          50u

/** INA226 寄存器 */
#define DSO_INA226_REG_CONFIG        0x00u
#define DSO_INA226_REG_SHUNT         0x01u
#define DSO_INA226_REG_MASK_EN       0x06u
#define DSO_INA226_REG_ALERT_LIMIT   0x07u

/**
 * 创建 1s 采样任务（调度器启动前调用，与 DSO_Sht40_Init 同级）。
 * 任务须 prio 3：MainTask GUI_Init 卡死 spin 会饿死 prio-1 任务。
 */
void DSO_Ina226_Init(void);

/**
 * 电源正常（未过流且最近样本有效）。过流→0，用于 LED2(POWER) 熄灭。
 * 无数据/读失败时返回 1（避免误灭）。
 */
uint8_t DSO_Ina226_PowerOk(void);

/**
 * 最近电流（mA，整数，正=顺向）。从未成功时为 0。供诊断/将来上报。
 */
int32_t DSO_Ina226_GetCurMa(void);

#ifdef __cplusplus
}
#endif

#endif /* DSO_INA226_H */
