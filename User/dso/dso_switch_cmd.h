/**
 * @file dso_switch_cmd.h
 * @brief 开关面板 JSON 控制命令处理（对齐 dsosim protocol v0.9）。
 */
#ifndef DSO_SWITCH_CMD_H
#define DSO_SWITCH_CMD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化开关面板命令模块：调用 DSO_Switch_Init()。
 * 心跳 LED 不设任务（堆紧张），在 get_switch 应答时按 tick 实时计算。
 * 在创建任何其它任务前调用一次（main 调度器启动前）。
 */
void DSO_Switch_Cmd_Init(void);

/**
 * 解析一行 JSON 控制命令并执行。
 * @param line     收到的完整行（不含 \r\n）
 * @param reply    输出缓冲区，填 JSON ack（不含 CRLF）
 * @param reply_sz reply 缓冲区大小
 * @return 1=本模块已处理（reply 有效）；0=非本模块命令
 */
int DSO_Switch_Cmd_HandleLine(const char *line, char *reply, size_t reply_sz);

#ifdef __cplusplus
}
#endif

#endif /* DSO_SWITCH_CMD_H */
