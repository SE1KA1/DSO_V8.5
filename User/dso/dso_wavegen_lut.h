/**
 * @file dso_wavegen_lut.h
 * @brief 波形 DAC 查找表。
 */

#ifndef DSO_WAVEGEN_LUT_H
/* 宏定义: DSO_WAVEGEN_LUT_H - 头文件保护宏。 */
#define DSO_WAVEGEN_LUT_H

#include <stdint.h>

#include "dso_wavegen_types.h"

typedef struct {
    DSO_WavegenWaveKind wave;
    uint16_t lut_count;
    uint16_t offset;
    uint16_t amplitude;
    uint16_t duty_permille;
} DSO_WavegenLutConfig;

/*
*********************************************************************************************************
*   函 数 名: DSO_WavegenLut_Build
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_WavegenStatus DSO_WavegenLut_Build(const DSO_WavegenLutConfig *cfg,
                                       uint16_t *out_lut,
                                       uint16_t out_capacity,
                                       uint16_t *out_count);

#endif /* DSO_WAVEGEN_LUT_H */
