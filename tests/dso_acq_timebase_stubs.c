#include "dso_acq_timebase_stubs.h"

#include "dso/dso_acq.h"

#include <string.h>

static uint8_t s_lane_count = 1u;
static uint32_t s_tim_clk_hz = 200000000u;
static bool s_runtime_enabled = false;
static DSO_AcqRuntimeInfo s_runtime;

void Test_AcqStub_Reset(void)
{
    s_lane_count = 1u;
    s_tim_clk_hz = 200000000u;
    s_runtime_enabled = false;
    memset(&s_runtime, 0, sizeof(s_runtime));
}

void Test_AcqStub_SetLaneCount(uint8_t lane_count)
{
    s_lane_count = lane_count;
}

void Test_AcqStub_SetTimClkHz(uint32_t tim_clk_hz)
{
    s_tim_clk_hz = tim_clk_hz;
}

void Test_AcqStub_SetRuntimeInfo(uint32_t tim_arr,
                                 uint32_t sample_rate_hz,
                                 bool enable)
{
    s_runtime_enabled = enable;
    memset(&s_runtime, 0, sizeof(s_runtime));
    s_runtime.tim8_arr = tim_arr;
    s_runtime.sample_rate_hz = sample_rate_hz;
}

const DSO_AcqRuntimeInfo *DSO_Acq_GetRuntimeInfo(void)
{
    return s_runtime_enabled ? &s_runtime : NULL;
}

uint32_t DSO_Acq_GetTimClkHz(void)
{
    return s_tim_clk_hz;
}

uint8_t DSO_Acq_GetLaneCount(void)
{
    return s_lane_count;
}
