/**
 * @file dso_roll_physics.c
 * @brief roll physics 实现。
 */

#include "dso_roll_physics.h"

#include <math.h>

/*
*********************************************************************************************************
*   函 数 名: DSO_RollPhysics_FullWindowSeconds
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_RollPhysics_FullWindowSeconds(uint32_t sec_div_ns)
{
    return (float)sec_div_ns * (float)DSO_ROLL_PHYS_GRID_COL_DIV * 1e-9f;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_RollPhysics_TimePerPixelNs
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_RollPhysics_TimePerPixelNs(uint32_t sec_div_ns, uint32_t width)
{
    if ((sec_div_ns == 0u) || (width == 0u)) {
        return 0.0f;
    }
    return ((float)sec_div_ns * (float)DSO_ROLL_PHYS_GRID_COL_DIV) / (float)width;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_RollPhysics_SamplesPerPixel
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_RollPhysics_SamplesPerPixel(uint32_t sec_div_ns,
                                      uint32_t sample_rate_hz,
                                      uint32_t width)
{
    float tpp_s;

    if ((sec_div_ns == 0u) || (sample_rate_hz == 0u) || (width == 0u)) {
        return 1.0f;
    }

    tpp_s = DSO_RollPhysics_TimePerPixelNs(sec_div_ns, width) * 1e-9f;
    {
        float spp = (float)sample_rate_hz * tpp_s;
        if (spp < 1.0f) {
            spp = 1.0f;
        }
        return spp;
    }
}
