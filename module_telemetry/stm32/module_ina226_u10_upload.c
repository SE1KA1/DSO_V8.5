/**
 * @file module_ina226_u10_upload.c
 * @brief 模块 4：U10-INA226 周期上报任务（参考骨架，数据源=真实 dso_ina226）。
 *
 * 数据字段：cur_ma/over_cur/valid —— dso_ina226 已缓存（over_cur=!PowerOk）。
 * 启用：MODULE_TEL_INA226U10_ENABLE=1 并拷入 DSO 工程后 Keil Project Items 加入。
 */
#include "module_ina226_u10_upload.h"
#include "module_telemetry.h"
#include <stdio.h>

#ifndef MODULE_TEL_INA226U10_ENABLE
#define MODULE_TEL_INA226U10_ENABLE 0
#endif

#if MODULE_TEL_INA226U10_ENABLE
#include "dso/dso_ina226.h"
#include "FreeRTOS.h"
#include "task.h"
#define INA226_U10_UPLOAD_MS 1000u
#endif

#if MODULE_TEL_INA226U10_ENABLE
static void ina226_u10_upload_task(void *pv)
{
    (void)pv;
    for (;;) {
        char js[128];
        snprintf(js, sizeof(js),
                 "{\"cur_ma\":%ld,\"over_cur\":%u,\"valid\":%u}",
                 (long)DSO_Ina226_GetCurMa(),
                 (unsigned)(DSO_Ina226_PowerOk() ? 0u : 1u),
                 (unsigned)1u);   /* TODO: 接入 dso_ina226 的有效标志后取真实值 */
        (void)mtel_send(MTEL_TYPE_MODULE_DATA, "ina226_u10", js);
        vTaskDelay(pdMS_TO_TICKS(INA226_U10_UPLOAD_MS));
    }
}
#endif

void MTEL_Ina226U10_Init(void)
{
#if MODULE_TEL_INA226U10_ENABLE
    if (xTaskCreate(ina226_u10_upload_task, "MTEL_Ina10", 512, NULL, 3, NULL)
            != pdPASS) {
        printf("[MTEL] FAIL ina226_u10 task\r\n");
    }
#else
    printf("[MTEL] ina226_u10 upload stub (MODULE_TEL_INA226U10_ENABLE=0)\r\n");
#endif
}
