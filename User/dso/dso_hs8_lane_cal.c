/**
 * @file dso_hs8_lane_cal.c
 * @brief HS8 lane cal 实现。
 */

#include "dso_hs8_lane_cal.h"

#include <math.h>
#include <string.h>

/* 宏定义: DSO_HS8_LANE_CAL_MIN_COUNT - 计数、阈值、容量或深度配置常量。 */
#define DSO_HS8_LANE_CAL_MIN_COUNT            1024u
/* Bootstrap threshold: must be low enough to break the chicken-and-egg loop
 * where uncalibrated lane offset/gain mismatch injects fs/3 spurs that inflate
 * period jitter, which in turn keeps rep_conf below the old 0.85 ceiling.
 * After the first solve lands, the mismatch disappears and rep_conf rises
 * naturally into the 0.85+ band. */
#define DSO_HS8_LANE_CAL_MIN_REPETITIVE       0.50f
/* 宏定义: DSO_HS8_LANE_CAL_FREQ_RESET_TOL - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HS8_LANE_CAL_FREQ_RESET_TOL       0.10f
/* 宏定义: DSO_HS8_LANE_CAL_READY_FRAMES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HS8_LANE_CAL_READY_FRAMES         3u
/* 宏定义: DSO_HS8_LANE_CAL_BLEND_ALPHA - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HS8_LANE_CAL_BLEND_ALPHA          0.25f
/* 宏定义: DSO_HS8_LANE_CAL_MIN_GAIN - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HS8_LANE_CAL_MIN_GAIN             0.50f
/* 宏定义: DSO_HS8_LANE_CAL_MAX_GAIN - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HS8_LANE_CAL_MAX_GAIN             1.50f
/* 宏定义: DSO_HS8_LANE_CAL_MIN_LANE_SPC - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HS8_LANE_CAL_MIN_LANE_SPC         4.0f

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
*   函 数 名: cubic_lagrange_weights
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void cubic_lagrange_weights(float u, float w[4], float dw[4])
{
    static const float s_nodes[4] = {-1.0f, 0.0f, 1.0f, 2.0f};

    for (uint32_t i = 0u; i < 4u; ++i) {
        float wi = 1.0f;
        float dwi = 0.0f;

        for (uint32_t j = 0u; j < 4u; ++j) {
            if (j == i) {
                continue;
            }
            wi *= (u - s_nodes[j]) / (s_nodes[i] - s_nodes[j]);
        }

        for (uint32_t k = 0u; k < 4u; ++k) {
            float term = 1.0f;

            if (k == i) {
                continue;
            }

            term /= (s_nodes[i] - s_nodes[k]);
            for (uint32_t j = 0u; j < 4u; ++j) {
                if ((j == i) || (j == k)) {
                    continue;
                }
                term *= (u - s_nodes[j]) / (s_nodes[i] - s_nodes[j]);
            }
            dwi += term;
        }

        w[i] = wi;
        dw[i] = dwi;
    }
}

/*
*********************************************************************************************************
*   函 数 名: estimate_from_lane0
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool estimate_from_lane0(const uint16_t *samples,
                                uint32_t count,
                                uint32_t index,
                                float lane_u,
                                uint8_t lane_count,
                                float *ideal,
                                float *slope)
{
    int32_t base;
    float w[4];
    float dw[4];
    float values[4];

    if ((samples == NULL) || (slope == NULL) || (index >= count) ||
        (lane_count < 2u) || (lane_count > 3u)) {
        return false;
    }

    base = (int32_t)index - (int32_t)(index % lane_count);
    if (((base - (int32_t)lane_count) < 0) ||
        ((uint32_t)(base + (2 * (int32_t)lane_count)) >= count)) {
        return false;
    }

    values[0] = (float)samples[(uint32_t)(base - (int32_t)lane_count)];
    values[1] = (float)samples[(uint32_t)base];
    values[2] = (float)samples[(uint32_t)(base + (int32_t)lane_count)];
    values[3] = (float)samples[(uint32_t)(base + (2 * (int32_t)lane_count))];

    cubic_lagrange_weights(lane_u, w, dw);
    if (ideal != NULL) {
        *ideal = (w[0] * values[0]) +
                 (w[1] * values[1]) +
                 (w[2] * values[2]) +
                 (w[3] * values[3]);
    }
    *slope = ((dw[0] * values[0]) +
              (dw[1] * values[1]) +
              (dw[2] * values[2]) +
              (dw[3] * values[3])) / (float)lane_count;
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: set_identity
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void set_identity(DSO_HS8LaneCoeffs *coeffs)
{
    if (coeffs == NULL) {
        return;
    }

    coeffs->offset_lsb = 0.0f;
    coeffs->gain = 1.0f;
    coeffs->skew = 0.0f;
    coeffs->valid = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_IsSampleDensityUsable
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_IsSampleDensityUsable(float total_samples_per_cycle,
                                          uint8_t lane_count)
{
    if ((lane_count < 2u) || (total_samples_per_cycle <= 0.0f)) {
        return false;
    }

    return (total_samples_per_cycle / (float)lane_count) >= DSO_HS8_LANE_CAL_MIN_LANE_SPC;
}

/*
*********************************************************************************************************
*   函 数 名: solve3x3
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool solve3x3(float aug[3][4], float out[3])
{
    for (uint32_t row = 0u; row < 3u; ++row) {
        uint32_t pivot = row;
        float pivot_abs = fabsf(aug[row][row]);

        for (uint32_t cand = row + 1u; cand < 3u; ++cand) {
            float cand_abs = fabsf(aug[cand][row]);
            if (cand_abs > pivot_abs) {
                pivot = cand;
                pivot_abs = cand_abs;
            }
        }

        if (pivot_abs < 1e-6f) {
            return false;
        }

        if (pivot != row) {
            for (uint32_t col = row; col < 4u; ++col) {
                float tmp = aug[row][col];
                aug[row][col] = aug[pivot][col];
                aug[pivot][col] = tmp;
            }
        }

        {
            float inv = 1.0f / aug[row][row];
            for (uint32_t col = row; col < 4u; ++col) {
                aug[row][col] *= inv;
            }
        }

        for (uint32_t other = 0u; other < 3u; ++other) {
            if (other == row) {
                continue;
            }
            if (fabsf(aug[other][row]) > 1e-6f) {
                float factor = aug[other][row];
                for (uint32_t col = row; col < 4u; ++col) {
                    aug[other][col] -= factor * aug[row][col];
                }
            }
        }
    }

    out[0] = aug[0][3];
    out[1] = aug[1][3];
    out[2] = aug[2][3];
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: solve_lane
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool solve_lane(const uint16_t *samples,
                       uint32_t count,
                       uint32_t lane,
                       uint8_t lane_count,
                       uint16_t adc_max,
                       DSO_HS8LaneCoeffs *out,
                       uint32_t *out_used,
                       DSO_HS8LaneSolveFail *out_fail)
{
    float aug[3][4] = {{0}};
    float solution[3];
    uint32_t used = 0u;

    if (out_used != NULL) *out_used = 0u;
    if (out_fail != NULL) *out_fail = DSO_HS8_LANE_SOLVE_FAIL_NONE;

    if ((samples == NULL) || (out == NULL) || (count < DSO_HS8_LANE_CAL_MIN_COUNT) ||
        (lane_count < 2u) || (lane_count > 3u) ||
        (lane == 0u) || (lane >= lane_count)) {
        if (out_fail != NULL) *out_fail = DSO_HS8_LANE_SOLVE_FAIL_ARG;
        return false;
    }

    for (uint32_t i = lane; i < count; i += lane_count) {
        float ideal;
        float slope;
        float y;
        float lane_u;

        /* Skip railed samples — they bias the LS fit towards the ADC limits
         * and no longer carry true amplitude information. */
        if ((samples[i] == 0u) || (samples[i] >= adc_max)) {
            continue;
        }

        lane_u = (float)lane / (float)lane_count;
        if (!estimate_from_lane0(samples, count, i, lane_u, lane_count, &ideal, &slope)) {
            continue;
        }
        y = (float)samples[i];

        aug[0][0] += 1.0f;
        aug[0][1] += ideal;
        aug[0][2] += slope;
        aug[0][3] += y;

        aug[1][0] += ideal;
        aug[1][1] += ideal * ideal;
        aug[1][2] += ideal * slope;
        aug[1][3] += ideal * y;

        aug[2][0] += slope;
        aug[2][1] += slope * ideal;
        aug[2][2] += slope * slope;
        aug[2][3] += slope * y;
        used++;
    }

    if (out_used != NULL) *out_used = used;

    if (used < 16u) {
        if (out_fail != NULL) *out_fail = DSO_HS8_LANE_SOLVE_FAIL_USED;
        return false;
    }
    if (!solve3x3(aug, solution)) {
        if (out_fail != NULL) *out_fail = DSO_HS8_LANE_SOLVE_FAIL_SINGULAR;
        return false;
    }

    /* For the current 8-bit dual-lane backend both lanes observe the same
     * analog source with only a phase offset, so a large fitted gain almost
     * always means the solver locked onto waveform shape instead of true lane
     * mismatch. Keep the acceptance window tight enough to reject those false
     * solves and leave the chain at identity until a sane estimate appears. */
    if ((solution[1] < DSO_HS8_LANE_CAL_MIN_GAIN) ||
        (solution[1] > DSO_HS8_LANE_CAL_MAX_GAIN)) {
        if (out_fail != NULL) *out_fail = DSO_HS8_LANE_SOLVE_FAIL_GAIN;
        return false;
    }
    if (fabsf(solution[2]) > 10.0f) {
        if (out_fail != NULL) *out_fail = DSO_HS8_LANE_SOLVE_FAIL_SKEW;
        return false;
    }
    if (fabsf(solution[0]) > (float)(2u * adc_max)) {
        if (out_fail != NULL) *out_fail = DSO_HS8_LANE_SOLVE_FAIL_OFFSET;
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
*   函 数 名: DSO_HS8LaneCal_Reset
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_Reset(DSO_HS8LaneCalState *state)
{
    DSO_HS8LaneCal_ResetForLaneCount(state, 3u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ResetForLaneCount
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_ResetForLaneCount(DSO_HS8LaneCalState *state, uint8_t lane_count)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    set_identity(&state->phase[0]);
    set_identity(&state->phase[1]);
    set_identity(&state->phase[2]);
    if (lane_count <= 1u) {
        state->ready = true;
        state->last_stage = (uint8_t)DSO_HS8_LANE_CAL_STAGE_READY;
    } else {
        state->phase[1].valid = false;
        state->phase[2].valid = false;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_IsReady
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_IsReady(const DSO_HS8LaneCalState *state)
{
    return DSO_HS8LaneCal_IsReadyForLaneCount(state, 3u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_IsReadyForLaneCount
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_IsReadyForLaneCount(const DSO_HS8LaneCalState *state,
                                        uint8_t lane_count)
{
    if (lane_count <= 1u) {
        return state != NULL;
    }

    return (state != NULL) &&
           state->ready &&
           state->phase[1].valid &&
           ((lane_count < 3u) || state->phase[2].valid);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_SolveFrame
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_SolveFrame(const uint16_t *samples,
                               uint32_t count,
                               uint16_t adc_max,
                               DSO_HS8LaneCoeffs phase[3])
{
    return DSO_HS8LaneCal_SolveFrameEx(samples, count, adc_max, 3u, phase);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_SolveFrameEx
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_SolveFrameEx(const uint16_t *samples,
                                 uint32_t count,
                                 uint16_t adc_max,
                                 uint8_t lane_count,
                                 DSO_HS8LaneCoeffs phase[3])
{
    uint32_t used_dummy = 0u;
    DSO_HS8LaneSolveFail fail_dummy = DSO_HS8_LANE_SOLVE_FAIL_NONE;

    if (phase == NULL) {
        return false;
    }

    set_identity(&phase[0]);
    set_identity(&phase[1]);
    set_identity(&phase[2]);
    phase[2].valid = (lane_count >= 3u);

    if (lane_count <= 1u) {
        return true;
    }

    if (!solve_lane(samples, count, 1u, lane_count, adc_max, &phase[1], &used_dummy, &fail_dummy)) {
        return false;
    }
    if ((lane_count >= 3u) &&
        !solve_lane(samples, count, 2u, lane_count, adc_max, &phase[2], &used_dummy, &fail_dummy)) {
        return false;
    }
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ServiceFrame
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_ServiceFrame(DSO_HS8LaneCalState *state,
                                 const uint16_t *samples,
                                 uint32_t count,
                                 uint16_t adc_max,
                                 float freq_hz,
                                 bool freq_valid,
                                 bool clipped,
                                 float repetitive_confidence)
{
    return DSO_HS8LaneCal_ServiceFrameEx(state,
                                         samples,
                                         count,
                                         adc_max,
                                         freq_hz,
                                         freq_valid,
                                         clipped,
                                         repetitive_confidence,
                                         3u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ServiceFrameEx
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_ServiceFrameEx(DSO_HS8LaneCalState *state,
                                   const uint16_t *samples,
                                   uint32_t count,
                                   uint16_t adc_max,
                                   float freq_hz,
                                   bool freq_valid,
                                   bool clipped,
                                   float repetitive_confidence,
                                   uint8_t lane_count)
{
    DSO_HS8LaneCoeffs solved[3];
    uint32_t used1 = 0u;
    uint32_t used2 = 0u;
    DSO_HS8LaneSolveFail fail1 = DSO_HS8_LANE_SOLVE_FAIL_NONE;
    DSO_HS8LaneSolveFail fail2 = DSO_HS8_LANE_SOLVE_FAIL_NONE;

    /* `clipped` no longer hard-blocks calibration: solve_lane skips railed
     * samples internally, so a near-full-scale sine can still converge. */
    (void)clipped;

    if (state != NULL) {
        state->service_calls++;
    }

    if (lane_count <= 1u) {
        if (state != NULL) {
            state->ready = true;
            state->last_stage = (uint8_t)DSO_HS8_LANE_CAL_STAGE_READY;
        }
        return true;
    }
    if ((state == NULL) || (samples == NULL) || (count < DSO_HS8_LANE_CAL_MIN_COUNT)) {
        if (state != NULL) state->last_stage = (uint8_t)DSO_HS8_LANE_CAL_STAGE_REJECT_ARG;
        return false;
    }
    if (!freq_valid || (freq_hz <= 0.0f)) {
        state->last_stage = (uint8_t)DSO_HS8_LANE_CAL_STAGE_REJECT_FREQ;
        return false;
    }
    if (repetitive_confidence < DSO_HS8_LANE_CAL_MIN_REPETITIVE) {
        state->last_stage = (uint8_t)DSO_HS8_LANE_CAL_STAGE_REJECT_REP;
        return false;
    }

    if (state->locked_freq_hz > 0.0f) {
        float diff = fabsf(freq_hz - state->locked_freq_hz) / state->locked_freq_hz;
        if (diff > DSO_HS8_LANE_CAL_FREQ_RESET_TOL) {
            DSO_HS8LaneCal_Reset(state);
            state->last_stage = (uint8_t)DSO_HS8_LANE_CAL_STAGE_RESET_FREQ;
            return false;
        }
    }

    set_identity(&solved[0]);
    set_identity(&solved[1]);
    set_identity(&solved[2]);
    if (lane_count < 3u) {
        solved[2].valid = false;
    }

    if (!solve_lane(samples, count, 1u, lane_count, adc_max, &solved[1], &used1, &fail1)) {
        state->last_lane1_used = used1;
        state->last_lane1_fail = (uint8_t)fail1;
        state->last_stage = (uint8_t)DSO_HS8_LANE_CAL_STAGE_SOLVE_FAIL_1;
        return false;
    }
    state->last_lane1_used = used1;
    state->last_lane1_fail = (uint8_t)fail1;

    if (lane_count >= 3u) {
        if (!solve_lane(samples, count, 2u, lane_count, adc_max, &solved[2], &used2, &fail2)) {
            state->last_lane2_used = used2;
            state->last_lane2_fail = (uint8_t)fail2;
            state->last_stage = (uint8_t)DSO_HS8_LANE_CAL_STAGE_SOLVE_FAIL_2;
            return false;
        }
    }
    state->last_lane2_used = used2;
    state->last_lane2_fail = (uint8_t)fail2;

    if (state->good_frames == 0u) {
        state->phase[0] = solved[0];
        state->phase[1] = solved[1];
        state->phase[2] = solved[2];
        state->good_frames = 1u;
    } else {
        for (uint32_t lane = 1u; lane < lane_count; ++lane) {
            state->phase[lane].offset_lsb =
                ((1.0f - DSO_HS8_LANE_CAL_BLEND_ALPHA) * state->phase[lane].offset_lsb) +
                (DSO_HS8_LANE_CAL_BLEND_ALPHA * solved[lane].offset_lsb);
            state->phase[lane].gain =
                ((1.0f - DSO_HS8_LANE_CAL_BLEND_ALPHA) * state->phase[lane].gain) +
                (DSO_HS8_LANE_CAL_BLEND_ALPHA * solved[lane].gain);
            state->phase[lane].skew =
                ((1.0f - DSO_HS8_LANE_CAL_BLEND_ALPHA) * state->phase[lane].skew) +
                (DSO_HS8_LANE_CAL_BLEND_ALPHA * solved[lane].skew);
            state->phase[lane].valid = true;
        }
        if (state->good_frames < UINT8_MAX) {
            state->good_frames++;
        }
    }

    state->locked_freq_hz = freq_hz;
    state->ready = (state->good_frames >= DSO_HS8_LANE_CAL_READY_FRAMES);
    state->last_stage = state->ready
                      ? (uint8_t)DSO_HS8_LANE_CAL_STAGE_READY
                      : (uint8_t)DSO_HS8_LANE_CAL_STAGE_SOLVED;
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ApplyWithCoeffs
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_ApplyWithCoeffs(const DSO_HS8LaneCoeffs phase[3],
                                    uint16_t adc_max,
                                    uint16_t *samples,
                                    uint32_t count)
{
    DSO_HS8LaneCal_ApplyWithCoeffsEx(phase, adc_max, samples, count, 3u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ApplyWithCoeffsEx
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_ApplyWithCoeffsEx(const DSO_HS8LaneCoeffs phase[3],
                                      uint16_t adc_max,
                                      uint16_t *samples,
                                      uint32_t count,
                                      uint8_t lane_count)
{
    if ((phase == NULL) || (samples == NULL) || (count < 5u)) {
        return;
    }
    if (lane_count <= 1u) {
        return;
    }

    for (uint32_t i = 1u; i + 1u < count; ++i) {
        uint32_t lane = i % lane_count;

        if ((lane != 0u) && phase[lane].valid) {
            float lane_u = (float)lane / (float)lane_count;
            float slope;
            float corrected;

            if (!estimate_from_lane0(samples, count, i, lane_u, lane_count, NULL, &slope)) {
                continue;
            }

            corrected = ((float)samples[i] -
                         phase[lane].offset_lsb -
                         (phase[lane].skew * slope)) /
                        ((fabsf(phase[lane].gain) < 1e-6f) ? 1.0f : phase[lane].gain);
            samples[i] = clamp_u16_from_f32(corrected, adc_max);
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_Apply
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_Apply(const DSO_HS8LaneCalState *state,
                          uint16_t adc_max,
                          uint16_t *samples,
                          uint32_t count)
{
    DSO_HS8LaneCal_ApplyEx(state, adc_max, samples, count, 3u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ApplyEx
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_ApplyEx(const DSO_HS8LaneCalState *state,
                            uint16_t adc_max,
                            uint16_t *samples,
                            uint32_t count,
                            uint8_t lane_count)
{
    if (!DSO_HS8LaneCal_IsReadyForLaneCount(state, lane_count)) {
        return;
    }

    DSO_HS8LaneCal_ApplyWithCoeffsEx(state->phase, adc_max, samples, count, lane_count);
}
