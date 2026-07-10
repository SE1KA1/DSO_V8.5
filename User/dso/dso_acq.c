/**
 * @file dso_acq.c
 * @brief 采集模式状态机、format 切换 pending、后端路由。
 */

/**
  * dso_acq.c - shared acquisition facade.
  *
  * 8-bit and 10-bit backends own their hardware details. This file only keeps
  * the public mode switch, shared format state, and uniform dispatch surface.
  *
  * Dispatch uses a vtable (DSO_AcqBackend) selected at format-change time.
  * Functions with incompatible signatures (CopyRecentSamples, CopyNewSamples,
  * CopyRecentCh2Samples, CopyNewCh2Samples) and functions with special logic
  * (IsCh2Available, GetLaneCount) remain as explicit if/else in this file.
  */

#include "dso_acq.h"

#include "dso_acq_10bit.h"
#include "dso_acq_8bit.h"
#include "dso_sample_spec.h"
#include "y_t_stress_ab.h"

/* ====== Backend vtables ====== */

static void acq10_service(void)                          { DSO_Acq10_Service(); }
static void acq10_start(void)                            { DSO_Acq10_Start(); }
static void acq10_stop(void)                             { DSO_Acq10_Stop(); }
static bool acq10_is_running(void)                       { return DSO_Acq10_IsRunning(); }
static bool acq10_is_data_ready(void)                   { return DSO_Acq10_IsDataReady(); }
static uint32_t acq10_get_pos(void)                     { return DSO_Acq10_GetPos(); }
static const volatile uint32_t *acq10_get_buf(void)     { return DSO_Acq10_GetBuf(); }
static uint32_t acq10_get_ch2_pos(void)                 { return DSO_Acq10_GetCh2Pos(); }
static const volatile uint16_t *acq10_get_ch2_buf(void) { return DSO_Acq10_GetCh2Buf(); }
static int16_t acq10_get_cal_offset(void)                { return DSO_Acq10_GetCalOffset(); }
static uint32_t acq10_get_sample_rate(void)              { return DSO_Acq10_GetSampleRate(); }
static uint32_t acq10_get_ch2_sample_rate(void)          { return DSO_Acq10_GetCh2SampleRate(); }
static uint32_t acq10_get_adc_kernel_clock_hz(void)     { return DSO_Acq10_GetAdcKernelClockHz(); }
static const DSO_AcqRuntimeInfo *acq10_get_runtime_info(void) { return DSO_Acq10_GetRuntimeInfo(); }
static bool acq10_get_debug_snapshot(DSO_AcqDebugSnapshot *out) { return DSO_Acq10_GetDebugSnapshot(out); }
static uint32_t acq10_get_tim_clk_hz(void)              { return DSO_Acq10_GetTimClkHz(); }
static uint32_t acq10_get_tim_arr(void)                 { return DSO_Acq10_GetTimArr(); }
static void acq10_set_tim_arr(uint32_t v)                { DSO_Acq10_SetTimArr(v); }
static void acq10_dma_irq_handler(void)                 { DSO_Acq10_DMA_IRQHandler(); }
static void acq10_dma2_irq_handler(void)                { DSO_Acq10_DMA2_IRQHandler(); }
static void acq10_ch2_dma_irq_handler(void)             { DSO_Acq10_CH2_DMA_IRQHandler(); }
static void acq10_handle_adc_half(ADC_TypeDef *inst)   { DSO_Acq10_HandleAdcHalfTransfer(inst); }
static void acq10_handle_adc_full(ADC_TypeDef *inst)    { DSO_Acq10_HandleAdcTransferComplete(inst); }
static uint32_t acq10_check_overrun(void)               { return DSO_Acq10_CheckOverrun(); }
static uint32_t acq10_get_overrun_count(void)            { return DSO_Acq10_GetOverrunCount(); }
static uint32_t acq10_get_phase_sync_error_count(void) { return DSO_Acq10_GetPhaseSyncErrorCount(); }

static const DSO_AcqBackend k_hs8_backend = {
    .Service                = DSO_Acq8_Service,
    .Start                  = DSO_Acq8_Start,
    .Stop                   = DSO_Acq8_Stop,
    .IsRunning              = DSO_Acq8_IsRunning,
    .IsDataReady            = DSO_Acq8_IsDataReady,
    .GetPos                 = DSO_Acq8_GetPos,
    .GetBuf                 = DSO_Acq8_GetBuf,
    .GetCh2Pos              = DSO_Acq8_GetCh2Pos,
    .GetCh2Buf              = DSO_Acq8_GetCh2Buf,
    .GetCalOffset           = DSO_Acq8_GetCalOffset,
    .GetSampleRate          = DSO_Acq8_GetSampleRate,
    .GetCh2SampleRate       = DSO_Acq8_GetCh2SampleRate,
    .GetAdcKernelClockHz   = DSO_Acq8_GetAdcKernelClockHz,
    .GetRuntimeInfo         = DSO_Acq8_GetRuntimeInfo,
    .GetDebugSnapshot       = DSO_Acq8_GetDebugSnapshot,
    .GetTimClkHz            = DSO_Acq8_GetTimClkHz,
    .GetTimArr              = DSO_Acq8_GetTimArr,
    .SetTimArr              = DSO_Acq8_SetTimArr,
    .DMA_IRQHandler         = DSO_Acq8_DMA_IRQHandler,
    .DMA2_IRQHandler        = DSO_Acq8_DMA2_IRQHandler,
    .CH2_DMA_IRQHandler     = DSO_Acq8_CH2_DMA_IRQHandler,
    .HandleAdcHalfTransfer  = DSO_Acq8_HandleAdcHalfTransfer,
    .HandleAdcTransferComplete = DSO_Acq8_HandleAdcTransferComplete,
    .CheckOverrun           = DSO_Acq8_CheckOverrun,
    .GetOverrunCount        = DSO_Acq8_GetOverrunCount,
    .GetPhaseSyncErrorCount = DSO_Acq8_GetPhaseSyncErrorCount,
};

static const DSO_AcqBackend k_hs10_backend = {
    .Service                = acq10_service,
    .Start                  = acq10_start,
    .Stop                   = acq10_stop,
    .IsRunning              = acq10_is_running,
    .IsDataReady            = acq10_is_data_ready,
    .GetPos                 = acq10_get_pos,
    .GetBuf                 = acq10_get_buf,
    .GetCh2Pos              = acq10_get_ch2_pos,
    .GetCh2Buf              = acq10_get_ch2_buf,
    .GetCalOffset           = acq10_get_cal_offset,
    .GetSampleRate          = acq10_get_sample_rate,
    .GetCh2SampleRate       = acq10_get_ch2_sample_rate,
    .GetAdcKernelClockHz   = acq10_get_adc_kernel_clock_hz,
    .GetRuntimeInfo         = acq10_get_runtime_info,
    .GetDebugSnapshot       = acq10_get_debug_snapshot,
    .GetTimClkHz            = acq10_get_tim_clk_hz,
    .GetTimArr              = acq10_get_tim_arr,
    .SetTimArr              = acq10_set_tim_arr,
    .DMA_IRQHandler         = acq10_dma_irq_handler,
    .DMA2_IRQHandler        = acq10_dma2_irq_handler,
    .CH2_DMA_IRQHandler     = acq10_ch2_dma_irq_handler,
    .HandleAdcHalfTransfer  = acq10_handle_adc_half,
    .HandleAdcTransferComplete = acq10_handle_adc_full,
    .CheckOverrun           = acq10_check_overrun,
    .GetOverrunCount        = acq10_get_overrun_count,
    .GetPhaseSyncErrorCount = acq10_get_phase_sync_error_count,
};

/* ====== Format state ====== */

static DSO_SampleFormat s_sample_format = DSO_SAMPLE_FORMAT_DEFAULT;
static DSO_SampleFormat s_pending_sample_format = DSO_SAMPLE_FORMAT_DEFAULT;
static uint8_t s_pending_hs8_topology = DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED;
static const DSO_AcqBackend *s_backend = &k_hs8_backend;

/*
*********************************************************************************************************
*   函 数 名: format_is_hs8
*   功能说明: 完成当前 UI 模块的绘制、格式化或局部状态辅助处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool format_is_hs8(DSO_SampleFormat format)
{
    return format == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED;
}

/*
*********************************************************************************************************
*   函 数 名: hs8_runtime_topology
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_Acq8Topology hs8_runtime_topology(void)
{
    return (DSO_Acq8Topology)DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED;
}

/*
*********************************************************************************************************
*   函 数 名: select_backend
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void select_backend(void)
{
    s_backend = format_is_hs8(s_sample_format) ? &k_hs8_backend : &k_hs10_backend;
}

/*
*********************************************************************************************************
*   函 数 名: init_active_backend
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void init_active_backend(void)
{
    DSO_SampleSpec_SetActive(s_sample_format);
    select_backend();

    if (format_is_hs8(s_sample_format)) {
        DSO_Acq8_Init();
        (void)DSO_Acq8_RequestTopology(hs8_runtime_topology());
    } else {
        DSO_Acq10_Init();
    }
}

/*
*********************************************************************************************************
*   函 数 名: deinit_active_backend
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void deinit_active_backend(void)
{
    if (format_is_hs8(s_sample_format)) {
        DSO_Acq8_Deinit();
    } else {
        DSO_Acq10_Deinit();
    }
}

/* ====== Public API ====== */

void DSO_Acq_Init(void)
{
    s_sample_format = DSO_SAMPLE_FORMAT_DEFAULT;
    s_pending_sample_format = s_sample_format;
    init_active_backend();

    if (DSO_Acq_GetSampleSpec()->start_immediately && !DSO_Acq_IsRunning()) {
        DSO_Acq_Start();
    }
}

void DSO_Acq_Service(void)                          { s_backend->Service(); }
void DSO_Acq_SetSampleFormat(DSO_SampleFormat fmt) { s_pending_sample_format = fmt; }
DSO_SampleFormat DSO_Acq_GetSampleFormat(void)     { return s_sample_format; }
DSO_SampleFormat DSO_Acq_GetPendingSampleFormat(void) { return s_pending_sample_format; }

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_ApplyPendingSampleFormat
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq_ApplyPendingSampleFormat(void)
{
    if (s_pending_sample_format == s_sample_format) {
        return false;
    }

    deinit_active_backend();
    s_sample_format = s_pending_sample_format;
    init_active_backend();
    return true;
}

const DSO_SampleSpec *DSO_Acq_GetSampleSpec(void)  { return DSO_SampleSpec_GetActive(); }

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_GetDebugSnapshot
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq_GetDebugSnapshot(DSO_AcqDebugSnapshot *out)
{
    if (format_is_hs8(s_sample_format)) {
        return DSO_Acq8_GetDebugSnapshot(out);
    }
    return DSO_Acq10_GetDebugSnapshot(out);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_IsCh2Available
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq_IsCh2Available(void)
{
    if (format_is_hs8(s_sample_format)) {
        return DSO_Acq8_IsCh2Available();
    }
    return DSO_SampleSpec_GetActive()->ch2_mode != DSO_CH2_MODE_DISABLED;
}

uint32_t DSO_Acq_GetPos(void)                       { return s_backend->GetPos(); }
const volatile uint32_t *DSO_Acq_GetBuf(void)      { return s_backend->GetBuf(); }
uint32_t DSO_Acq_GetCh2Pos(void)                    { return s_backend->GetCh2Pos(); }
const volatile uint16_t *DSO_Acq_GetCh2Buf(void)   { return s_backend->GetCh2Buf(); }
int16_t DSO_Acq_GetCalOffset(void)                  { return s_backend->GetCalOffset(); }
uint32_t DSO_Acq_GetSampleRate(void)               { return s_backend->GetSampleRate(); }

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_GetCh2SampleRate
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq_GetCh2SampleRate(void)
{
    if (!DSO_Acq_IsCh2Available()) {
        return 0u;
    }
    return s_backend->GetCh2SampleRate();
}

uint32_t DSO_Acq_GetAdcKernelClockHz(void)          { return s_backend->GetAdcKernelClockHz(); }

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_GetLaneCount
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t DSO_Acq_GetLaneCount(void)
{
    if (format_is_hs8(s_sample_format)) {
        return DSO_Acq8_GetLaneCount();
    }
    return DSO_SampleSpec_Get(s_sample_format)->lane_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_GetHs8Topology
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t DSO_Acq_GetHs8Topology(void)
{
    if (format_is_hs8(s_sample_format)) {
        return (uint8_t)DSO_Acq8_GetTopology();
    }
    return 0u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_RequestHs8Topology
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq_RequestHs8Topology(uint8_t topology)
{
    (void)topology;
    if (!format_is_hs8(s_sample_format)) {
        return false;
    }
    s_pending_hs8_topology = DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED;
    return DSO_Acq8_RequestTopology(hs8_runtime_topology());
}

uint8_t DSO_Acq_GetPendingHs8Topology(void)         { return s_pending_hs8_topology; }
void DSO_Acq_SetPendingHs8Topology(uint8_t t)      { (void)t; s_pending_hs8_topology = DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED; }
const DSO_AcqRuntimeInfo *DSO_Acq_GetRuntimeInfo(void) { return s_backend->GetRuntimeInfo(); }
uint32_t DSO_Acq_GetTimClkHz(void)                  { return s_backend->GetTimClkHz(); }
uint32_t DSO_Acq_GetTimArr(void)                    { return s_backend->GetTimArr(); }
void DSO_Acq_SetTimArr(uint32_t v)                  { s_backend->SetTimArr(v); }
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_Start
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq_Start(void)
{
#if (Y_T_AB_DISABLE_ACQ_START == 0)
    s_backend->Start();
#endif
}
void DSO_Acq_Stop(void)                             { s_backend->Stop(); }
bool DSO_Acq_IsRunning(void)                         { return s_backend->IsRunning(); }
bool DSO_Acq_IsDataReady(void)                      { return s_backend->IsDataReady(); }

/* Copy functions have different signatures between 8-bit and 10-bit backends,
 * so they remain as explicit if/else dispatch. */

uint32_t DSO_Acq_CopyRecentSamples(uint32_t dma_pos,
                                   const volatile uint32_t *dma_buf,
                                   int16_t cal_offset,
                                   uint16_t *out_samples,
                                   uint32_t max_samples)
{
    if (format_is_hs8(s_sample_format)) {
        (void)dma_buf;
        (void)cal_offset;
        return DSO_Acq8_CopyRecentSamples(dma_pos, out_samples, max_samples);
    }
    (void)dma_buf;
    return DSO_Acq10_CopyRecentSamples(dma_pos, cal_offset, out_samples, max_samples);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_CopyNewSamples
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq_CopyNewSamples(uint32_t prev_dma_pos,
                                uint32_t dma_pos,
                                const volatile uint32_t *dma_buf,
                                int16_t cal_offset,
                                uint16_t *out_samples,
                                uint32_t max_samples)
{
    if (format_is_hs8(s_sample_format)) {
        (void)dma_buf;
        (void)cal_offset;
        return DSO_Acq8_CopyNewSamples(prev_dma_pos, dma_pos, out_samples, max_samples);
    }
    (void)dma_buf;
    return DSO_Acq10_CopyNewSamples(prev_dma_pos, dma_pos, cal_offset, out_samples, max_samples);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_CopyRecentCh2Samples
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq_CopyRecentCh2Samples(uint32_t dma_pos,
                                      const volatile uint16_t *dma_buf,
                                      uint16_t *out_samples,
                                      uint32_t max_samples)
{
    if (format_is_hs8(s_sample_format)) {
        (void)dma_buf;
        return DSO_Acq8_CopyRecentCh2Samples(dma_pos, out_samples, max_samples);
    }
    (void)dma_buf;
    return DSO_Acq10_CopyRecentCh2Samples(dma_pos, out_samples, max_samples);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_CopyNewCh2Samples
*   功能说明: 在采集 facade 中完成采样格式判断、后端选择、状态查询或转发调用。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq_CopyNewCh2Samples(uint32_t prev_dma_pos,
                                   uint32_t dma_pos,
                                   const volatile uint16_t *dma_buf,
                                   uint16_t *out_samples,
                                   uint32_t max_samples)
{
    if (format_is_hs8(s_sample_format)) {
        (void)dma_buf;
        return DSO_Acq8_CopyNewCh2Samples(prev_dma_pos, dma_pos, out_samples, max_samples);
    }
    (void)dma_buf;
    return DSO_Acq10_CopyNewCh2Samples(prev_dma_pos, dma_pos, out_samples, max_samples);
}

/* IRQ handlers — on the hot path, vtable dispatch is acceptable
 * (~2 cycle overhead on Cortex-M7 at 400 MHz). */

void DSO_Acq_DMA_IRQHandler(void)                     { s_backend->DMA_IRQHandler(); }
void DSO_Acq_DMA2_IRQHandler(void)                   { s_backend->DMA2_IRQHandler(); }
void DSO_Acq_CH2_DMA_IRQHandler(void)                { s_backend->CH2_DMA_IRQHandler(); }
void DSO_Acq_HandleAdcHalfTransfer(ADC_TypeDef *inst) { s_backend->HandleAdcHalfTransfer(inst); }
void DSO_Acq_HandleAdcTransferComplete(ADC_TypeDef *inst) { s_backend->HandleAdcTransferComplete(inst); }
uint32_t DSO_Acq_CheckOverrun(void)                   { return s_backend->CheckOverrun(); }
uint32_t DSO_Acq_GetOverrunCount(void)                { return s_backend->GetOverrunCount(); }
uint32_t DSO_Acq_GetPhaseSyncErrorCount(void)         { return s_backend->GetPhaseSyncErrorCount(); }
