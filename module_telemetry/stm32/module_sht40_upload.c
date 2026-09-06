/**
 * @file module_sht40_upload.c
 * @brief 模块 3：SHT40 周期上报任务（参考骨架，数据源=真实 dso_sht40）。
 *
 * 数据字段：temp_dezi(十分之一℃)/temp_ok/hi_alarm —— 直接读 dso_sht40 缓存。
 * 启用：MODULE_TEL_SHT40_ENABLE=1 并拷入 DSO 工程（User/dso/ 同级含
 * dso_sht40.h 与 module_telemetry.h 的路径），再经 Keil Project Items 加入。
 */
#include "module_sht40_upload.h"
#include "module_telemetry.h"
#include <stdio.h>

#ifndef MODULE_TEL_SHT40_ENABLE
#define MODULE_TEL_SHT40_ENABLE 0
#endif

#if MODULE_TEL_SHT40_ENABLE
#include "dso/dso_sht40.h"
#include "FreeRTOS.h"
#include "task.h"
#define SHT40_UPLOAD_MS 1000u
#endif

#if MODULE_TEL_SHT40_ENABLE
static void sht40_upload_task(void *pv)
{
    uint32_t seq = 0;
    (void)pv;
    for (;;) {
        char js[160];
        snprintf(js, sizeof(js),
                 "{\"temp_dezi\":%ld,\"temp_ok\":%u,\"hi_alarm\":%u}",
                 (long)DSO_Sht40_GetTempDezi(),
                 (unsigned)DSO_Sht40_IsValid(),
                 (unsigned)DSO_Sht40_HiAlarm());
        (void)mtel_send(MTEL_TYPE_MODULE_DATA, "sht40", js);
        seq++;
        vTaskDelay(pdMS_TO_TICKS(SHT40_UPLOAD_MS));
    }
}
#endif

void MTEL_Sht40_Init(void)
{
#if MODULE_TEL_SHT40_ENABLE
    if (xTaskCreate(sht40_upload_task, "MTEL_Sht40", 512, NULL, 3, NULL)
            != pdPASS) {
        printf("[MTEL] FAIL sht40 task\r\n");
    }
#else
    printf("[MTEL] sht40 upload stub (MODULE_TEL_SHT40_ENABLE=0)\r\n");
#endif
}
