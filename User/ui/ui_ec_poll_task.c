/**
 * @file ui_ec_poll_task.c
 * @brief 编码器/按键轮询 FreeRTOS 任务实现。
 */
#include "bsp.h"
#include "ui_app_config.h"
#include "ui_ec_poll_task.h"
#include "ui_nav.h"
#include "FreeRTOS.h"
#include "task.h"

/**
 * 编码器/按键轮询任务：与 MainTask 渲染环解耦，避免 GUI 绘制阻塞输入。
 * 周期 UI_EC_POLL_MS（默认 8 ms），优先级 UI_EC_POLL_TASK_PRIORITY（默认 3，高于 emWin）。
 * 由 MainTask 首次进入时调用 UI_EcPollTask_Start() 创建，全局仅启动一次。
 */
static void vEcPollTask(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        UI_Nav_Poll();
        vTaskDelay(pdMS_TO_TICKS(UI_EC_POLL_MS));
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_EcPollTask_Start
*   功能说明: 轮询编码器/按键输入并转换为 UI 导航或参数调节事件。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_EcPollTask_Start(void)
{
    static uint8_t s_started;

    if (s_started != 0u) {
        return;
    }
    s_started = 1u;
    if (xTaskCreate((TaskFunction_t)vEcPollTask, "ecpoll",
                    (configSTACK_DEPTH_TYPE)UI_EC_POLL_TASK_STACK_WORDS, NULL,
                    (UBaseType_t)UI_EC_POLL_TASK_PRIORITY, NULL) != pdPASS) {
        Error_Handler(__FILE__, __LINE__);
    }
}
