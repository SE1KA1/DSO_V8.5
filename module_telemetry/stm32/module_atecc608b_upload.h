/**
 * @file module_atecc608b_upload.h
 * @brief 模块 1：ATECC608B 状态/事件上报（骨架，待驱动联调后接入真实数据）。
 *
 * 规格：module_telemetry/packet_specs/01_atecc608b.md
 */
#ifndef MODULE_ATECC608B_UPLOAD_H
#define MODULE_ATECC608B_UPLOAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 创建上报任务（调度器启动前调用一次）。驱动就绪后启用。 */
void MTEL_Atecc608b_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_ATECC608B_UPLOAD_H */
