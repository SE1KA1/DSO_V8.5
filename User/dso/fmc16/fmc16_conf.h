/**
 * @file fmc16_conf.h
 * @brief FMC16 协议栈可配置参数。
 *
 * 所有超时、队列大小、调试开关集中在此文件。
 * 修改无需进入驱动源码。
 */
#ifndef FMC16_CONF_H
#define FMC16_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 一、FMC 总线配置
 * ======================================================================== */

/** FMC Bank1 NE1 基地址（协议 §2） */
#define FMC16_BASE_ADDR             0x60000000U

/** FMC 访问宏：直接读写 16-bit */
#define FMC16_REG                   (*(__IO uint16_t *)(FMC16_BASE_ADDR))

/** FMC 总线互斥锁超时（ms） */
#define FMC16_BUS_LOCK_TIMEOUT_MS   100U

/** 总线读写之间的 turnaround 延迟（循环次数，实测校准） */
#define FMC16_BUS_TURNAROUND_CYCLES 10U

/* ========================================================================
 * 二、包协议参数
 * ======================================================================== */

/** 包间 word 最大间隔（ms），超时则丢弃半包并回到 SOF 搜索 */
#define FMC16_WORD_TIMEOUT_MS       10U

/** 默认命令超时（ms） */
#define FMC16_CMD_DEFAULT_TIMEOUT_MS 100U

/** 最大重试次数 */
#define FMC16_MAX_RETRIES           3U

/** 重试间隔（ms） */
#define FMC16_RETRY_INTERVAL_MS     10U

/* ========================================================================
 * 三、链路参数
 * ======================================================================== */

/** 等待 LINK_HELLO 超时（ms） */
#define FMC16_HELLO_TIMEOUT_MS      5000U

/** 握手总体超时（ms） */
#define FMC16_HANDSHAKE_TIMEOUT_MS  10000U

/** LINK_STATUS 查询周期（ms） */
#define FMC16_LINK_STATUS_PERIOD_MS 500U

/* ========================================================================
 * 四、缓冲区
 * ======================================================================== */

/** 接收缓冲区 word 数（应 ≥ FMC16_MAX_PACKET_WORDS） */
#define FMC16_RX_BUF_WORDS          300U

/** 发送缓冲区 word 数 */
#define FMC16_TX_BUF_WORDS          300U

/* ========================================================================
 * 五、调试开关
 * ======================================================================== */

/** 启用串口调试输出 */
#ifndef FMC16_DEBUG
#define FMC16_DEBUG                 1
#endif

/* ========================================================================
 * 五、遥测推送（dso_fmc_telemetry.c）
 * ======================================================================== */

/** 遥测服务器 IP（STM32 作为 TCP 客户端主动连接） */
#define FMC_TEL_SERVER_IP0  192
#define FMC_TEL_SERVER_IP1  168
#define FMC_TEL_SERVER_IP2  100
#define FMC_TEL_SERVER_IP3  100

/** 遥测服务器端口 */
#define FMC_TEL_SERVER_PORT 5005U

/** 周期快照推送间隔（ms） */
#define FMC_TEL_PERIOD_MS   500U

/** 调试标签 */
#define FMC16_TAG                   "[FMC16]"

#if FMC16_DEBUG
#include <stdio.h>
#define FMC16_LOG(fmt, ...)  printf(FMC16_TAG " " fmt "\r\n", ##__VA_ARGS__)
#define FMC16_WARN(fmt, ...) printf(FMC16_TAG " WARN: " fmt "\r\n", ##__VA_ARGS__)
#define FMC16_ERR(fmt, ...)  printf(FMC16_TAG " ERR: " fmt "\r\n", ##__VA_ARGS__)
#else
#define FMC16_LOG(...)  do { } while (0)
#define FMC16_WARN(...) do { } while (0)
#define FMC16_ERR(...)  do { } while (0)
#endif

#ifdef __cplusplus
}
#endif
#endif /* FMC16_CONF_H */
