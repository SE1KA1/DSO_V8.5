/**
 * @file dso_engine_cal.c
 * @brief 引擎校准 Apply 与 HS8 重入策略。
 */

/*
 * dso_engine_cal.c — HS8 / 10-bit calibration subsystem extracted from dso_engine.c
 *
 * Pure mechanical extraction — zero algorithm changes.
 */

#include "dso_engine_cal.h"
#include "dso_acq.h"
#include "dso_10bit_lane_cal.h"
#include "dso_config.h"
#include "dso_timebase.h"
#include "dso_sample_spec.h"

#include <math.h>
#include <string.h>

/* ── Persistent state ── */

typedef enum {
    DSO_ENGINE_HS8_REENTRY_IDLE = 0,
    DSO_ENGINE_HS8_REENTRY_SETTLING,
    DSO_ENGINE_HS8_REENTRY_CALIBRATING,
    DSO_ENGINE_HS8_REENTRY_READY,
} DSO_EngineHs8ReentryState;

static DSO_EngineHs8ReentryState s_engine_hs8_reentry_state;
static uint8_t s_engine_hs8_reentry_settle_frames;
static bool s_engine_hs8_cal_ready_prev;
static DSO_HS8LaneCalState s_engine_hs8_lane_cal;
static uint8_t s_engine_hs8_lane2_fail_streak;
/*
*********************************************************************************************************
*   函 数 名: __attribute__
*   功能说明: 在引擎校准链路中完成交织校准服务、校准结果应用或低频 lane 参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t s_engine_hs8_lane_freq_samples[DSO_DMA_BUF_SIZE] __attribute__((section(".RAM_D1"), aligned(32)));

/* ── Reentry state machine ── */

static void engine_hs8_reentry_begin(void)
{
    s_engine_hs8_cal_ready_prev = false;

    if (DSO_EngineCal_IsHs8Active() && (DSO_EngineCal_Hs8LaneCount() >= 2u)) {
        s_engine_hs8_reentry_state = DSO_ENGINE_HS8_REENTRY_SETTLING;
        s_engine_hs8_reentry_settle_frames = 2u;
    } else {
        s_engine_hs8_reentry_state = DSO_ENGINE_HS8_REENTRY_IDLE;
        s_engine_hs8_reentry_settle_frames = 0u;
    }
}

/*
*********************************************************************************************************
*   函 数 名: engine_hs8_reentry_allows_calibration
*   功能说明: 在引擎校准链路中完成交织校准服务、校准结果应用或低频 lane 参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool engine_hs8_reentry_allows_calibration(void)
{
    if (s_engine_hs8_reentry_state == DSO_ENGINE_HS8_REENTRY_SETTLING) {
        if (s_engine_hs8_reentry_settle_frames > 0u) {
            s_engine_hs8_reentry_settle_frames--;
            return false;
        }
        s_engine_hs8_reentry_state = DSO_ENGINE_HS8_REENTRY_CALIBRATING;
    }

    return s_engine_hs8_reentry_state != DSO_ENGINE_HS8_REENTRY_IDLE;
}

/*
*********************************************************************************************************
*   函 数 名: engine_hs8_reentry_note_ready
*   功能说明: 在引擎校准链路中完成交织校准服务、校准结果应用或低频 lane 参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_hs8_reentry_note_ready(bool ready)
{
    if (!DSO_EngineCal_IsHs8Active()) {
        s_engine_hs8_reentry_state = DSO_ENGINE_HS8_REENTRY_IDLE;
        s_engine_hs8_reentry_settle_frames = 0u;
        s_engine_hs8_cal_ready_prev = false;
        return;
    }

    if (ready && !s_engine_hs8_cal_ready_prev) {
        DSO_Timebase_ResetHs8ReconstructionForChannel(0u);
        s_engine_hs8_reentry_state = DSO_ENGINE_HS8_REENTRY_READY;
    } else if (!ready && (s_engine_hs8_reentry_state == DSO_ENGINE_HS8_REENTRY_READY)) {
        s_engine_hs8_reentry_state = DSO_ENGINE_HS8_REENTRY_CALIBRATING;
    }
    s_engine_hs8_cal_ready_prev = ready;
}

/* ── Lane0 bootstrap measurement ── */

static bool engine_hs8_measure_lane0_bootstrap(const uint16_t *samples,
                                               uint32_t count,
                                               uint32_t sample_rate,
                                               const DSO_TriggerConfig *trig_cfg,
                                               DSO_ProcessAnalysis *lane_analysis,
                                               uint8_t *out_lane_count)
{
    uint32_t lane_count;
    uint32_t lane_rate;
    uint32_t lane_sample_count = 0u;
    uint32_t i;

    if ((samples == NULL) || (lane_analysis == NULL) || (trig_cfg == NULL) ||
        (sample_rate == 0u) || !DSO_EngineCal_IsHs8Active()) {
        return false;
    }
    if (out_lane_count != NULL) {
        *out_lane_count = 0u;
    }

    lane_count = DSO_EngineCal_Hs8LaneCount();
    if (lane_count < 2u) {
        return false;
    }

    lane_rate = sample_rate / lane_count;
    if (lane_rate == 0u) {
        return false;
    }

    for (i = 0u; (i < count) && (lane_sample_count < DSO_DMA_BUF_SIZE); i += lane_count) {
        s_engine_hs8_lane_freq_samples[lane_sample_count++] = samples[i];
    }

    if (lane_sample_count < 32u) {
        return false;
    }

    memset(lane_analysis, 0, sizeof(*lane_analysis));
    lane_analysis->trigger_index = -1;
    lane_analysis->trigger_pos_samples = -1.0f;
    DSO_Process_AnalyzeSamplesEx(s_engine_hs8_lane_freq_samples,
                                 lane_sample_count,
                                 lane_rate,
                                 trig_cfg,
                                 lane_analysis);

    if (!lane_analysis->freq_valid ||
        (lane_analysis->freq_hz <= 0.0f) ||
        (lane_analysis->freq_hz > DSO_ENGINE_HS8_LANE_BOOTSTRAP_MAX_FREQ_HZ) ||
        (lane_analysis->samples_per_cycle < DSO_ENGINE_HS8_LANE_BOOTSTRAP_MIN_SPC) ||
        (lane_analysis->repetitive_confidence < DSO_ENGINE_HS8_LANE_BOOTSTRAP_MIN_REP)) {
        return false;
    }
    if (out_lane_count != NULL) {
        *out_lane_count = (uint8_t)lane_count;
    }
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: engine_hs8_apply_lane0_bootstrap
*   功能说明: 在引擎校准链路中完成交织校准服务、校准结果应用或低频 lane 参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_hs8_apply_lane0_bootstrap(DSO_ProcessAnalysis *analysis,
                                             uint32_t sample_rate,
                                             uint8_t lane_count,
                                             const DSO_ProcessAnalysis *lane_analysis)
{
    if ((analysis == NULL) || (lane_analysis == NULL) ||
        (lane_count < 2u) || (sample_rate == 0u) || (lane_analysis->freq_hz <= 0.0f)) {
        return;
    }

    analysis->freq_hz = lane_analysis->freq_hz;
    analysis->freq_valid = true;
    analysis->samples_per_cycle = (float)sample_rate / lane_analysis->freq_hz;
    analysis->crossing_count = lane_analysis->crossing_count * (uint32_t)lane_count;
    analysis->period_jitter_pct = lane_analysis->period_jitter_pct;
    analysis->clipped = lane_analysis->clipped;
    if (analysis->repetitive_confidence < lane_analysis->repetitive_confidence) {
        analysis->repetitive_confidence = lane_analysis->repetitive_confidence;
    }
}

/* ── Calibration gate checks ── */

static bool engine_hs8_can_calibrate(uint8_t lane_count,
                                     const DSO_ProcessAnalysis *analysis)
{
    if ((analysis == NULL) || (lane_count < 2u)) {
        return false;
    }

    if (!analysis->freq_valid ||
        (analysis->freq_hz <= 0.0f) ||
        (analysis->freq_hz > DSO_ENGINE_HS8_CAL_MAX_FREQ_HZ) ||
        analysis->clipped ||
        (analysis->repetitive_confidence < 0.85f)) {
        return false;
    }

    return DSO_HS8LaneCal_IsSampleDensityUsable(analysis->samples_per_cycle, lane_count);
}

/*
*********************************************************************************************************
*   函 数 名: engine_hs8_build_cal_analysis
*   功能说明: 在引擎校准链路中完成交织校准服务、校准结果应用或低频 lane 参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool engine_hs8_build_cal_analysis(const uint16_t *samples,
                                          uint32_t count,
                                          uint32_t sample_rate,
                                          const DSO_TriggerConfig *trig_cfg,
                                          const DSO_ProcessAnalysis *raw_analysis,
                                          DSO_ProcessAnalysis *out_analysis)
{
    DSO_ProcessAnalysis analysis_for_cal;

    if ((samples == NULL) || (trig_cfg == NULL) ||
        (raw_analysis == NULL) || (out_analysis == NULL)) {
        return false;
    }

    analysis_for_cal = *raw_analysis;
    {
        DSO_ProcessAnalysis lane_analysis = {0};
        uint8_t lane_count = 0u;

        if (engine_hs8_measure_lane0_bootstrap(samples,
                                               count,
                                               sample_rate,
                                               trig_cfg,
                                               &lane_analysis,
                                               &lane_count)) {
            engine_hs8_apply_lane0_bootstrap(&analysis_for_cal,
                                             sample_rate,
                                             lane_count,
                                             &lane_analysis);
        }
    }

    *out_analysis = analysis_for_cal;
    return true;
}

/* ── Public API ── */

void DSO_EngineCal_Init(void)
{
    DSO_HS8LaneCal_Reset(&s_engine_hs8_lane_cal);
    s_engine_hs8_lane2_fail_streak = 0u;
    engine_hs8_reentry_begin();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_Reset
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineCal_Reset(void)
{
    DSO_HS8LaneCal_Reset(&s_engine_hs8_lane_cal);
    s_engine_hs8_lane2_fail_streak = 0u;
    engine_hs8_reentry_begin();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_ReentryBegin
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineCal_ReentryBegin(void)
{
    engine_hs8_reentry_begin();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_IsCh1Ready
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_EngineCal_IsCh1Ready(void)
{
    if (DSO_EngineCal_IsHs8Active()) {
        if (DSO_EngineCal_Hs8LaneCount() < 2u) {
            return false;
        }
        return DSO_HS8LaneCal_IsReadyForLaneCount(&s_engine_hs8_lane_cal,
                                                  DSO_EngineCal_Hs8LaneCount());
    }

    if (DSO_EngineCal_Is10bitActive()) {
        return DSO_10bitLaneCal_ShouldApply();
    }

    return false;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_ApplyCh1IfReady
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_EngineCal_ApplyCh1IfReady(uint16_t *samples, uint32_t count)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();

    if ((samples == NULL) || (count == 0u)) {
        return false;
    }

    if (DSO_EngineCal_IsHs8Active()) {
        uint8_t lane_count = DSO_EngineCal_Hs8LaneCount();

        if (lane_count < 2u) {
            return false;
        }
        if (!DSO_HS8LaneCal_IsReadyForLaneCount(&s_engine_hs8_lane_cal, lane_count)) {
            return false;
        }

        DSO_HS8LaneCal_ApplyEx(&s_engine_hs8_lane_cal, spec->adc_max, samples, count, lane_count);
        return true;
    }

    if (!DSO_EngineCal_Is10bitActive()) {
        return false;
    }
    if (!DSO_10bitLaneCal_ShouldApply()) {
        return false;
    }

    DSO_10bitLaneCal_ApplyInPlace(samples, count, spec->adc_max);
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_ServiceFrame
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineCal_ServiceFrame(const uint16_t *samples,
                                 uint32_t count,
                                 const DSO_ProcessAnalysis *raw_analysis,
                                 const DSO_TriggerConfig *trig_cfg,
                                 uint32_t sample_rate)
{
    if (DSO_EngineCal_IsHs8Active()) {
        const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
        uint8_t lane_count = DSO_EngineCal_Hs8LaneCount();
        DSO_ProcessAnalysis cal_analysis = {0};
        bool have_cal_analysis;

        have_cal_analysis = engine_hs8_build_cal_analysis(samples,
                                                          count,
                                                          sample_rate,
                                                          trig_cfg,
                                                          raw_analysis,
                                                          &cal_analysis);
        if (engine_hs8_reentry_allows_calibration() &&
            have_cal_analysis &&
            engine_hs8_can_calibrate(lane_count, &cal_analysis)) {
            (void)DSO_HS8LaneCal_ServiceFrameEx(&s_engine_hs8_lane_cal,
                                                samples,
                                                count,
                                                spec->adc_max,
                                                cal_analysis.freq_hz,
                                                cal_analysis.freq_valid,
                                                cal_analysis.clipped,
                                                cal_analysis.repetitive_confidence,
                                                lane_count);
        } else if ((s_engine_hs8_reentry_state != DSO_ENGINE_HS8_REENTRY_SETTLING) &&
                   !DSO_HS8LaneCal_IsReadyForLaneCount(&s_engine_hs8_lane_cal, lane_count)) {
            DSO_HS8LaneCal_ResetForLaneCount(&s_engine_hs8_lane_cal, lane_count);
        }
        s_engine_hs8_lane2_fail_streak = 0u;
    } else if (DSO_EngineCal_Is10bitActive()) {
        DSO_10bitLaneCal_ServiceFrame(samples, count, raw_analysis);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_RefineLowFreqFromLane0
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_EngineCal_RefineLowFreqFromLane0(const uint16_t *samples,
                                           uint32_t count,
                                           uint32_t sample_rate,
                                           const DSO_TriggerConfig *trig_cfg,
                                           DSO_ProcessAnalysis *analysis)
{
    DSO_ProcessAnalysis lane_analysis = {0};
    uint8_t lane_count = 0u;

    if ((analysis == NULL) ||
        !engine_hs8_measure_lane0_bootstrap(samples,
                                            count,
                                            sample_rate,
                                            trig_cfg,
                                            &lane_analysis,
                                            &lane_count)) {
        return false;
    }

    if (analysis->freq_valid && (analysis->freq_hz > 0.0f)) {
        float diff = fabsf(analysis->freq_hz - lane_analysis.freq_hz);

        if ((analysis->freq_hz > DSO_ENGINE_HS8_LANE_BOOTSTRAP_MAX_FREQ_HZ) ||
            ((diff / analysis->freq_hz) > DSO_ENGINE_HS8_LANE_BOOTSTRAP_FREQ_TOL)) {
            return false;
        }
    }

    engine_hs8_apply_lane0_bootstrap(analysis, sample_rate, lane_count, &lane_analysis);
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_SyncTimebase
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineCal_SyncTimebase(void)
{
    bool ch1_ready = true;

    if (DSO_EngineCal_IsHs8Active() && (DSO_EngineCal_Hs8LaneCount() >= 2u)) {
        ch1_ready = DSO_EngineCal_IsCh1Ready();
    }

    engine_hs8_reentry_note_ready(ch1_ready);
    DSO_Timebase_SetHs8CalReadyForChannel(0u, ch1_ready);
    DSO_Timebase_SetHs8CalReadyForChannel(1u, true);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_GetHs8State
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const DSO_HS8LaneCalState *DSO_EngineCal_GetHs8State(void)
{
    return &s_engine_hs8_lane_cal;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_GetLane2FailStreak
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t DSO_EngineCal_GetLane2FailStreak(void)
{
    return s_engine_hs8_lane2_fail_streak;
}
