/**
 * @file main.c
 * @brief 精简五模块上报工程入口（module_telemetry_delivery/firmware）。
 *
 * 只做四件事：
 *   1. 系统时钟/MPU/Cache/UART（bsp.c，与主固件同源）
 *   2. 拉起软件 I2C 总线上的 4 类传感器驱动（PCAL6524/SHT40/INA226×2）采样任务
 *   3. 启动五模块上报任务（dso_module_upload.c：每秒 5 帧 TCP 上行）
 *   4. 喂 SP706T 看门狗（PB9，不喂会整板复位）+ 延时后 MX_LWIP_Init
 *
 * 不含 LCD/GUI/FPGA/示波器采集/开关命令串口等 DSO 全栈功能。
 */
#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lwip.h"

#include "dso/dso_switch_panel.h"
#include "dso/dso_sht40.h"
#include "dso/dso_ina226.h"
#include "dso/dso_ina226_u44.h"
#include "dso/dso_module_upload.h"
#include "dso/dso_watchdog.h"

/* LwIP 拉起任务：仿主固件 dso_eth_upload 的 3s PHY 自协商等待 */
static void vNetifTask(void *pv)
{
    (void)pv;
    printf("[NET] MX_LWIP_Init after 3s PHY settle\r\n");
    vTaskDelay(pdMS_TO_TICKS(3000u));
    MX_LWIP_Init();
    printf("[NET] MX_LWIP_Init done\r\n");
    vTaskDelete(NULL);
}

/* FreeRTOS 钩子（configCHECK_FOR_STACK_OVERFLOW=2 / configUSE_MALLOC_FAILED_HOOK=1） */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("\r\n[RTOS] STACK OVERFLOW: %s\r\n",
           (pcTaskName != NULL) ? pcTaskName : "?");
    for (;;) {}
}

void vApplicationMallocFailedHook(void)
{
    printf("\r\n[RTOS] pvPortMalloc failed (heap exhausted)\r\n");
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

int main(void)
{
    System_Init();               /* MPU + Cache + HAL_Init + 400MHz 时钟 */

    HAL_SuspendTick();
    __set_PRIMASK(1);

    bsp_Init();                  /* DWT + UART1(printf, 1152000) + LED */
    printf("\r\n[BOOT] minimal 5ch telemetry fw "
           "(module_telemetry_delivery/firmware)\r\n");

    /* 传感器/上报/喂狗任务全部在调度器启动前创建 */
    DSO_Switch_Init();           /* PH7/PH8 + PCAL6524 + PB7/PB8 I2C 总线拉起 */
    DSO_Sht40_Init();            /* SHT40(0x44) 1s 采样任务 prio3 */
    DSO_Ina226_Init();           /* INA226 U10(0x41) 1s 采样任务 prio3 */
    DSO_Ina226U44_Init();        /* INA226 U44(0x40) 1s 采样任务 prio3 */
    DSO_ModuleUpload_Init();     /* MUpTcp 逐秒 5 帧 + MUpYds 点灯 + MUpBuz 蜂鸣 */
    DSO_Watchdog_Init();         /* SP706T 喂狗：Wdog 任务 500ms toggle PB9 */
    if (xTaskCreate(vNetifTask, "EthInit", 512, NULL, 3, NULL) != pdPASS) {
        printf("[BOOT] FAIL EthInit\r\n");
    }

    HAL_ResumeTick();
    __set_PRIMASK(0);
    __DSB();
    SCB_CleanInvalidateDCache();
    __DSB();

    vTaskStartScheduler();
    for (;;) {}
}
