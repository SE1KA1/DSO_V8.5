/**
 * @file dso_wavegen_types.h
 * @brief 波形发生器类型：方波/正弦等 profile 枚举。
 */

#ifndef DSO_WAVEGEN_TYPES_H
/* 宏定义: DSO_WAVEGEN_TYPES_H - 头文件保护宏。 */
#define DSO_WAVEGEN_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* 宏定义: DSO_WAVEGEN_MAX_LUT_SAMPLES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_MAX_LUT_SAMPLES 64u

typedef enum {
    DSO_WAVEGEN_WAVE_KIND_SINE = 0,
    DSO_WAVEGEN_WAVE_KIND_TRIANGLE,
    DSO_WAVEGEN_WAVE_KIND_SQUARE,
} DSO_WavegenWaveKind;

typedef enum {
    DSO_WAVEGEN_OK = 0,
    DSO_WAVEGEN_INVALID,
    DSO_WAVEGEN_TIMER_ERROR,
    DSO_WAVEGEN_HAL_ERROR,
    DSO_WAVEGEN_DMA_UNDERRUN,
} DSO_WavegenStatus;

typedef struct {
    DSO_WavegenWaveKind wave;
    const char *name;
    uint32_t wave_hz;
    uint16_t lut_count;
    uint16_t duty_permille;
} DSO_WavegenProfile;

typedef struct {
    bool running;
    bool dma_underrun;
    uint32_t requested_update_hz;
    uint32_t actual_update_hz;
    uint32_t tim_psc;
    uint32_t tim_arr;
    uint16_t lut_count;
} DSO_WavegenSnapshot;

#endif /* DSO_WAVEGEN_TYPES_H */
