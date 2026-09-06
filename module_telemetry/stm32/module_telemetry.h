/**
 * @file module_telemetry.h
 * @brief 模块数据上云公共发送适配层（骨架）。
 *
 * 目标：把各功能模块的 module.data / module.event 帧经以太网 TCP 发给服务器。
 * 传输与信封对齐 module_telemetry/packet_specs/00_envelope_framework.md：
 *   TCP 192.168.100.100:5005（默认，可配），CRLF 行帧，一帧一行 JSON。
 *
 * 当前状态：骨架。挂接点 = 既有 FMC 遥测任务 dso_fmc_telemetry.c 的发送路径
 * （tel_send_frame / 同一条连接），接入 Keil 工程后再实现 mtel_send 本体。
 * 新文件本轮不参与 Keil 构建，README 注明待接入。
 */
#ifndef MODULE_TELEMETRY_H
#define MODULE_TELEMETRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 公共帧类型（00 规格 §2） */
#define MTEL_TYPE_MODULE_DATA   "module.data"
#define MTEL_TYPE_MODULE_EVENT  "module.event"

/**
 * 发送一帧模块数据（骨架：TODO 接入 dso_fmc_telemetry 的 TCP 连接发送）。
 * @param type   帧类型（module.data / module.event）
 * @param module 模块标识（≤24 小写，如 "atecc608b"）
 * @param data_json 已序列化的 data 对象 JSON 文本（不含换行）
 * @return 0 成功入队；-1 失败（未连接/超长）。TODO: 由实际发送路径决定
 */
int mtel_send(const char *type, const char *module, const char *data_json);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_TELEMETRY_H */
