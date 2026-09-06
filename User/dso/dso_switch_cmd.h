/**
 * @file dso_switch_cmd.h
 * @brief 开关面板 JSON 控制命令处理（对齐 dsosim protocol v0.9）。
 */
#ifndef DSO_SWITCH_CMD_H
#define DSO_SWITCH_CMD_H

#include <stddef.h>
#include <stdint.h>

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

/** 最小 JSON 子集：按 key 读取字符串/整数值（容忍空白）。供遥测/命令模块共用。 */
int DSO_Switch_Cmd_JsonGetStr(const char *line, const char *key, char *buf, size_t buf_sz);
int DSO_Switch_Cmd_JsonGetInt(const char *line, const char *key, int *out);

/**
 * USB-UART 通道 5s 内是否有活动（YDS5/USB 状态灯数据源）。
 * 串口收到完整行即刷新。返回 1=有活动。
 */
uint8_t DSO_Switch_UsbActive(void);

/** 蜂鸣器静音（GUI set_buzzer 置位，节奏引擎读取；1=完全静音，LED 照常）。 */
uint8_t DSO_Switch_BuzzerMute(void);

#ifdef __cplusplus
}
#endif

#endif /* DSO_SWITCH_CMD_H */
