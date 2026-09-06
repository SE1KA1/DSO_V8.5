/**
 * @file module_sht40_upload.h
 * @brief 模块 3：SHT40 状态周期上报（参考实现骨架）。
 * 规格：packet_specs/03_sht40.md；数据源：User/dso/dso_sht40.c（已实现）。
 */
#ifndef MODULE_SHT40_UPLOAD_H
#define MODULE_SHT40_UPLOAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MTEL_Sht40_Init(void);   /* 创建 1s 上报任务（MODULE_TEL_SHT40_ENABLE=1 时） */

#ifdef __cplusplus
}
#endif

#endif /* MODULE_SHT40_UPLOAD_H */
