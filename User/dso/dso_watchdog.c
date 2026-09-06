/**
 * @file dso_watchdog.c
 * @brief SP706TEN 看门狗喂狗与监测（WDI=PB9 翻转；PFO#=PG10 输入读）。
 *
 * SP706 看门狗超时典型 1.6s：WDI 需在该时间内发生跳变，否则 RESET# 拉低。
 * 本任务每 500ms 翻转 PB9 电平提供跳变；PB9 原 LED3 在当前固件无调用，安全。
 * PFO#(电源故障/输出)接 PG10：本固件 FMC16 用 NE1=PD7，PG10 无占用，按输入读；
 * 若后续 PG10 复用作 FMC NE3/其它，将 DSO_WDOG_READ_PFO 置 0。
 * 远程恢复 = 后端可下发 reboot（command.execute，见协议文档，TBD 与后端对齐）。
 */
#include "dso_watchdog.h"
#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

#define WDOG_KICK_MS   500u

static volatile uint32_t s_kick_count = 0;

uint32_t DSO_Watchdog_GetKickCount(void)
{
    return s_kick_count;
}

uint8_t DSO_Watchdog_Pfo(void)
{
#if DSO_WDOG_READ_PFO
    return (uint8_t)((GPIOC->IDR >> 10) & 1u);
#else
    return 0xFFu;
#endif
}

uint8_t DSO_Watchdog_ResetCause(void)
{
    return (uint8_t)(RCC->CSR & 0xFFu);   /* IWDGRSTF/WWDGRSTF/LPWRRSTF/PINRSTF/BORRSTF/SFTRSTF/PORRSTF */
}

static void wdog_init_gpio(void)
{
    GPIO_InitTypeDef g;

#if DSO_WDOG_FEED_PB9
    __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Pin = GPIO_PIN_9;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
#endif
#if DSO_WDOG_READ_PFO
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = GPIO_PIN_10;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &g);
#endif
}

static void wdog_task(void *pv)
{
    (void)pv;
    printf("[WDOG] task start feed=%d pfo_read=%d\r\n",
           (int)DSO_WDOG_FEED_PB9, (int)DSO_WDOG_READ_PFO);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(WDOG_KICK_MS));
#if DSO_WDOG_FEED_PB9
        /* WDI 跳变喂狗（500ms < SP706 1.6s 超时） */
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_9);
        s_kick_count++;
#endif
    }
}

void DSO_Watchdog_Init(void)
{
    wdog_init_gpio();
    if (xTaskCreate(wdog_task, "Wdog", 256, NULL, 3, NULL) != pdPASS) {
        printf("[BOOT] FAIL Wdog\r\n");
    }
    printf("[WDOG] reset cause 0x%02X\r\n", (unsigned)DSO_Watchdog_ResetCause());
}
