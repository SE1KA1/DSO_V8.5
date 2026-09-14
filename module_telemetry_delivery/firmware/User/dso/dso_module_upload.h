/**
 * @file dso_module_upload.h
 * @brief module_telemetry 各功能模块数据经以太网上报（spec-00 信封）。
 *
 * 帧格式与字段定义见 module_telemetry/packet_specs/（00 信封 + 各模块）。
 * 复用 dso_fmc_telemetry 的 TCP 连接（DSO_FmcTelemetry_SendRaw）。
 */
#ifndef DSO_MODULE_UPLOAD_H
#define DSO_MODULE_UPLOAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 创建各模块上报任务（调度器启动前调用）。当前含：sht40、ina226_u10。 */
void DSO_ModuleUpload_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* DSO_MODULE_UPLOAD_H */
