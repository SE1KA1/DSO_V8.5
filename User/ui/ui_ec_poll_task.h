/**
 * @file ui_ec_poll_task.h
 * @brief 编码器/按键独立轮询任务，与 MainTask 渲染环解耦。
 *
 * MCP23017 无 MCU EXTI 时必须软件高频采样 A/B 相。
 * 由 MainTask 在 GUI_Init 后调用 UI_EcPollTask_Start() 创建一次。
 */
#ifndef UI_EC_POLL_TASK_H
/* 宏定义: UI_EC_POLL_TASK_H - 头文件保护宏。 */
#define UI_EC_POLL_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 在 MainTask 内于 GUI_Init（及需要时的 UI_Nav 初值）之后调用一次，创建 ecpoll 任务。
 * 可重复调用，仅首启生效。
 */
void UI_EcPollTask_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_EC_POLL_TASK_H */
