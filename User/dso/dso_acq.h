/**
 * @file dso_acq.h
 * @brief 采集 facade：统一 start/stop/pos/buf API，分发 8-bit/10-bit 后端。
 */

#ifndef DSO_ACQ_H
/* 宏定义: DSO_ACQ_H - 头文件保护宏。 */
#define DSO_ACQ_H

#include "dso_acq_model.h"
#include "dso_config.h"
#include "dso_sample_spec.h"
#include "stm32h7xx.h"

typedef struct DSO_AcqDebugSnapshot {
    /* 当前采集格式与运行态概览。 */
    DSO_SampleFormat format;
    bool running;

    /* 8-bit 高速模式拓扑与 CH2 状态。 */
    uint8_t hs8_topology;
    uint8_t hs8_lane_count;
    bool hs8_lane2_active;
    bool ch2_valid;
    bool ch2_capture_active;

    /* 采样率、触发率和 DMA 位置换算得到的运行速率。 */
    uint32_t sample_rate_hz;
    uint32_t ch2_sample_rate_hz;
    uint32_t trigger_rate_hz;
    uint32_t accepted_trigger_div;
    uint32_t dma_sample_rate_hz;
    uint32_t dma_pos_sample_rate_hz;
    uint32_t dma_pos_rate_max_elapsed_ms;
    uint32_t dma_sample_count;
    uint32_t lane0_dma_sample_rate_hz;
    uint32_t lane1_dma_sample_rate_hz;
    uint32_t lane2_dma_sample_rate_hz;
    uint32_t lane0_dma_pos_sample_rate_hz;
    uint32_t lane1_dma_pos_sample_rate_hz;
    uint32_t lane2_dma_pos_sample_rate_hz;
    uint32_t lane0_dma_sample_count;
    uint32_t lane1_dma_sample_count;
    uint32_t lane2_dma_sample_count;

    /* 各 lane DMA 半传输/全传输计数，用于判断中断是否连续到达。 */
    uint32_t lane0_dma_half_count;
    uint32_t lane1_dma_half_count;
    uint32_t lane2_dma_half_count;
    uint32_t lane0_dma_full_count;
    uint32_t lane1_dma_full_count;
    uint32_t lane2_dma_full_count;

    /* ADC/TIM 基础运行参数和当前 DMA 写指针。 */
    uint32_t adc_kernel_clock_hz;
    uint32_t dma_pos;
    uint32_t ch2_dma_pos;
    uint32_t lane0_pos;
    uint32_t lane1_pos;
    uint32_t lane2_pos;
    uint32_t lane0_delta;
    uint32_t lane1_delta;
    uint32_t lane2_delta;
    uint32_t common_delta;
    bool phase_sync_error;
    bool data_ready;
    uint8_t data_ready_lane_count;

    /* 逻辑窗口位置：用于串口诊断查看交织合并后的样本窗口。 */
    uint32_t lane0_window_start;
    uint32_t lane1_window_start;
    uint32_t lane2_window_start;
    uint32_t lane_window_count;

    /* 物理窗口位置：用于排查 DMA 原始缓冲与逻辑窗口之间的对应关系。 */
    uint32_t lane0_physical_window_start;
    uint32_t lane1_physical_window_start;
    uint32_t lane2_physical_window_start;
    uint32_t lane_physical_window_count;
    uint32_t ch2_count;

    /* 相位同步和 overrun 累计计数。 */
    uint32_t phase_sync_errors;
    uint32_t overrun_count;
    uint32_t lane0_overrun_count;
    uint32_t lane1_overrun_count;
    uint32_t lane2_overrun_count;

    /* lane0 统计值与诊断窗口。 */
    uint16_t lane0_vmin;
    uint16_t lane0_vmax;
    uint16_t lane0_mean;
    uint16_t lane0_window[DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS];
    uint16_t lane0_physical_window[DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS];

    /* lane1 统计值与诊断窗口。 */
    uint16_t lane1_vmin;
    uint16_t lane1_vmax;
    uint16_t lane1_mean;
    uint16_t lane1_window[DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS];
    uint16_t lane1_physical_window[DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS];

    /* lane2 统计值与诊断窗口。 */
    uint16_t lane2_vmin;
    uint16_t lane2_vmax;
    uint16_t lane2_mean;
    uint16_t lane2_window[DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS];
    uint16_t lane2_physical_window[DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS];

    /* CH2 统计值；CH2 不可用时由后端按无效状态填充。 */
    uint16_t ch2_vmin;
    uint16_t ch2_vmax;
    uint16_t ch2_mean;

    /* 触发定时器和 ADC/DMA 寄存器快照，用于现场串口诊断。 */
    bool tim_enabled;
    uint32_t tim_counter;
    uint32_t tim_arr;
    uint32_t tim_ccr1;
    uint32_t tim_ccr2;
    uint32_t tim_ccr3;
    uint32_t tim_cr1;
    uint32_t tim_ccmr1;
    uint32_t tim_ccmr2;
    uint32_t tim_ccer;
    uint32_t tim_bdtr;
    uint32_t tim_sr;
    uint32_t dma_ndtr0;
    uint32_t dma_ndtr1;
    uint32_t dma_ndtr2;
    uint32_t adc1_cr;
    uint32_t adc2_cr;
    uint32_t adc3_cr;
    uint32_t adc1_cfgr;
    uint32_t adc2_cfgr;
    uint32_t adc3_cfgr;
    uint32_t adc1_isr;
    uint32_t adc2_isr;
    uint32_t adc3_isr;
} DSO_AcqDebugSnapshot;

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_Init
*   功能说明: 初始化采集 facade 和当前默认采样格式对应的采集后端。
*   形    参: 无。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_Acq_Init(void);

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_Service
*   功能说明: 周期服务当前采集后端，处理后端内部状态维护。
*   形    参: 无。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_Acq_Service(void);

/* 设置 pending 采样格式；真正切换在 DSO_Acq_ApplyPendingSampleFormat() 中完成。 */
void DSO_Acq_SetSampleFormat(DSO_SampleFormat format);

/* 读取当前已经生效的采样格式。 */
DSO_SampleFormat DSO_Acq_GetSampleFormat(void);

/* 读取等待应用的采样格式，供 UI 或诊断显示 pending 状态。 */
DSO_SampleFormat DSO_Acq_GetPendingSampleFormat(void);

/* 应用 pending 采样格式，必要时释放旧后端并初始化新后端。 */
bool DSO_Acq_ApplyPendingSampleFormat(void);

/* 读取当前采样格式对应的规格表。 */
const DSO_SampleSpec *DSO_Acq_GetSampleSpec(void);

/* 获取采集调试快照，用于串口诊断输出。 */
bool DSO_Acq_GetDebugSnapshot(DSO_AcqDebugSnapshot *out);

/* 判断当前采样格式下 CH2 是否可用。 */
bool DSO_Acq_IsCh2Available(void);

/* 读取 CH1 DMA 环形缓冲当前位置。 */
uint32_t DSO_Acq_GetPos(void);

/* 读取 CH1 DMA 原始缓冲地址。 */
const volatile uint32_t *DSO_Acq_GetBuf(void);

/* 读取 CH2 DMA 环形缓冲当前位置。 */
uint32_t DSO_Acq_GetCh2Pos(void);

/* 读取 CH2 DMA 原始缓冲地址。 */
const volatile uint16_t *DSO_Acq_GetCh2Buf(void);

/* 读取当前后端校准偏移，供样本拷贝时做 code 修正。 */
int16_t DSO_Acq_GetCalOffset(void);

/* 读取 CH1 等效采样率。 */
uint32_t DSO_Acq_GetSampleRate(void);

/* 读取 CH2 采样率；CH2 不可用时返回 0。 */
uint32_t DSO_Acq_GetCh2SampleRate(void);

/* 读取 ADC kernel clock，用于诊断采样率计算是否符合预期。 */
uint32_t DSO_Acq_GetAdcKernelClockHz(void);

/* 读取当前 CH1 lane 数量。 */
uint8_t DSO_Acq_GetLaneCount(void);

/* 读取 8-bit 高速模式拓扑；非 HS8 模式返回后端定义的无效值。 */
uint8_t DSO_Acq_GetHs8Topology(void);

/* 请求切换 8-bit 高速拓扑；当前产品路径固定三路交替。 */
bool DSO_Acq_RequestHs8Topology(uint8_t topology);

/* 读取等待应用的 8-bit 高速拓扑。 */
uint8_t DSO_Acq_GetPendingHs8Topology(void);

/* 设置等待应用的 8-bit 高速拓扑。 */
void DSO_Acq_SetPendingHs8Topology(uint8_t topology);

/* 读取采集运行时模型信息，例如芯片版本、时钟、ARR 和有效采样率。 */
const DSO_AcqRuntimeInfo *DSO_Acq_GetRuntimeInfo(void);

/* 当前后端 DMA 中断入口，由 stm32h7xx_it.c 或 HAL 回调转入。 */
void DSO_Acq_DMA_IRQHandler(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_DMA2_IRQHandler
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq_DMA2_IRQHandler(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_CH2_DMA_IRQHandler
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq_CH2_DMA_IRQHandler(void);

/* Overrun monitoring — call each frame, returns cumulative OVR count */
uint32_t DSO_Acq_CheckOverrun(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_GetOverrunCount
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq_GetOverrunCount(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_GetPhaseSyncErrorCount
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq_GetPhaseSyncErrorCount(void);

/* Timer parameters for exact frequency calculation (zero precision loss) */
uint32_t DSO_Acq_GetTimClkHz(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_GetTimArr
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq_GetTimArr(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_SetTimArr
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq_SetTimArr(uint32_t new_arr);

/* 启动、停止和查询当前采集后端运行状态。 */
void DSO_Acq_Start(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_Stop
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq_Stop(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_IsRunning
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq_IsRunning(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq_IsDataReady
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq_IsDataReady(void);

/* 拷贝最近一段 CH1 样本，供非 Roll 渲染或测量使用。 */
uint32_t DSO_Acq_CopyRecentSamples(uint32_t dma_pos,
                                   const volatile uint32_t *dma_buf,
                                   int16_t cal_offset,
                                   uint16_t *out_samples,
                                   uint32_t max_samples);

/* 从上一次 DMA 位置到当前位置拷贝 CH1 新样本，主要供 Roll/record 增量输入使用。 */
uint32_t DSO_Acq_CopyNewSamples(uint32_t prev_dma_pos,
                                uint32_t dma_pos,
                                const volatile uint32_t *dma_buf,
                                int16_t cal_offset,
                                uint16_t *out_samples,
                                uint32_t max_samples);

/* 拷贝最近一段 CH2 样本。 */
uint32_t DSO_Acq_CopyRecentCh2Samples(uint32_t dma_pos,
                                      const volatile uint16_t *dma_buf,
                                      uint16_t *out_samples,
                                      uint32_t max_samples);

/* 从上一次 DMA 位置到当前位置拷贝 CH2 新样本。 */
uint32_t DSO_Acq_CopyNewCh2Samples(uint32_t prev_dma_pos,
                                   uint32_t dma_pos,
                                   const volatile uint16_t *dma_buf,
                                   uint16_t *out_samples,
                                   uint32_t max_samples);

/**
 * DSO_AcqBackend — Function pointer table for 8-bit / 10-bit acquisition backends.
 *
 * Each backend (HS8, HS10) populates one of these at compile time.
 * The facade (dso_acq.c) selects the active backend at runtime when the
 * sample format changes, then dispatches all calls through the vtable.
 *
 * Functions NOT in the vtable (kept as explicit if/else in dso_acq.c):
 *   - CopyRecentSamples / CopyNewSamples — different signatures (cal_offset param)
 *   - CopyRecentCh2Samples / CopyNewCh2Samples — different signatures (dma_buf param)
 *   - IsCh2Available — 10-bit has special ch2_mode logic
 *   - GetLaneCount — 10-bit falls back to SampleSpec
 *   - Init / Deinit — handled by init_active_backend() / deinit_active_backend()
 */
typedef struct DSO_AcqBackend {
    /* 后端周期服务与物理启停。 */
    void (*Service)(void);
    void (*Start)(void);
    void (*Stop)(void);
    bool (*IsRunning)(void);
    bool (*IsDataReady)(void);

    /* DMA 缓冲位置、缓冲地址和采样率查询。 */
    uint32_t (*GetPos)(void);
    const volatile uint32_t *(*GetBuf)(void);
    uint32_t (*GetCh2Pos)(void);
    const volatile uint16_t *(*GetCh2Buf)(void);
    int16_t (*GetCalOffset)(void);
    uint32_t (*GetSampleRate)(void);
    uint32_t (*GetCh2SampleRate)(void);
    uint32_t (*GetAdcKernelClockHz)(void);

    /* 运行时模型与调试快照。 */
    const DSO_AcqRuntimeInfo *(*GetRuntimeInfo)(void);
    bool (*GetDebugSnapshot)(DSO_AcqDebugSnapshot *out);

    /* 采样触发定时器参数。 */
    uint32_t (*GetTimClkHz)(void);
    uint32_t (*GetTimArr)(void);
    void (*SetTimArr)(uint32_t new_arr);

    /* DMA/ADC 中断回调转发入口。 */
    void (*DMA_IRQHandler)(void);
    void (*DMA2_IRQHandler)(void);
    void (*CH2_DMA_IRQHandler)(void);
    void (*HandleAdcHalfTransfer)(ADC_TypeDef *instance);
    void (*HandleAdcTransferComplete)(ADC_TypeDef *instance);

    /* overrun 和相位同步故障统计。 */
    uint32_t (*CheckOverrun)(void);
    uint32_t (*GetOverrunCount)(void);
    uint32_t (*GetPhaseSyncErrorCount)(void);
} DSO_AcqBackend;

#endif /* DSO_ACQ_H */
