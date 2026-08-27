/**
 * @file fmc16_bus.h
 * @brief FMC16 物理层 —— FMC Bank1 配置、GPIO、读写原语。
 *
 * 本模块直接操作 STM32H7 FMC 外设和 GPIO，不解析协议内容。
 *
 * 硬件背景：
 *   - 使用 FMC_NE1（PD7），BANK1，基地址 0x60000000
 *   - 16-bit 异步 NOR/SRAM 模式，无地址/数据复用，无 NWAIT
 *   - DB0~DB4 与 SPI4 (PE6/PE5/PE4/PE2) + X2_INT0 (PE3) 硬复用
 *   - 需与 OLED/HC574/AD7606 共享 NE1——通过互斥锁仲裁
 */
#ifndef FMC16_BUS_H
#define FMC16_BUS_H

#include "fmc16.h"
#include "fmc16_conf.h"
#include <stm32h7xx_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 一、初始化与去初始化
 * ======================================================================== */

/**
 * @brief 初始化 FMC Bank1 为 FMC16 模式。
 *
 * 执行步骤：
 *   1. 使能 FMC + GPIO 时钟
 *   2. 配置 PD7(FMC_NE1) + FMC_DB[15:0] + NOE/NWE 为 AF12_FMC
 *   3. 配置 FMC Bank1 SRAM 时序
 *   4. 初始化互斥锁
 *
 * 注意：
 *   - 调用前 XO2 必须已加载 FMC16_PERSONALITY 位流
 *   - 所有 NE1 共享设备应在调用前通过隔离措施静默
 */
void fmc16_bus_init(void);

/**
 * @brief 释放 FMC Bank1，回到安全默认态。
 *
 * 执行步骤：
 *   1. 将 FMC_DB[15:0] GPIO 切回输入高阻
 *   2. 置 NE1=HIGH（不选中任何设备）
 *   3. 可选的 FMC 时钟门控
 */
void fmc16_bus_deinit(void);

/* ========================================================================
 * 二、底层读写
 * ======================================================================== */

/**
 * @brief 写单个半字到 XO2。
 * @param data  要写入的 16-bit 值
 *
 * FMC 操作序列：NE1 LOW → 写 data 到 0x60000000 → NE1 HIGH
 * 调此函数前已持有总线锁。
 */
void fmc16_bus_write(uint16_t data);

/**
 * @brief 从 XO2 读单个半字。
 * @return 读到的 16-bit 值，总线空闲时 XO2 返回 0xFFFF
 *
 * FMC 操作序列：NE1 LOW → 从 0x60000000 读 → NE1 HIGH
 * 调此函数前已持有总线锁。
 */
uint16_t fmc16_bus_read(void);

/**
 * @brief 连续写多个半字到 XO2。
 *
 * 用于发送完整包（SOF0→CRC32），包内相邻 word 默认最⻓间隔 10 ms。
 * 建议使用 DMA 或至少在此临界区内禁止中断嵌套。
 *
 * @param buf  数据缓冲区
 * @param len  word 数
 */
void fmc16_bus_write_buffer(const uint16_t *buf, uint16_t len);

/**
 * @brief 连续从 XO2 读多个半字。
 *
 * 用于接收完整包。每次读操作的 FMC 周期由 XO2 内部 TX 队列控制
 * （有数据时返回 payload word，无数据时返回 0xFFFF）。
 *
 * @param buf  接收缓冲区
 * @param len  预期 word 数
 */
void fmc16_bus_read_buffer(uint16_t *buf, uint16_t len);

/**
 * @brief 读但不消费 TX 队列（peek）。
 *
 * FMC 执行读周期但不记录消费。用于包边界检测。
 * @return 当前 FMC_DB 值
 */
uint16_t fmc16_bus_peek(void);

/* ========================================================================
 * 三、总线仲裁
 * ======================================================================== */

/**
 * @brief 获取 NE1 总线互斥锁。
 *
 * 由于 XO2 不解码地址线，任何 NE1 读周期 XO2 都会驱动总线。
 * 在 FMC16 通信期间，HC574 等其他 NE1 设备必须暂停访问。
 *
 * @param timeout_ms 最大等待时间（ms），0 表示不等待
 * @return 1=获取成功，0=超时
 */
int fmc16_bus_try_lock(uint32_t timeout_ms);

/**
 * @brief 释放 NE1 总线互斥锁。
 */
void fmc16_bus_unlock(void);

/**
 * @brief 查询总线锁状态。
 * @return 1=已被锁定（FMC16 正在通信），0=空闲
 */
int fmc16_bus_is_locked(void);

/* ========================================================================
 * 四、设备就绪查询
 * ======================================================================== */

/**
 * @brief 查询 XO2 是否在 FMC 总线上就绪。
 *
 * 简单探测：读一个 word，如果是非 0xFFFF 的合法 SOF0/SOF1 开始则 XO2 在驱动总线。
 * 这只是启发式检查，正式确认需走完整 LINK_HELLO 握手。
 *
 * @return 1=探测到 XO2 总线活动，0=未探测到
 */
int fmc16_bus_probe(void);

/**
 * @brief 等待 XO2 TX 队列非空（有数据可读）。
 * @param timeout_ms  最大等待时间
 * @return 1=有数据，0=超时
 */
int fmc16_bus_wait_data_ready(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
#endif /* FMC16_BUS_H */
