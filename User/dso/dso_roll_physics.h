/**
 * @file dso_roll_physics.h
 * @brief Roll 模式 samples-per-pixel 等物理量计算。
 */

#ifndef DSO_ROLL_PHYSICS_H
/* 宏定义: DSO_ROLL_PHYSICS_H - 头文件保护宏。 */
#define DSO_ROLL_PHYSICS_H

#include <stdint.h>

/* Must match DSO_GRID_COL_DIV in dso_timebase.c (Roll horizontal grid). */
#define DSO_ROLL_PHYS_GRID_COL_DIV 10u

/** Full-screen time span for one horizontal row: sec_div * grid_cols (seconds). */
float DSO_RollPhysics_FullWindowSeconds(uint32_t sec_div_ns);

/** Time per display column (nanoseconds). */
float DSO_RollPhysics_TimePerPixelNs(uint32_t sec_div_ns, uint32_t width);

/** Boxcar samples per pixel (>= 1). Same policy as RollRender / RollLineBuilder. */
float DSO_RollPhysics_SamplesPerPixel(uint32_t sec_div_ns,
                                      uint32_t sample_rate_hz,
                                      uint32_t width);

#endif /* DSO_ROLL_PHYSICS_H */
