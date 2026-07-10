/**
 * @file dso_vertical.c
 * @brief 垂直档位与 ADC→plot Y 映射实现。
 */

#include "dso_vertical.h"
#include "dso_sample_spec.h"

#include <math.h>
#include <string.h>

/* 宏定义: DSO_VERTICAL_COUNT - 计数、阈值、容量或深度配置常量。 */
#define DSO_VERTICAL_COUNT 7u

typedef struct {
    uint8_t index;
    float offset_v;
    DSO_ProbeAtten probe;
    DSO_CouplingMode coupling;
    float scale_min_v;
    float scale_max_v;
    float volts_per_div;
    float hp_alpha;
    float hp_prev_x;
    float hp_prev_y;
} DSO_VerticalState;

static const DSO_VdivEntry s_table[DSO_VERTICAL_COUNT] = {
    { 0u,   10u },
    { 1u,   20u },
    { 2u,   50u },
    { 3u,  100u },
    { 4u,  200u },
    { 5u,  500u },
    { 6u, 1000u },
};

static DSO_VerticalState s_vert[DSO_CHANNEL_COUNT];

/*
*********************************************************************************************************
*   函 数 名: vert_state
*   功能说明: 在垂直系统中完成档位、偏移、耦合、探头倍率或屏幕 Y 坐标映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_VerticalState *vert_state(DSO_ChannelId channel)
{
    if (channel >= DSO_CHANNEL_COUNT) {
        channel = DSO_CHANNEL_1;
    }
    return &s_vert[channel];
}

/*
*********************************************************************************************************
*   函 数 名: adc_lsb_volts
*   功能说明: 在垂直系统中完成档位、偏移、耦合、探头倍率或屏幕 Y 坐标映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float adc_lsb_volts(void)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    return spec->vref / (float)spec->adc_max;
}

/*
*********************************************************************************************************
*   函 数 名: refresh_scale
*   功能说明: 在垂直系统中完成档位、偏移、耦合、探头倍率或屏幕 Y 坐标映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void refresh_scale(DSO_VerticalState *state)
{
    float center_v = (DSO_VREF * 0.5f) + state->offset_v;
    float half_range_v = state->volts_per_div * (float)state->probe * 4.0f;

    state->scale_min_v = center_v - half_range_v;
    state->scale_max_v = center_v + half_range_v;
}

/*
*********************************************************************************************************
*   函 数 名: reset_ac_state
*   功能说明: 在垂直系统中完成档位、偏移、耦合、探头倍率或屏幕 Y 坐标映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void reset_ac_state(DSO_VerticalState *state)
{
    state->hp_prev_x = (float)DSO_SampleSpec_GetActive()->midscale;
    state->hp_prev_y = 0.0f;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_Init
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_Init(void)
{
    memset(&s_vert, 0, sizeof(s_vert));
    for (uint32_t i = 0; i < (uint32_t)DSO_CHANNEL_COUNT; i++) {
        s_vert[i].index = 5u;
        s_vert[i].probe = DSO_PROBE_X1;
        s_vert[i].coupling = DSO_COUPLING_DC;
        s_vert[i].volts_per_div = (float)s_table[s_vert[i].index].mv_per_div / 1000.0f;
        refresh_scale(&s_vert[i]);
        reset_ac_state(&s_vert[i]);
    }
    DSO_Vertical_SetOffsetForChannel(DSO_CHANNEL_1, -0.4f);
    DSO_Vertical_SetOffsetForChannel(DSO_CHANNEL_2, 0.4f);
    DSO_Vertical_UpdateViewTime(1000.0f);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_GetCount
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t DSO_Vertical_GetCount(void)
{
    return (uint8_t)DSO_VERTICAL_COUNT;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_GetTable
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const DSO_VdivEntry *DSO_Vertical_GetTable(void)
{
    return s_table;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_GetEntryAt
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const DSO_VdivEntry *DSO_Vertical_GetEntryAt(uint8_t idx)
{
    if (idx >= DSO_VERTICAL_COUNT) {
        return NULL;
    }
    return &s_table[idx];
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_SetIndexForChannel
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_SetIndexForChannel(DSO_ChannelId channel, uint8_t idx)
{
    DSO_VerticalState *state;

    if (idx >= DSO_VERTICAL_COUNT) {
        return;
    }
    state = vert_state(channel);
    state->index = idx;
    state->volts_per_div = (float)s_table[idx].mv_per_div / 1000.0f;
    refresh_scale(state);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_GetIndexForChannel
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t DSO_Vertical_GetIndexForChannel(DSO_ChannelId channel)
{
    return vert_state(channel)->index;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_SetOffsetForChannel
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_SetOffsetForChannel(DSO_ChannelId channel, float volts)
{
    DSO_VerticalState *state = vert_state(channel);

    if (volts > DSO_VREF) volts = DSO_VREF;
    if (volts < -DSO_VREF) volts = -DSO_VREF;
    state->offset_v = volts;
    refresh_scale(state);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_OffsetReset
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_OffsetReset(void)
{
    DSO_Vertical_SetOffsetForChannel(DSO_CHANNEL_1, 0.0f);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_GetOffsetForChannel
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_Vertical_GetOffsetForChannel(DSO_ChannelId channel)
{
    return vert_state(channel)->offset_v;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_SetProbeForChannel
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_SetProbeForChannel(DSO_ChannelId channel, DSO_ProbeAtten atten)
{
    DSO_VerticalState *state;

    if ((atten != DSO_PROBE_X1) && (atten != DSO_PROBE_X10)) {
        return;
    }
    state = vert_state(channel);
    state->probe = atten;
    refresh_scale(state);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_SetCouplingForChannel
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_SetCouplingForChannel(DSO_ChannelId channel, DSO_CouplingMode mode)
{
    DSO_VerticalState *state = vert_state(channel);

    if ((unsigned)mode > (unsigned)DSO_COUPLING_GND) {
        return;
    }
    state->coupling = mode;
    reset_ac_state(state);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_MapYForChannel
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_MapYForChannel(DSO_ChannelId channel,
                                 const uint16_t *samples,
                                 uint32_t count,
                                 int16_t *y_out)
{
    const DSO_VerticalState *state = vert_state(channel);
    float lsb = adc_lsb_volts();
    float scale_min_adc = state->scale_min_v / lsb;
    float scale_max_adc = state->scale_max_v / lsb;
    float y_range = (float)(DSO_DISPLAY_HEIGHT - 1u - 2u * DSO_PIXEL_MARGIN);
    float y_bottom = (float)(DSO_DISPLAY_HEIGHT - 1u - DSO_PIXEL_MARGIN);
    float denom = scale_max_adc - scale_min_adc;

    if ((samples == NULL) || (y_out == NULL) || (count == 0u)) {
        return;
    }

    if (denom < 1.0f) {
        denom = 1.0f;
    }

    if (state->coupling == DSO_COUPLING_GND) {
        uint16_t mid = DSO_SampleSpec_GetActive()->midscale;
        float norm = ((float)mid - scale_min_adc) / denom;
        float y0 = y_bottom - (norm * y_range);
        if (y0 < 0.0f) {
            y0 = 0.0f;
        }
        if (y0 > (float)(DSO_DISPLAY_HEIGHT - 1u)) {
            y0 = (float)(DSO_DISPLAY_HEIGHT - 1u);
        }
        {
            int16_t yflat = (int16_t)(y0 + 0.5f);
            for (uint32_t i = 0u; i < count; i++) {
                y_out[i] = yflat;
            }
        }
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        float norm = ((float)samples[i] - scale_min_adc) / denom;
        float y = y_bottom - (norm * y_range);

        if (y < 0.0f) y = 0.0f;
        if (y > (float)(DSO_DISPLAY_HEIGHT - 1u)) y = (float)(DSO_DISPLAY_HEIGHT - 1u);
        y_out[i] = (int16_t)(y + 0.5f);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_MapSampleToPlotYForChannel
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int16_t DSO_Vertical_MapSampleToPlotYForChannel(DSO_ChannelId channel, uint16_t sample_adc)
{
    const DSO_VerticalState *state = vert_state(channel);
    float lsb = adc_lsb_volts();
    float scale_min_adc = state->scale_min_v / lsb;
    float scale_max_adc = state->scale_max_v / lsb;
    float y_range = (float)(DSO_DISPLAY_HEIGHT - 1u - 2u * DSO_PIXEL_MARGIN);
    float y_bottom = (float)(DSO_DISPLAY_HEIGHT - 1u - DSO_PIXEL_MARGIN);
    float denom = scale_max_adc - scale_min_adc;
    float norm;
    float y;

    if (denom < 1.0f) {
        denom = 1.0f;
    }

    if (state->coupling == DSO_COUPLING_GND) {
        sample_adc = DSO_SampleSpec_GetActive()->midscale;
    }

    norm = ((float)sample_adc - scale_min_adc) / denom;
    y = y_bottom - (norm * y_range);
    if (y < 0.0f) {
        y = 0.0f;
    }
    if (y > (float)(DSO_DISPLAY_HEIGHT - 1u)) {
        y = (float)(DSO_DISPLAY_HEIGHT - 1u);
    }
    return (int16_t)(y + 0.5f);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_ApplyACDisplayFilterForChannel
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_ApplyACDisplayFilterForChannel(DSO_ChannelId channel,
                                                 uint16_t *samples,
                                                 uint32_t count)
{
    DSO_VerticalState *state = vert_state(channel);

    if ((samples == NULL) || (count == 0u) || (state->coupling != DSO_COUPLING_AC)) {
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        float x = (float)samples[i];
        float y = state->hp_alpha * (state->hp_prev_y + x - state->hp_prev_x);
        float out = (float)DSO_SampleSpec_GetActive()->midscale + y;

        state->hp_prev_x = x;
        state->hp_prev_y = y;

        if (out < 0.0f) out = 0.0f;
        if (out > (float)DSO_SampleSpec_GetActive()->adc_max) {
            out = (float)DSO_SampleSpec_GetActive()->adc_max;
        }
        samples[i] = (uint16_t)(out + 0.5f);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_UpdateViewTime
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_UpdateViewTime(float time_per_pixel_ns)
{
    const float cutoff_hz = 1.0f;
    float dt_s = time_per_pixel_ns * 1e-9f;
    float rc = 1.0f / (2.0f * 3.14159265358979323846f * cutoff_hz);

    if (dt_s <= 0.0f) {
        dt_s = 1e-9f;
    }

    for (uint32_t i = 0; i < (uint32_t)DSO_CHANNEL_COUNT; i++) {
        s_vert[i].hp_alpha = rc / (rc + dt_s);
        reset_ac_state(&s_vert[i]);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_GetInfoForChannel
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_GetInfoForChannel(DSO_ChannelId channel, DSO_VerticalInfo *out)
{
    const DSO_VerticalState *state = vert_state(channel);

    if (out == NULL) {
        return;
    }

    out->vdiv_index = state->index;
    out->mv_per_div = s_table[state->index].mv_per_div;
    out->volts_per_div = state->volts_per_div;
    out->offset_v = state->offset_v;
    out->probe = state->probe;
    out->coupling = state->coupling;
    out->scale_min_v = state->scale_min_v;
    out->scale_max_v = state->scale_max_v;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_VoltageAtPixelYForChannel
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_Vertical_VoltageAtPixelYForChannel(DSO_ChannelId channel, int16_t y)
{
    const DSO_VerticalState *state = vert_state(channel);
    float y_range = (float)(DSO_DISPLAY_HEIGHT - 1u - 2u * DSO_PIXEL_MARGIN);
    float y_bottom = (float)(DSO_DISPLAY_HEIGHT - 1u - DSO_PIXEL_MARGIN);
    float norm = (y_bottom - (float)y) / y_range;
    return state->scale_min_v + norm * (state->scale_max_v - state->scale_min_v);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_SampleToVolts
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_Vertical_SampleToVolts(uint16_t sample)
{
    return (float)sample * adc_lsb_volts();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_SetIndex
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_SetIndex(uint8_t idx)
{
    DSO_Vertical_SetIndexForChannel(DSO_CHANNEL_1, idx);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_GetIndex
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t DSO_Vertical_GetIndex(void)
{
    return DSO_Vertical_GetIndexForChannel(DSO_CHANNEL_1);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_SetOffset
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_SetOffset(float volts)
{
    DSO_Vertical_SetOffsetForChannel(DSO_CHANNEL_1, volts);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_GetOffset
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_Vertical_GetOffset(void)
{
    return DSO_Vertical_GetOffsetForChannel(DSO_CHANNEL_1);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_SetProbe
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_SetProbe(DSO_ProbeAtten atten)
{
    DSO_Vertical_SetProbeForChannel(DSO_CHANNEL_1, atten);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_SetCoupling
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_SetCoupling(DSO_CouplingMode mode)
{
    DSO_Vertical_SetCouplingForChannel(DSO_CHANNEL_1, mode);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_MapY
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_MapY(const uint16_t *samples, uint32_t count, int16_t *y_out)
{
    DSO_Vertical_MapYForChannel(DSO_CHANNEL_1, samples, count, y_out);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_ApplyACDisplayFilter
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_ApplyACDisplayFilter(uint16_t *samples, uint32_t count)
{
    DSO_Vertical_ApplyACDisplayFilterForChannel(DSO_CHANNEL_1, samples, count);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_GetInfo
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Vertical_GetInfo(DSO_VerticalInfo *out)
{
    DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_1, out);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Vertical_VoltageAtPixelY
*   功能说明: 维护垂直档位、耦合、偏移和 ADC code 到屏幕 Y 坐标的映射。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_Vertical_VoltageAtPixelY(int16_t y)
{
    return DSO_Vertical_VoltageAtPixelYForChannel(DSO_CHANNEL_1, y);
}
