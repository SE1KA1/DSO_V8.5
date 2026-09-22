/**
 * @file dso_ina226_u44.h
 * @brief 核心板 U44-INA226AIDGSR 电流监测（软件 I2C 0x40，20mΩ 检流电阻）。
 *
 * 与过桥板 U10(0x41/100mΩ, dso_ina226.c) 同型号第二实例：同一 PB7/PB8 总线，
 * 独立 1s 采样任务 + 过流判警（纯分流阈值分级，迟滞×4/5；AFF 只作触发参考）。
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
 * 声光联动（20mΩ）：实测核心板正常工作 118mA（≈944 LSB），
 * 额定取 160mA（1280 LSB）——正常态 118mA 落在额定 85% 预警线(136mA)之下，余量 15%：
 *   预警 85% = 1088 LSB(136mA)；告警 100% = 1280 LSB(160mA)；
 *   紧急 150% = 1920 LSB(240mA)。
 * ALERT 引脚芯片限值同告警线 1280。
 */
#define DSO_INA226U44_WARN_LSB      1088
#define DSO_INA226U44_ALERT_LSB     1280
#define DSO_INA226U44_EMERG_LSB     1920
/** 恢复迟滞下限 = 告警线×4/5（须高于正常稳态 944：1280×4/5=1024 LSB=128mA） */
#define DSO_INA226U44_RESTORE_LSB   (DSO_INA226U44_ALERT_LSB * 4 / 5)
#define DSO_INA226U44_POLL_MS       1000u

void DSO_Ina226U44_Init(void);          /* 创建 1s 采样任务（prio 3） */

/** 电源/电流正常（未过流）。过流→0。 */
uint8_t DSO_Ina226U44_PowerOk(void);

/** 最近电流（mA，lsb×2.5µV/20mΩ = lsb/8；可负=反向）。 */
int32_t DSO_Ina226U44_GetCurMa(void);

/**
 * 电流告警等级（声光联动四级）：
 *   0=正常(≤136mA) 1=预警(>136) 2=告警(>160mA) 3=紧急(>240mA)
 * 读失败时保留上次判定。
 */
uint8_t DSO_Ina226U44_GetLevel(void);

#ifdef __cplusplus
}
#endif

#endif /* DSO_INA226_U44_H */
