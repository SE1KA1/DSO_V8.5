/**
 * @file dso_ina226_u44.h
 * @brief 核心板 U44-INA226AIDGSR 电流监测（软件 I2C 0x40，20mΩ 检流电阻）。
 *
 * 与过桥板 U10(0x41/100mΩ, dso_ina226.c) 同型号第二实例：同一 PB7/PB8 总线，
 * 独立 1s 采样任务 + 过流判警（纯分流阈值分级，迟滞×3/4；AFF 只作触发参考）。
 * 数据面见 module_telemetry/packet_specs/05_ina226_u44.md。
 */
#ifndef DSO_INA226_U44_H
#define DSO_INA226_U44_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DSO_INA226U44_I2C_ADDR      0x40u
#define DSO_INA226U44_SHUNT_MOHM    20u
/**
 * 过流告警分级阈值（分流 LSB，1 LSB=2.5µV）。
 * 声光联动（20mΩ）：实测正常态 103mA 稳定（≈824 LSB），
 * 额定取 135mA（1080 LSB）——正常态 103mA 落在额定 85% 预警线(115mA)之下：
 *   预警 85% ≈ 920 LSB(115mA)；告警 100% = 1080 LSB(135mA)；
 *   紧急 150% = 1620 LSB(202.5mA)。
 * ALERT 引脚芯片限值同告警线 1080。
 */
#define DSO_INA226U44_WARN_LSB      920
#define DSO_INA226U44_ALERT_LSB     1080
#define DSO_INA226U44_EMERG_LSB     1620
/** 恢复迟滞下限 = 告警线×4/5（须高于正常稳态 824：1080×4/5=864 LSB=108mA） */
#define DSO_INA226U44_RESTORE_LSB   (DSO_INA226U44_ALERT_LSB * 4 / 5)
#define DSO_INA226U44_POLL_MS       1000u

void DSO_Ina226U44_Init(void);          /* 创建 1s 采样任务（prio 3） */

/** 电源/电流正常（未过流）。过流→0。 */
uint8_t DSO_Ina226U44_PowerOk(void);

/** 最近电流（mA，lsb×2.5µV/20mΩ = lsb/8；可负=反向）。 */
int32_t DSO_Ina226U44_GetCurMa(void);

/**
 * 电流告警等级（声光联动四级）：
 *   0=正常(≤115mA) 1=预警(>115) 2=告警(>135mA) 3=紧急(>202.5mA)
 * 读失败时保留上次判定。
 */
uint8_t DSO_Ina226U44_GetLevel(void);

#ifdef __cplusplus
}
#endif

#endif /* DSO_INA226_U44_H */
