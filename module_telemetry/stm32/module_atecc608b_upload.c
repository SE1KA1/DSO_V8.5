/**
 * @file module_atecc608b_upload.c
 * @brief ATECC608B 周期状态/事件上报任务（骨架）。
 *
 * 数据面 schema 见 packet_specs/01_atecc608b.md：
 *   module.data：present/comm_ok/chip_info{serial4,part,rev,lock_config,lock_data}
 *                /login_state/fail_cnt
 *   module.event：tamper（I2C 通信失败超阈值/锁状态异常/校验失败）、login
 *
 * 构建开关：MODULE_TEL_ATECC_ENABLE=1 时才包含 FreeRTOS 与任务（骨架默认 0，
 * 便于在 DSO 树外独立阅读/静态检查）。接入时置 1 并：
 *  1) atecc608b_collect_status() 内接真实驱动（I2C 7-bit 0x60，PB7/PB8 总线，
 *     与 PCAL6524/SHT40/INA226 共用 BSP_I2C_UI 互斥锁；访问前须先按 ATECC608
 *     规范 Wake：≤100kHz 发 0x00 使 SDA 低 ≥60µs → 等 ≥1.5ms）
 *  2) mtel_send() 接入 dso_fmc_telemetry 的 TCP 发送路径（module_telemetry.h）
 *  3) 本文件加入 Keil 工程（Project→Manage→Project Items）
 */
#include "module_atecc608b_upload.h"
#include "module_telemetry.h"
#include <stdio.h>

#ifndef MODULE_TEL_ATECC_ENABLE
#define MODULE_TEL_ATECC_ENABLE 0
#endif

#define ATECC608B_I2C_ADDR  0x60u   /* 7 位地址（SSHDA-T 默认；总线字节 W=0xC0/R=0xC1，PB7/PB8 总线） */
#define ATECC608B_POLL_MS   1000u

#if MODULE_TEL_ATECC_ENABLE
#include "FreeRTOS.h"
#include "task.h"
#endif

/* ---- 数据源（骨架：驱动就绪后填充真实采集） ---- */

/** 采集当前状态并写入 data_json。@return 0=可上报；非 0=不可用（跳过本周期） */
static int atecc608b_collect_status(char *json, size_t sz)
{
    /* TODO：真实驱动就绪后填充——
     *   present   = (先 Wake，再 BSP_I2C_UI_Probe(0x60) == 0)
     *   comm_ok   = 最近读成功
     *   chip_info = 读 Config zone：序列号前 4B hex、part、rev、lock_config/lock_data
     *   login_state / fail_cnt 由上层登录状态机维护
     * 未接入时上报"不可用"状态，避免后端误判：
     */
    snprintf(json, sz,
             "{\"present\":0,\"comm_ok\":0,"
             "\"login_state\":\"LOGGED_OUT\",\"fail_cnt\":0}");
    return -1;   /* TODO：接入后返回 0 */
}

#if MODULE_TEL_ATECC_ENABLE
static void atecc608b_upload_task(void *pv)
{
    (void)pv;
    uint32_t seq = 0;
    char data_json[512];

    printf("[MTEL] atecc608b task start (addr=0x%02X)\r\n",
           (unsigned)ATECC608B_I2C_ADDR);
    for (;;) {
        if (atecc608b_collect_status(data_json, sizeof(data_json)) == 0) {
            /* TODO：ts_ms 用 HAL_GetTick()/系统单调 ms */
            (void)mtel_send(MTEL_TYPE_MODULE_DATA, "atecc608b", data_json);
            seq++;
        }
        vTaskDelay(pdMS_TO_TICKS(ATECC608B_POLL_MS));
    }
}
#endif

void MTEL_Atecc608b_Init(void)
{
#if MODULE_TEL_ATECC_ENABLE
    if (xTaskCreate(atecc608b_upload_task, "MTEL_Atecc", 512, NULL, 1, NULL)
            != pdPASS) {
        printf("[MTEL] FAIL atecc608b task\r\n");
    }
#else
    printf("[MTEL] atecc608b upload stub (MODULE_TEL_ATECC_ENABLE=0)\r\n");
#endif
}
