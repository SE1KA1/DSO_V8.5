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
 * 过流告警分级阈值（分流电压 LSB，1 LSB = 2.5µV）。
 * 声光联动（100mΩ）：实测正常态 10~23mA 波动（峰 ≈920 LSB），
 * 额定取 35mA（1400 LSB）——正常态 ≤23mA 落在额定 85% 预警线(29.75mA)之下：
 *   预警 85% = 1190 LSB(29.75mA)；告警 100% = 1400 LSB(35mA)；
 *   紧急 150% = 2100 LSB(52.5mA)。
 * ALERT 引脚（蜂鸣器）芯片限值同告警线 1400；响窗口由节奏引擎临时压 200 LSB。
 */
#define DSO_INA226_WARN_LSB          1190
#define DSO_INA226_ALERT_LSB         1400
#define DSO_INA226_EMERG_LSB         2100

/** 恢复迟滞下限 = 告警线×4/5（须高于正常稳态上界：1400×4/5=1120 LSB=28mA > 920） */
#define DSO_INA226_RESTORE_LSB       (DSO_INA226_ALERT_LSB * 4 / 5)

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

/**
 * 电流告警等级（声光联动四级）：
 *   0=正常(≤29.75mA) 1=预警(>29.75) 2=告警(>35mA) 3=紧急(>52.5mA)
 * 读失败时保留上次判定。
 */
uint8_t DSO_Ina226_GetLevel(void);

/**
 * 蜂鸣器节奏引擎：写 ALERT LIMIT(0x07) 控制 U10 ALERT(蜂鸣器)通断。
 * @param lsb 0x7FFF=抑制(停)；小值(如 200)=强制触发(响)
 */
void DSO_Ina226_SetAlertLimit(uint16_t lsb);

/** 读 M/E(0x06) 清锁存，释放 ALERT（抑制后调用，立即停响）。 */
void DSO_Ina226_ClearAlertLatch(void);

#ifdef __cplusplus
}
#endif

#endif /* DSO_INA226_H */
