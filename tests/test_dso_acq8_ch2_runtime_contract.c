#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dso/dso_acq.h"
#include "dso/dso_acq_10bit.h"
#include "dso/dso_acq_8bit.h"
#include "dso/dso_sample_spec.h"

static uint8_t g_hs8_topology;
static uint32_t g_realtime_copy_recent_calls;
static uint32_t g_realtime_copy_new_calls;
static uint32_t g_request_topology_calls;
static uint8_t g_last_requested_topology;

static void switch_to_hs8(uint8_t topology)
{
    DSO_Acq_SetPendingHs8Topology(topology);
    DSO_Acq_SetSampleFormat(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    (void)DSO_Acq_ApplyPendingSampleFormat();
}

void DSO_Acq8_Init(void)
{
    g_hs8_topology = DSO_ACQ8_TOPOLOGY_DEFAULT;
}

void DSO_Acq8_Deinit(void) {}
void DSO_Acq8_Service(void) {}
void DSO_Acq8_Start(void) {}
void DSO_Acq8_Stop(void) {}
bool DSO_Acq8_IsRunning(void) { return true; }
uint32_t DSO_Acq8_GetPos(void) { return 111u; }
const volatile uint32_t *DSO_Acq8_GetBuf(void) { return (const volatile uint32_t *)0; }
uint32_t DSO_Acq8_GetCh2Pos(void) { return 222u; }
const volatile uint16_t *DSO_Acq8_GetCh2Buf(void) { return (const volatile uint16_t *)0; }
int16_t DSO_Acq8_GetCalOffset(void) { return 0; }
uint32_t DSO_Acq8_GetSampleRate(void) { return 8000000u; }
uint32_t DSO_Acq8_GetCh2SampleRate(void) { return 0u; }
uint32_t DSO_Acq8_GetAdcKernelClockHz(void) { return 50000000u; }
DSO_Acq8Topology DSO_Acq8_GetTopology(void) { return g_hs8_topology; }
bool DSO_Acq8_IsDataReady(void) { return true; }
uint8_t DSO_Acq8_GetLaneCount(void)
{
    return (g_hs8_topology == DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED) ? 3u :
           1u;
}
bool DSO_Acq8_RequestTopology(DSO_Acq8Topology topology)
{
    g_request_topology_calls++;
    g_last_requested_topology = topology;
    g_hs8_topology = topology;
    return true;
}
const DSO_AcqRuntimeInfo *DSO_Acq8_GetRuntimeInfo(void)
{
    static DSO_AcqRuntimeInfo info;
    return &info;
}
void DSO_Acq8_DMA_IRQHandler(void) {}
void DSO_Acq8_DMA2_IRQHandler(void) {}
void DSO_Acq8_CH2_DMA_IRQHandler(void) {}
void DSO_Acq8_HandleAdcHalfTransfer(ADC_TypeDef *instance) { (void)instance; }
void DSO_Acq8_HandleAdcTransferComplete(ADC_TypeDef *instance) { (void)instance; }
uint32_t DSO_Acq8_CheckOverrun(void) { return 0u; }
uint32_t DSO_Acq8_GetOverrunCount(void) { return 0u; }
uint32_t DSO_Acq8_GetPhaseSyncErrorCount(void) { return 0u; }
bool DSO_Acq8_GetDebugSnapshot(DSO_AcqDebugSnapshot *out)
{
    (void)out;
    return false;
}
uint32_t DSO_Acq8_GetTimClkHz(void) { return 200000000u; }
uint32_t DSO_Acq8_GetTimArr(void) { return 23u; }
void DSO_Acq8_SetTimArr(uint32_t new_arr) { (void)new_arr; }
uint32_t DSO_Acq8_CopyRecentSamples(uint32_t dma_pos, uint16_t *out_samples, uint32_t max_samples)
{
    (void)dma_pos;
    (void)out_samples;
    (void)max_samples;
    return 0u;
}
uint32_t DSO_Acq8_CopyNewSamples(uint32_t prev_dma_pos, uint32_t dma_pos, uint16_t *out_samples, uint32_t max_samples)
{
    (void)prev_dma_pos;
    (void)dma_pos;
    (void)out_samples;
    (void)max_samples;
    return 0u;
}
bool DSO_Acq8_IsCh2Available(void)
{
    return false;
}
uint32_t DSO_Acq8_CopyRecentCh2Samples(uint32_t dma_pos, uint16_t *out_samples, uint32_t max_samples)
{
    (void)dma_pos;
    (void)out_samples;
    (void)max_samples;
    g_realtime_copy_recent_calls++;
    return 0u;
}
uint32_t DSO_Acq8_CopyNewCh2Samples(uint32_t prev_dma_pos, uint32_t dma_pos, uint16_t *out_samples, uint32_t max_samples)
{
    (void)prev_dma_pos;
    (void)dma_pos;
    (void)out_samples;
    (void)max_samples;
    g_realtime_copy_new_calls++;
    return 0u;
}

void DSO_Acq10_Init(void) {}
void DSO_Acq10_Deinit(void) {}
void DSO_Acq10_Service(void) {}
void DSO_Acq10_Start(void) {}
void DSO_Acq10_Stop(void) {}
bool DSO_Acq10_IsRunning(void) { return true; }
bool DSO_Acq10_IsDataReady(void) { return true; }
uint32_t DSO_Acq10_GetPos(void) { return 0u; }
const volatile uint32_t *DSO_Acq10_GetBuf(void) { return (const volatile uint32_t *)0; }
uint32_t DSO_Acq10_GetCh2Pos(void) { return 0u; }
const volatile uint16_t *DSO_Acq10_GetCh2Buf(void) { return (const volatile uint16_t *)0; }
int16_t DSO_Acq10_GetCalOffset(void) { return 0; }
uint32_t DSO_Acq10_GetSampleRate(void) { return 10000000u; }
uint32_t DSO_Acq10_GetCh2SampleRate(void) { return 5000000u; }
uint32_t DSO_Acq10_GetAdcKernelClockHz(void) { return 50000000u; }
const DSO_AcqRuntimeInfo *DSO_Acq10_GetRuntimeInfo(void)
{
    static DSO_AcqRuntimeInfo info;
    return &info;
}
bool DSO_Acq10_GetDebugSnapshot(DSO_AcqDebugSnapshot *out)
{
    (void)out;
    return false;
}
uint32_t DSO_Acq10_GetTimClkHz(void) { return 200000000u; }
uint32_t DSO_Acq10_GetTimArr(void) { return 42u; }
void DSO_Acq10_SetTimArr(uint32_t new_arr) { (void)new_arr; }
uint32_t DSO_Acq10_CopyRecentSamples(uint32_t dma_pos, int16_t cal_offset, uint16_t *out_samples, uint32_t max_samples)
{
    (void)dma_pos;
    (void)cal_offset;
    (void)out_samples;
    (void)max_samples;
    return 0u;
}
uint32_t DSO_Acq10_CopyNewSamples(uint32_t prev_dma_pos, uint32_t dma_pos, int16_t cal_offset, uint16_t *out_samples, uint32_t max_samples)
{
    (void)prev_dma_pos;
    (void)dma_pos;
    (void)cal_offset;
    (void)out_samples;
    (void)max_samples;
    return 0u;
}
uint32_t DSO_Acq10_CopyRecentCh2Samples(uint32_t dma_pos, uint16_t *out_samples, uint32_t max_samples)
{
    (void)dma_pos;
    (void)out_samples;
    (void)max_samples;
    return 0u;
}
uint32_t DSO_Acq10_CopyNewCh2Samples(uint32_t prev_dma_pos, uint32_t dma_pos, uint16_t *out_samples, uint32_t max_samples)
{
    (void)prev_dma_pos;
    (void)dma_pos;
    (void)out_samples;
    (void)max_samples;
    return 0u;
}
void DSO_Acq10_DMA_IRQHandler(void) {}
void DSO_Acq10_DMA2_IRQHandler(void) {}
void DSO_Acq10_CH2_DMA_IRQHandler(void) {}
void DSO_Acq10_HandleAdcHalfTransfer(ADC_TypeDef *instance) { (void)instance; }
void DSO_Acq10_HandleAdcTransferComplete(ADC_TypeDef *instance) { (void)instance; }
uint32_t DSO_Acq10_CheckOverrun(void) { return 0u; }
uint32_t DSO_Acq10_GetOverrunCount(void) { return 0u; }
uint32_t DSO_Acq10_GetPhaseSyncErrorCount(void) { return 0u; }

static int test_hs8_dual_request_is_clamped_to_tri(void)
{
    DSO_Acq_Init();
    g_request_topology_calls = 0u;
    g_last_requested_topology = 0u;

    DSO_Acq_SetPendingHs8Topology(DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED);
    if (DSO_Acq_GetPendingHs8Topology() != DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED) {
        fprintf(stderr, "pending hs8 topology must clamp dual request to tri\n");
        return 1;
    }
    DSO_Acq_SetSampleFormat(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    (void)DSO_Acq_ApplyPendingSampleFormat();

    if (!DSO_Acq_RequestHs8Topology(DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED)) {
        fprintf(stderr, "tri-only hs8 topology request should succeed while active in hs8\n");
        return 1;
    }
    if ((g_request_topology_calls == 0u) ||
        (g_last_requested_topology != DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED) ||
        (DSO_Acq_GetHs8Topology() != DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED)) {
        fprintf(stderr,
                "dual request must reach backend as tri: calls=%lu last=%u active=%u\n",
                (unsigned long)g_request_topology_calls,
                (unsigned)g_last_requested_topology,
                (unsigned)DSO_Acq_GetHs8Topology());
        return 1;
    }

    return 0;
}

static int test_hs8_ch2_is_always_unavailable(void)
{
    uint16_t out[16] = {0};

    DSO_Acq_Init();
    switch_to_hs8(DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED);

    if (DSO_Acq_IsCh2Available()) {
        fprintf(stderr, "8-bit tri-only mode must not expose CH2\n");
        return 1;
    }
    if (DSO_Acq_GetCh2SampleRate() != 0u) {
        fprintf(stderr, "8-bit CH2 sample rate must be zero\n");
        return 1;
    }
    if (DSO_Acq_CopyRecentCh2Samples(7u, (const volatile uint16_t *)0x1, out, 16u) != 0u) {
        fprintf(stderr, "8-bit recent CH2 copy must return zero\n");
        return 1;
    }
    if (DSO_Acq_CopyNewCh2Samples(5u, 9u, (const volatile uint16_t *)0x1, out, 16u) != 0u) {
        fprintf(stderr, "8-bit new CH2 copy must return zero\n");
        return 1;
    }

    return 0;
}

static int test_10bit_ch2_remains_available(void)
{
    DSO_Acq_Init();
    DSO_Acq_SetSampleFormat(DSO_SAMPLE_FORMAT_10BIT);
    (void)DSO_Acq_ApplyPendingSampleFormat();

    if (!DSO_Acq_IsCh2Available()) {
        fprintf(stderr, "10-bit CH2 must remain available\n");
        return 1;
    }
    if (DSO_Acq_GetCh2SampleRate() != 5000000u) {
        fprintf(stderr,
                "10-bit CH2 sample rate changed unexpectedly: %lu\n",
                (unsigned long)DSO_Acq_GetCh2SampleRate());
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_hs8_dual_request_is_clamped_to_tri() != 0) {
        return 1;
    }
    if (test_hs8_ch2_is_always_unavailable() != 0) {
        return 1;
    }
    if (test_10bit_ch2_remains_available() != 0) {
        return 1;
    }

    puts("all acq8 CH2 runtime contract tests passed");
    return 0;
}
