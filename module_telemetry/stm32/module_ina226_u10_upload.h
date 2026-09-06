/**
 * @file module_ina226_u10_upload.h
 * @brief 模块 4：U10-INA226 电流周期上报（参考骨架）。
 * 规格：packet_specs/04_ina226_u10.md；数据源：User/dso/dso_ina226.c（已实现）。
 */
#ifndef MODULE_INA226_U10_UPLOAD_H
#define MODULE_INA226_U10_UPLOAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MTEL_Ina226U10_Init(void);   /* 创建 1s 上报任务（MODULE_TEL_INA226U10_ENABLE=1） */

#ifdef __cplusplus
}
#endif

#endif /* MODULE_INA226_U10_UPLOAD_H */
