/**
 * @file dso_watchdog.h
 * @brief SP706TEN 看门狗监测（WDI=PB9 喂狗；PFO#=PG10 读）。
 *
 * 职责：定期喂 WDI 防 STM32/XO2 宕机；上报喂狗计数、PFO# 电平与复位原因，
 * 供后端判定"是否曾宕机/被看门狗复位"；并提供远程复位入口
 * （DSO_Watchdog_RequestReboot：由喂狗任务延迟执行 NVIC_SystemReset）。
 * 数据面见 module_telemetry/packet_specs/08_watchdog.md。
 */
#ifndef DSO_WATCHDOG_H
#define DSO_WATCHDOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 1=用 PB9 定期翻转喂 WDI（PB9 原 LED3，当前固件无占用，安全） */
#ifndef DSO_WDOG_FEED_PB9
#define DSO_WDOG_FEED_PB9  1
#endif
/** 1=读 PG10(PFO#) 作为输入上拉（当前固件无占用；若被 FMC NE3/其它复用则置 0） */
#ifndef DSO_WDOG_READ_PFO
#define DSO_WDOG_READ_PFO  1
#endif

/** 创建喂狗任务（500ms 翻转 WDI）并初始化 PFO 读。 */
void DSO_Watchdog_Init(void);

/**
 * 请求远程复位（软复位，效果等同于按复位键）：置标志后立即返回，
 * 由喂狗任务延迟 ~300ms（让命令 ack 先经 TCP 发出）执行 NVIC_SystemReset()。
 * 复位原因位为 SFTRSTF，可在下次启动的 [WDOG] reset cause 行自证。
 */
void DSO_Watchdog_RequestReboot(void);

/** 喂狗翻转次数（自启动累计）。 */
uint32_t DSO_Watchdog_GetKickCount(void);

/** PFO# 电平：0/1；DSO_WDOG_READ_PFO=0 时返回 0xFF（未知）。 */
uint8_t DSO_Watchdog_Pfo(void);

/** 最近复位原因：RCC->CSR 低字节原始位（表见 08 规格：IWDG/WWDG/PIN/BOR/SFT/POR/LPWR）。 */
uint8_t DSO_Watchdog_ResetCause(void);

#ifdef __cplusplus
}
#endif

#endif /* DSO_WATCHDOG_H */
