/**
 * @file fmc16_integration.h
 * @brief FMC16 系统集成: FreeRTOS 服务任务、初始化序列、对外查询接口。
 */
#ifndef FMC16_INTEGRATION_H
#define FMC16_INTEGRATION_H

#include "fmc16.h"
#include "fmc16_link.h"
#include "fmc16_status.h"
#include "fmc16_command.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 全局系统上下文 ---- */
typedef struct {
    Fmc16SystemState sys_state;
    Fmc16LinkCtx     link;
    Fmc16RxCtx       rx;
    Fmc16PendingCmd  cmds[FMC16_MAX_PENDING_CMDS];
    uint16_t         tx_buf[FMC16_MAX_PACKET_WORDS];

    /* 最新快照缓存 */
    Fmc16StatusSnapshot status_snap;
    Fmc16ModeSnapshot   mode_snap;
    Fmc16IOSnapshot     io_snap;
    uint8_t status_valid : 1;
    uint8_t mode_valid   : 1;
    uint8_t io_valid     : 1;

    /* 诊断 */
    uint32_t packets_rx;
    uint32_t packets_tx;
    uint32_t err_crc;
    uint32_t err_timeout;
} Fmc16System;

/* ---- 全局实例（在 fmc16_integration.c 中定义） ---- */
extern Fmc16System g_fmc16;

/* ---- API ---- */

/**
 * @brief FMC16 系统初始化。
 *        调用 fmc16_bus_init, 运行 CRC 自检, 初始化链路 + RX 状态机。
 *        此函数阻塞直到 CRC 自检通过或失败。
 * @return 1=成功, 0=CRC 自检失败
 */
int fmc16_system_init(void);

/**
 * @brief FMC16 FreeRTOS 服务任务。
 *        轮询 FMC 总线 → 喂入 RX 状态机 → 分发包到链路层/命令层。
 *        同时处理命令超时重试、周期 HOST_STATUS 发送。
 *
 *        建议优先级: 1 (低于 emWin)
 *        建议栈: 2048 words (8192 bytes)
 */
void FMC16_Service_Task(void *arg);

/**
 * @brief 非阻塞轮询（可在其他任务中调用）。
 *        处理最多 1 个就绪包。
 * @return 1=处理了一个包, 0=无数据
 */
int fmc16_poll_once(void);

/**
 * @brief 获取最新缓存的快照。
 */
const Fmc16StatusSnapshot *fmc16_get_status_snapshot(void);
const Fmc16ModeSnapshot   *fmc16_get_mode_snapshot(void);
const Fmc16IOSnapshot     *fmc16_get_io_snapshot(void);

#ifdef __cplusplus
}
#endif
#endif /* FMC16_INTEGRATION_H */
