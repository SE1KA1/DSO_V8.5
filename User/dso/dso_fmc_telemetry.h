/**
 * @file dso_fmc_telemetry.h
 * @brief FMC 主机遥测：读 XO2 快照 → JSON 帧 → TCP 主动推送服务器。
 *
 * 帧格式（JSON，CRLF 定界，对齐 dsosim v0.9）见
 * Markdown/STM32H743_FMC_HOST_TELEMETRY.md。
 */
#ifndef DSO_FMC_TELEMETRY_H
#define DSO_FMC_TELEMETRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化遥测模块并创建推送任务（ETH/LwIP 就绪后调用；任务内部等待 netif up）。
 */
void DSO_FmcTelemetry_Init(void);

/**
 * 事件回调：由 FMC16_Service_Task 在收到 XO2 EVENT 包时调用，即时推送事件帧。
 * @param code  事件码（FMC16_EVENT_MASK_* 对应位）
 * @param ts    XO2 事件时间戳（ms）
 * @param data  事件数据 words（可为 NULL）
 * @param n     数据 word 数（0..8）
 */
void DSO_FmcTelemetry_OnEvent(uint16_t code, uint32_t ts,
                              const uint16_t *data, uint16_t n);

#ifdef __cplusplus
}
#endif

#endif /* DSO_FMC_TELEMETRY_H */
