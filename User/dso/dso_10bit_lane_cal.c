/**
 * @file dso_10bit_lane_cal.c
 * @brief 10-bit lane cal 求解与 Apply。
 */

#include "dso_10bit_lane_cal.h"

#include "bsp_i2c_eeprom_24xx.h"
#include "dso_sample_spec.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/* 宏定义: DSO_10BIT_LANE_CAL_TOUCH_PARAM_BYTES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_10BIT_LANE_CAL_TOUCH_PARAM_BYTES 64u
/* 宏定义: DSO_10BIT_LANE_CAL_RESERVED_BYTES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_10BIT_LANE_CAL_RESERVED_BYTES   128u
/* 宏定义: DSO_10BIT_LANE_CAL_TOUCH_PARAM_ADDR - 内存地址或外设地址映射常量。 */
#define DSO_10BIT_LANE_CAL_TOUCH_PARAM_ADDR (EE_SIZE - DSO_10BIT_LANE_CAL_TOUCH_PARAM_BYTES)
/* 宏定义: DSO_10BIT_LANE_CAL_EE_ADDR - 内存地址或外设地址映射常量。 */
#define DSO_10BIT_LANE_CAL_EE_ADDR          (DSO_10BIT_LANE_CAL_TOUCH_PARAM_ADDR - DSO_10BIT_LANE_CAL_RESERVED_BYTES)
/* 宏定义: DSO_10BIT_LANE_CAL_MAGIC - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_10BIT_LANE_CAL_MAGIC            0x434C544Au
/* 宏定义: DSO_10BIT_LANE_CAL_VERSION - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_10BIT_LANE_CAL_VERSION          1u
/* 宏定义: DSO_10BIT_LANE_CAL_TARGET_FRAMES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_10BIT_LANE_CAL_TARGET_FRAMES    16u
/* 宏定义: DSO_10BIT_LANE_CAL_MIN_FREQ_HZ - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_10BIT_LANE_CAL_MIN_FREQ_HZ      1000000.0f
/* 宏定义: DSO_10BIT_LANE_CAL_MAX_FREQ_HZ - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_10BIT_LANE_CAL_MAX_FREQ_HZ      5000000.0f
/* 宏定义: DSO_10BIT_LANE_CAL_MIN_COUNT - 计数、阈值、容量或深度配置常量。 */
#define DSO_10BIT_LANE_CAL_MIN_COUNT        1024u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved0;
    float offset_lsb;
    float gain;
    float skew;
    uint32_t checksum;
} DSO_10bitLaneCalStorage;

typedef struct {
    DSO_10bitLaneCalCoeffs coeffs;
    DSO_10bitLaneCalState state;
    DSO_10bitLaneCalError error;
    bool valid;
    uint8_t good_frames;
    float accum_offset;
    float accum_gain;
    float accum_skew;
    float last_freq_hz;
} DSO_10bitLaneCalRuntime;

static DSO_10bitLaneCalRuntime s_cal;

/*
*********************************************************************************************************
*   函 数 名: clampf_local
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float clampf_local(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

/*
*********************************************************************************************************
*   函 数 名: clamp_u16_from_f32
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t clamp_u16_from_f32(float v, uint16_t adc_max)
{
    if (v < 0.0f) {
        return 0u;
    }
    if (v > (float)adc_max) {
        return adc_max;
    }
    return (uint16_t)(v + 0.5f);
}

/*
*********************************************************************************************************
*   函 数 名: estimate_ideal_and_slope
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void estimate_ideal_and_slope(const uint16_t *samples,
                                     uint32_t count,
                                     uint32_t odd_index,
                                     float *out_ideal,
                                     float *out_slope)
{
    float ideal;
    float slope;

    if ((samples == NULL) || (out_slope == NULL) || (odd_index >= count)) {
        return;
    }

    if ((odd_index >= 3u) && ((odd_index + 3u) < count)) {
        float e_m3 = (float)samples[odd_index - 3u];
        float e_m1 = (float)samples[odd_index - 1u];
        float e_p1 = (float)samples[odd_index + 1u];
        float e_p3 = (float)samples[odd_index + 3u];
        ideal = (-e_m3 + (9.0f * e_m1) + (9.0f * e_p1) - e_p3) * (1.0f / 16.0f);
        slope = (e_m3 - (27.0f * e_m1) + (27.0f * e_p1) - e_p3) * (1.0f / 48.0f);
    } else if ((odd_index > 0u) && ((odd_index + 1u) < count)) {
        ideal = ((float)samples[odd_index - 1u] + (float)samples[odd_index + 1u]) * 0.5f;
        slope = ((float)samples[odd_index + 1u] - (float)samples[odd_index - 1u]) * 0.5f;
    } else {
        ideal = (float)samples[odd_index];
        slope = 0.0f;
    }

    if (out_ideal != NULL) {
        *out_ideal = ideal;
    }
    *out_slope = slope;
}

/*
*********************************************************************************************************
*   函 数 名: checksum32_bytes
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t checksum32_bytes(const uint8_t *data, size_t len)
{
    uint32_t hash = 2166136261u;
    size_t i;

    for (i = 0u; i < len; ++i) {
        hash ^= (uint32_t)data[i];
        hash *= 16777619u;
    }

    return hash;
}

/*
*********************************************************************************************************
*   函 数 名: solve3x3
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t solve3x3(float a[3][4], float out[3])
{
    uint32_t col;

    for (col = 0u; col < 3u; ++col) {
        uint32_t pivot = col;
        float pivot_abs = fabsf(a[pivot][col]);
        uint32_t row;

        for (row = col + 1u; row < 3u; ++row) {
            float v = fabsf(a[row][col]);
            if (v > pivot_abs) {
                pivot = row;
                pivot_abs = v;
            }
        }

        if (pivot_abs < 1e-6f) {
            return 0u;
        }

        if (pivot != col) {
            uint32_t j;
            for (j = col; j < 4u; ++j) {
                float tmp = a[col][j];
                a[col][j] = a[pivot][j];
                a[pivot][j] = tmp;
            }
        }

        {
            float div = a[col][col];
            uint32_t j;
            for (j = col; j < 4u; ++j) {
                a[col][j] /= div;
            }
        }

        for (row = 0u; row < 3u; ++row) {
            float factor;
            uint32_t j;

            if (row == col) {
                continue;
            }
            factor = a[row][col];
            for (j = col; j < 4u; ++j) {
                a[row][j] -= factor * a[col][j];
            }
        }
    }

    out[0] = a[0][3];
    out[1] = a[1][3];
    out[2] = a[2][3];
    return 1u;
}

/*
*********************************************************************************************************
*   函 数 名: storage_load
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool storage_load(DSO_10bitLaneCalCoeffs *out)
{
    DSO_10bitLaneCalStorage raw;
    uint32_t sum;

    if (out == NULL) {
        return false;
    }

    memset(&raw, 0, sizeof(raw));
    if (ee_ReadBytes((uint8_t *)&raw,
                     (uint16_t)DSO_10BIT_LANE_CAL_EE_ADDR,
                     (uint16_t)sizeof(raw)) == 0u) {
        return false;
    }

    if ((raw.magic != DSO_10BIT_LANE_CAL_MAGIC) ||
        (raw.version != DSO_10BIT_LANE_CAL_VERSION)) {
        return false;
    }

    sum = checksum32_bytes((const uint8_t *)&raw, offsetof(DSO_10bitLaneCalStorage, checksum));
    if (sum != raw.checksum) {
        return false;
    }

    out->offset_lsb = raw.offset_lsb;
    out->gain = raw.gain;
    out->skew = raw.skew;
    out->valid = true;
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: storage_save
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool storage_save(const DSO_10bitLaneCalCoeffs *coeffs)
{
    DSO_10bitLaneCalStorage raw;

    if ((coeffs == NULL) || !coeffs->valid) {
        return false;
    }

    memset(&raw, 0, sizeof(raw));
    raw.magic = DSO_10BIT_LANE_CAL_MAGIC;
    raw.version = DSO_10BIT_LANE_CAL_VERSION;
    raw.offset_lsb = coeffs->offset_lsb;
    raw.gain = coeffs->gain;
    raw.skew = coeffs->skew;
    raw.checksum = checksum32_bytes((const uint8_t *)&raw, offsetof(DSO_10bitLaneCalStorage, checksum));

    return ee_WriteBytes((uint8_t *)&raw,
                         (uint16_t)DSO_10BIT_LANE_CAL_EE_ADDR,
                         (uint16_t)sizeof(raw)) != 0u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_Init
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_Init(void)
{
    memset(&s_cal, 0, sizeof(s_cal));
    s_cal.state = DSO_10BIT_LANE_CAL_STATE_IDLE;
    if (storage_load(&s_cal.coeffs)) {
        s_cal.valid = true;
        s_cal.state = DSO_10BIT_LANE_CAL_STATE_LOADED;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_RequestRun
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_RequestRun(void)
{
    if (DSO_SampleSpec_GetActiveFormat() != DSO_SAMPLE_FORMAT_10BIT) {
        s_cal.state = DSO_10BIT_LANE_CAL_STATE_FAILED;
        s_cal.error = DSO_10BIT_LANE_CAL_ERR_NO_SIGNAL;
        s_cal.good_frames = 0u;
        return;
    }

    s_cal.state = DSO_10BIT_LANE_CAL_STATE_COLLECTING;
    s_cal.error = DSO_10BIT_LANE_CAL_ERR_NONE;
    s_cal.good_frames = 0u;
    s_cal.accum_offset = 0.0f;
    s_cal.accum_gain = 0.0f;
    s_cal.accum_skew = 0.0f;
    s_cal.last_freq_hz = 0.0f;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_Cancel
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_Cancel(void)
{
    s_cal.state = s_cal.valid ? DSO_10BIT_LANE_CAL_STATE_LOADED : DSO_10BIT_LANE_CAL_STATE_IDLE;
    s_cal.error = DSO_10BIT_LANE_CAL_ERR_NONE;
    s_cal.good_frames = 0u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_GetInfo
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_GetInfo(DSO_10bitLaneCalInfo *out)
{
    if (out == NULL) {
        return;
    }

    out->coeffs = s_cal.coeffs;
    out->state = s_cal.state;
    out->error = s_cal.error;
    out->valid = s_cal.valid;
    out->good_frames = s_cal.good_frames;
    out->target_frames = DSO_10BIT_LANE_CAL_TARGET_FRAMES;
    out->progress_pct = (uint8_t)((s_cal.good_frames * 100u) / DSO_10BIT_LANE_CAL_TARGET_FRAMES);
    out->last_freq_hz = s_cal.last_freq_hz;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_IsValid
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_10bitLaneCal_IsValid(void)
{
    return s_cal.valid && s_cal.coeffs.valid;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_ShouldApply
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_10bitLaneCal_ShouldApply(void)
{
    if (DSO_SampleSpec_GetActiveFormat() != DSO_SAMPLE_FORMAT_10BIT) {
        return false;
    }

    return DSO_10bitLaneCal_IsValid() &&
           (s_cal.state != DSO_10BIT_LANE_CAL_STATE_COLLECTING);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_ApplyWithCoeffs
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_ApplyWithCoeffs(uint16_t *samples,
                                      uint32_t count,
                                      const DSO_10bitLaneCalCoeffs *coeffs,
                                      uint16_t adc_max)
{
    uint32_t i;

    if ((samples == NULL) || (coeffs == NULL) || !coeffs->valid || (count < 3u)) {
        return;
    }

    for (i = 1u; i < count; i += 2u) {
        float slope;
        float corrected;

        estimate_ideal_and_slope(samples, count, i, NULL, &slope);

        corrected = ((float)samples[i] - coeffs->offset_lsb - (coeffs->skew * slope)) /
                    ((fabsf(coeffs->gain) < 1e-6f) ? 1.0f : coeffs->gain);
        samples[i] = clamp_u16_from_f32(corrected, adc_max);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_ApplyInPlace
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_ApplyInPlace(uint16_t *samples,
                                   uint32_t count,
                                   uint16_t adc_max)
{
    DSO_10bitLaneCal_ApplyWithCoeffs(samples, count, &s_cal.coeffs, adc_max);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_SolveFrame
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_10bitLaneCal_SolveFrame(const uint16_t *samples,
                                 uint32_t count,
                                 DSO_10bitLaneCalCoeffs *out)
{
    float aug[3][4] = {{0}};
    uint32_t used = 0u;
    float solution[3];
    uint32_t i;

    if ((out == NULL) || (samples == NULL) || (count < DSO_10BIT_LANE_CAL_MIN_COUNT)) {
        return false;
    }

    for (i = 1u; (i + 1u) < count; i += 2u) {
        float y = (float)samples[i];
        float ideal;
        float slope;
        float x0 = 1.0f;
        float x1;
        float x2;

        estimate_ideal_and_slope(samples, count, i, &ideal, &slope);
        x1 = ideal;
        x2 = slope;

        aug[0][0] += x0 * x0;
        aug[0][1] += x0 * x1;
        aug[0][2] += x0 * x2;
        aug[0][3] += x0 * y;

        aug[1][0] += x1 * x0;
        aug[1][1] += x1 * x1;
        aug[1][2] += x1 * x2;
        aug[1][3] += x1 * y;

        aug[2][0] += x2 * x0;
        aug[2][1] += x2 * x1;
        aug[2][2] += x2 * x2;
        aug[2][3] += x2 * y;
        used++;
    }

    if ((used < 64u) || !solve3x3(aug, solution)) {
        return false;
    }

    if ((solution[1] < 0.7f) || (solution[1] > 1.3f) ||
        (fabsf(solution[2]) > 1.0f) || (fabsf(solution[0]) > 256.0f)) {
        return false;
    }

    out->offset_lsb = solution[0];
    out->gain = solution[1];
    out->skew = solution[2];
    out->valid = true;
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_ServiceFrame
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_ServiceFrame(const uint16_t *samples,
                                   uint32_t count,
                                   const DSO_ProcessAnalysis *analysis)
{
    DSO_10bitLaneCalCoeffs solved;

    if (s_cal.state != DSO_10BIT_LANE_CAL_STATE_COLLECTING) {
        return;
    }
    if (DSO_SampleSpec_GetActiveFormat() != DSO_SAMPLE_FORMAT_10BIT) {
        s_cal.state = DSO_10BIT_LANE_CAL_STATE_FAILED;
        s_cal.error = DSO_10BIT_LANE_CAL_ERR_NO_SIGNAL;
        return;
    }

    if ((analysis == NULL) || !analysis->freq_valid || (count < DSO_10BIT_LANE_CAL_MIN_COUNT)) {
        s_cal.error = DSO_10BIT_LANE_CAL_ERR_NO_SIGNAL;
        return;
    }

    s_cal.last_freq_hz = analysis->freq_hz;

    if ((analysis->freq_hz < DSO_10BIT_LANE_CAL_MIN_FREQ_HZ) ||
        (analysis->freq_hz > DSO_10BIT_LANE_CAL_MAX_FREQ_HZ)) {
        s_cal.error = DSO_10BIT_LANE_CAL_ERR_NO_SIGNAL;
        return;
    }
    if (analysis->clipped) {
        s_cal.error = DSO_10BIT_LANE_CAL_ERR_CLIPPED;
        return;
    }
    if (analysis->repetitive_confidence < 0.85f) {
        s_cal.error = DSO_10BIT_LANE_CAL_ERR_NOT_REPETITIVE;
        return;
    }
    if (!DSO_10bitLaneCal_SolveFrame(samples, count, &solved)) {
        s_cal.error = DSO_10BIT_LANE_CAL_ERR_SOLVE;
        return;
    }

    s_cal.error = DSO_10BIT_LANE_CAL_ERR_NONE;
    s_cal.accum_offset += solved.offset_lsb;
    s_cal.accum_gain += solved.gain;
    s_cal.accum_skew += solved.skew;
    s_cal.good_frames++;

    if (s_cal.good_frames < DSO_10BIT_LANE_CAL_TARGET_FRAMES) {
        return;
    }

    s_cal.coeffs.offset_lsb = s_cal.accum_offset / (float)s_cal.good_frames;
    s_cal.coeffs.gain = clampf_local(s_cal.accum_gain / (float)s_cal.good_frames, 0.8f, 1.2f);
    s_cal.coeffs.skew = clampf_local(s_cal.accum_skew / (float)s_cal.good_frames, -0.75f, 0.75f);
    s_cal.coeffs.valid = true;
    s_cal.valid = true;

    if (!storage_save(&s_cal.coeffs)) {
        s_cal.state = DSO_10BIT_LANE_CAL_STATE_FAILED;
        s_cal.error = DSO_10BIT_LANE_CAL_ERR_STORE;
        return;
    }

    s_cal.state = DSO_10BIT_LANE_CAL_STATE_SUCCESS;
}
