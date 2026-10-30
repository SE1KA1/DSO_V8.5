/**
 * @file dso_config.h
 * @brief DSO 全局编译配置：ADC 参数、显示尺寸、调试开关、默认档位等。
 */

#ifndef DSO_CONFIG_H
/* 宏定义: DSO_CONFIG_H - 头文件保护宏。 */
#define DSO_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/* Hardware clocks */
#define DSO_TIM8_CLK_HZ        200000000u

/* ADC */
/*
 * DSO_ADC_MAX is a legacy compatibility constant for old code paths and tests.
 * Active acquisition formats must use DSO_SampleSpec.adc_max or DSO_Frame.adc_lsb_v.
 */
#define DSO_ADC_MAX            4095u
/* 宏定义: DSO_VREF - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_VREF               3.3f

/*
 * Dual interleaved acquisition is now timer-triggered only. This keeps the
 * trigger rate, lane rate and total sample rate deterministic from TIM8 ARR.
 */
#ifndef DSO_ACQ_CONTINUOUS
/* 宏定义: DSO_ACQ_CONTINUOUS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ACQ_CONTINUOUS     0u
#endif

/*
 * Throughput ladder:
 *   HS11:     ARR=36 -> period=37 TIM8 clocks -> 10.81 MSPS total (overdrives 12-bit interleaved timing)
 *   HS10:     ARR=39 -> period=40 TIM8 clocks -> 10.00 MSPS total (overdrives 12-bit interleaved timing)
 *   STABLE93: ARR=42 -> period=43 TIM8 clocks ->  9.30 MSPS total (12-bit, DELAY=1 safe point)
 *   SAFE9:    ARR=43 -> period=44 TIM8 clocks ->  9.09 MSPS total
 */
#define DSO_DUAL_SWEEP_PROFILE_HS11      1u
/* 宏定义: DSO_DUAL_SWEEP_PROFILE_HS10 - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DUAL_SWEEP_PROFILE_HS10      2u
/* 宏定义: DSO_DUAL_SWEEP_PROFILE_STABLE93 - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DUAL_SWEEP_PROFILE_STABLE93  3u
/* 宏定义: DSO_DUAL_SWEEP_PROFILE_SAFE9 - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DUAL_SWEEP_PROFILE_SAFE9     4u

/* 宏定义: DSO_DUAL_SWEEP_PROFILE_HS11_TIM8_ARR - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_DUAL_SWEEP_PROFILE_HS11_TIM8_ARR   36u
/* 宏定义: DSO_DUAL_SWEEP_PROFILE_HS10_TIM8_ARR - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_DUAL_SWEEP_PROFILE_HS10_TIM8_ARR   39u
/* 宏定义: DSO_DUAL_SWEEP_PROFILE_STABLE93_TIM8_ARR - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_DUAL_SWEEP_PROFILE_STABLE93_TIM8_ARR 42u
/* 宏定义: DSO_DUAL_SWEEP_PROFILE_SAFE9_TIM8_ARR - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_DUAL_SWEEP_PROFILE_SAFE9_TIM8_ARR  43u

#ifndef DSO_DUAL_SWEEP_PROFILE
/* 宏定义: DSO_DUAL_SWEEP_PROFILE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DUAL_SWEEP_PROFILE DSO_DUAL_SWEEP_PROFILE_STABLE93
#endif

#if (DSO_DUAL_SWEEP_PROFILE == DSO_DUAL_SWEEP_PROFILE_HS11)
/* 宏定义: DSO_DUAL_SWEEP_PROFILE_NAME - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DUAL_SWEEP_PROFILE_NAME "dual_hs11_d1"
/* 宏定义: DSO_TIM8_TRIGGER_ARR - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_TIM8_TRIGGER_ARR        DSO_DUAL_SWEEP_PROFILE_HS11_TIM8_ARR
#elif (DSO_DUAL_SWEEP_PROFILE == DSO_DUAL_SWEEP_PROFILE_HS10)
/* 宏定义: DSO_DUAL_SWEEP_PROFILE_NAME - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DUAL_SWEEP_PROFILE_NAME "dual_hs10_d1"
/* 宏定义: DSO_TIM8_TRIGGER_ARR - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_TIM8_TRIGGER_ARR        DSO_DUAL_SWEEP_PROFILE_HS10_TIM8_ARR
#elif (DSO_DUAL_SWEEP_PROFILE == DSO_DUAL_SWEEP_PROFILE_STABLE93)
/* 宏定义: DSO_DUAL_SWEEP_PROFILE_NAME - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DUAL_SWEEP_PROFILE_NAME "dual_stable93_d1"
/* 宏定义: DSO_TIM8_TRIGGER_ARR - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_TIM8_TRIGGER_ARR        DSO_DUAL_SWEEP_PROFILE_STABLE93_TIM8_ARR
#elif (DSO_DUAL_SWEEP_PROFILE == DSO_DUAL_SWEEP_PROFILE_SAFE9)
/* 宏定义: DSO_DUAL_SWEEP_PROFILE_NAME - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DUAL_SWEEP_PROFILE_NAME "dual_safe9_d1"
/* 宏定义: DSO_TIM8_TRIGGER_ARR - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_TIM8_TRIGGER_ARR        DSO_DUAL_SWEEP_PROFILE_SAFE9_TIM8_ARR
#else
#error "Unsupported DSO_DUAL_SWEEP_PROFILE"
#endif

/* 宏定义: DSO_ADC_MASTER_DMNGT_DMA_CIRCULAR - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ADC_MASTER_DMNGT_DMA_CIRCULAR 1u
/* 宏定义: DSO_ADC_MASTER_DMNGT_DMA_ONESHOT - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ADC_MASTER_DMNGT_DMA_ONESHOT  2u

#ifndef DSO_ADC_MASTER_DMNGT_MODE
/* 宏定义: DSO_ADC_MASTER_DMNGT_MODE - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ADC_MASTER_DMNGT_MODE   DSO_ADC_MASTER_DMNGT_DMA_CIRCULAR
#endif

/* Independent-lane acquisition target: 75 MHz effective ADC core clock, DELAY=1. */
#define DSO_ADC_CORE_TARGET_HZ       75000000u
/* 宏定义: DSO_ADC_INPUT_CLK_DEFAULT_HZ - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ADC_INPUT_CLK_DEFAULT_HZ 75000000u
/* 宏定义: DSO_ADC_INPUT_CLK_REVV_HZ - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ADC_INPUT_CLK_REVV_HZ   150000000u

/* Compatibility alias: logical "working ADC clock" target. */
#define DSO_ADC_CLK_HZ               DSO_ADC_CORE_TARGET_HZ

/*
 * VOFA+ FireWater：周期性把整屏 frame->y_coords[] 打成「整数,整数,...\\n」从 COM1 发出。
 * 默认关闭：否则串口助手会像「全是数字」，且淹没 DSO_ROLL_SERIAL_DIAG_ENABLE 的 [DSO] 文本。
 * 需要 VoFA 时在工程选项里 -DDSO_VOFA_FIREWATER_ENABLE=1 或改此处为 1u。
 */
#ifndef DSO_VOFA_FIREWATER_ENABLE
/* 宏定义: DSO_VOFA_FIREWATER_ENABLE - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_VOFA_FIREWATER_ENABLE      0u
#endif

#ifndef DSO_VOFA_FIREWATER_INTERVAL_MS
/* 宏定义: DSO_VOFA_FIREWATER_INTERVAL_MS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_VOFA_FIREWATER_INTERVAL_MS 100u
#endif

#ifndef DSO_VOFA_FIREWATER_UART_BAUD
/* 宏定义: DSO_VOFA_FIREWATER_UART_BAUD - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_VOFA_FIREWATER_UART_BAUD   1152000u
#endif

#ifndef DSO_VOFA_FIREWATER_SILENCE_PRINTF
/* 宏定义: DSO_VOFA_FIREWATER_SILENCE_PRINTF - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_VOFA_FIREWATER_SILENCE_PRINTF 1u
#endif

#ifndef DSO_VOFA_FIREWATER_CHUNK_BYTES
/* 宏定义: DSO_VOFA_FIREWATER_CHUNK_BYTES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_VOFA_FIREWATER_CHUNK_BYTES 1024u
#endif

/* Structured UART diagnostics for ACQ/PROC/UI analysis (frame/snapshot text on COM1). */
#ifndef DSO_SERIAL_DEBUG_ENABLE
/* 宏定义: DSO_SERIAL_DEBUG_ENABLE - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_SERIAL_DEBUG_ENABLE      0u
#endif

/* main.c / MainTask / dma2d 等信息类 printf；1=打开 [BOOT]/[APP] 等；默认关闭减少串口占用 */
#ifndef DSO_APP_INFO_PRINTF_ENABLE
/* 宏定义: DSO_APP_INFO_PRINTF_ENABLE - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_APP_INFO_PRINTF_ENABLE   0u
#endif

#ifndef DSO_SERIAL_DEBUG_INTERVAL_MS
/* 宏定义: DSO_SERIAL_DEBUG_INTERVAL_MS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_SERIAL_DEBUG_INTERVAL_MS 500u
#endif

#ifndef DSO_SERIAL_DEBUG_CH2_ENABLE
/* 宏定义: DSO_SERIAL_DEBUG_CH2_ENABLE - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_SERIAL_DEBUG_CH2_ENABLE  0u
#endif

#ifndef DSO_SERIAL_DEBUG_SNAPSHOT_POINTS
/* 宏定义: DSO_SERIAL_DEBUG_SNAPSHOT_POINTS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SERIAL_DEBUG_SNAPSHOT_POINTS 48u
#endif

#ifndef DSO_SERIAL_DEBUG_SNAPSHOT_INTERVAL_MS
/* 宏定义: DSO_SERIAL_DEBUG_SNAPSHOT_INTERVAL_MS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_SERIAL_DEBUG_SNAPSHOT_INTERVAL_MS 1500u
#endif

#ifndef DSO_SERIAL_DEBUG_RAW_DUMP_ENABLE
/* 宏定义: DSO_SERIAL_DEBUG_RAW_DUMP_ENABLE - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_SERIAL_DEBUG_RAW_DUMP_ENABLE 0u
#endif

#ifndef DSO_SERIAL_DEBUG_PHASE_ONLY
/* 宏定义: DSO_SERIAL_DEBUG_PHASE_ONLY - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SERIAL_DEBUG_PHASE_ONLY 0u
#endif

#ifndef DSO_SERIAL_DEBUG_PHASE_STATS_WINDOW
/* 宏定义: DSO_SERIAL_DEBUG_PHASE_STATS_WINDOW - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SERIAL_DEBUG_PHASE_STATS_WINDOW 8u
#endif

#ifndef DSO_SERIAL_DEBUG_RAW_DUMP_INTERVAL_MS
/* 宏定义: DSO_SERIAL_DEBUG_RAW_DUMP_INTERVAL_MS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_SERIAL_DEBUG_RAW_DUMP_INTERVAL_MS 1000u
#endif

#ifndef DSO_SERIAL_DEBUG_HS8_STAGE
/* 0=legacy HS8/HS8R, 1=HS8D summary, 2+=HS8D + HS8W windows */
#define DSO_SERIAL_DEBUG_HS8_STAGE 1u
#endif

#ifndef DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS
/* 宏定义: DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS 18u
#endif

/* Narrow Vpp investigation output on COM1. Keep this independent from the
 * full serial-debug stream so bench captures show only the values needed. */
#ifndef DSO_VPP_DIAG_ENABLE
/* 宏定义: DSO_VPP_DIAG_ENABLE - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_VPP_DIAG_ENABLE 1u
#endif

#ifndef DSO_VPP_DIAG_INTERVAL_MS
/* 宏定义: DSO_VPP_DIAG_INTERVAL_MS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_VPP_DIAG_INTERVAL_MS 500u
#endif

#ifndef DSO_MEASURE_AMP_SCALE
/* 宏定义: DSO_MEASURE_AMP_SCALE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_MEASURE_AMP_SCALE 0.680f
#endif

#ifndef DSO_HF_MEASURE_AMP_SCALE
/* 宏定义: DSO_HF_MEASURE_AMP_SCALE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HF_MEASURE_AMP_SCALE 0.600f
#endif

#ifndef DSO_HF_NORMAL_MEASURE_AMP_SCALE
/* 宏定义: DSO_HF_NORMAL_MEASURE_AMP_SCALE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HF_NORMAL_MEASURE_AMP_SCALE 0.560f
#endif

#ifndef DSO_VPP_DIAG_AMP_SCALE
/* 宏定义: DSO_VPP_DIAG_AMP_SCALE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_VPP_DIAG_AMP_SCALE DSO_MEASURE_AMP_SCALE
#endif

#ifndef DSO_FREQ_METER_SERIAL_DEBUG_ENABLE
/* 宏定义: DSO_FREQ_METER_SERIAL_DEBUG_ENABLE - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_FREQ_METER_SERIAL_DEBUG_ENABLE 0u
#endif

#ifndef DSO_FREQ_METER_SERIAL_DEBUG_ONLY
/* 宏定义: DSO_FREQ_METER_SERIAL_DEBUG_ONLY - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_FREQ_METER_SERIAL_DEBUG_ONLY   0u
#endif

#ifndef DSO_FREQ_METER_SERIAL_DEBUG_INTERVAL_MS
/* 宏定义: DSO_FREQ_METER_SERIAL_DEBUG_INTERVAL_MS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_FREQ_METER_SERIAL_DEBUG_INTERVAL_MS 100u
#endif

/* 宏定义: DSO_ACQ8_TOPOLOGY_SINGLE_SAFE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ACQ8_TOPOLOGY_SINGLE_SAFE       1u
/* 宏定义: DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED  2u
/* 宏定义: DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED   3u

#ifndef DSO_ACQ8_SINGLE_SAFE_SAMPLE_RATE_HZ
/* 宏定义: DSO_ACQ8_SINGLE_SAFE_SAMPLE_RATE_HZ - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ACQ8_SINGLE_SAFE_SAMPLE_RATE_HZ 3333333u
#endif

#ifndef DSO_ACQ8_TRI_INTERLEAVED_TIM_PERIOD_CLKS
/* 宏定义: DSO_ACQ8_TRI_INTERLEAVED_TIM_PERIOD_CLKS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_ACQ8_TRI_INTERLEAVED_TIM_PERIOD_CLKS 18u
#endif

#ifndef DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR
/* 宏定义: DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR (DSO_ACQ8_TRI_INTERLEAVED_TIM_PERIOD_CLKS - 1u)
#endif

#ifndef DSO_ACQ8_TRI_INTERLEAVED_LANE_RATE_HZ
/* 宏定义: DSO_ACQ8_TRI_INTERLEAVED_LANE_RATE_HZ - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ACQ8_TRI_INTERLEAVED_LANE_RATE_HZ (DSO_TIM8_CLK_HZ / DSO_ACQ8_TRI_INTERLEAVED_TIM_PERIOD_CLKS)
#endif

#ifndef DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ
/* 宏定义: DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ (3u * DSO_ACQ8_TRI_INTERLEAVED_LANE_RATE_HZ)
#endif

#ifndef DSO_ACQ8_TOPOLOGY_DEFAULT
/* 宏定义: DSO_ACQ8_TOPOLOGY_DEFAULT - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ACQ8_TOPOLOGY_DEFAULT DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED
#endif

#ifndef DSO_SERIAL_DEBUG_FORCE_SNAPSHOT
/* 宏定义: DSO_SERIAL_DEBUG_FORCE_SNAPSHOT - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SERIAL_DEBUG_FORCE_SNAPSHOT 0u
#endif

#ifndef DSO_SERIAL_DEBUG_ALLOW_VOFA
/* 宏定义: DSO_SERIAL_DEBUG_ALLOW_VOFA - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SERIAL_DEBUG_ALLOW_VOFA  0u
#endif

#ifndef DSO_SERIAL_DEBUG_UART_BAUD
/* 宏定义: DSO_SERIAL_DEBUG_UART_BAUD - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SERIAL_DEBUG_UART_BAUD   1152000u
#endif

#ifndef DSO_SERIAL_DEBUG_LINE_BUF_SIZE
/* 宏定义: DSO_SERIAL_DEBUG_LINE_BUF_SIZE - 计数、阈值、容量或深度配置常量。 */
#define DSO_SERIAL_DEBUG_LINE_BUF_SIZE 1024u
#endif

/* Bring-up switch: 1 = ADC1 only, 0 = dual ADC1+ADC2 interleaved */
#ifdef DSO_SINGLE_ADC_TEST
#if (DSO_SINGLE_ADC_TEST != 0u)
#error "Dual interleaved sweep requires DSO_SINGLE_ADC_TEST=0"
#endif
#else
/* 宏定义: DSO_SINGLE_ADC_TEST - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_SINGLE_ADC_TEST         0u
#endif

/* ADC sampling time in x10 cycles: 15 => 1.5 cycles */
#ifndef DSO_ADC_SAMPLE_CYCLES_X10
/* 宏定义: DSO_ADC_SAMPLE_CYCLES_X10 - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ADC_SAMPLE_CYCLES_X10   15u
#endif

/* Keep the slave delay fixed at 1 cycle for the whole throughput ladder. */
#define DSO_ADC_DELAY_CYCLES        1u

/* Continuous-mode throughput debugging: bypass ETS so high-frequency frames stay on raw/direct path */
#ifndef DSO_DISABLE_ETS_FOR_SWEEP
/* 宏定义: DSO_DISABLE_ETS_FOR_SWEEP - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_DISABLE_ETS_FOR_SWEEP   1u
#endif

/* ---- Stage 2C: DEV software P-256 backend (mbedTLS) ----
 * 1 = 授权命令（get_pubkey / auth_start / auth_verify）走**软件**后端，ATECC 不参与。
 * 目的：ATECC 未锁定、数据区不可用，先用公开测试密钥把整条授权链在真机跑通。
 * ⚠️ DEV ONLY：私钥是公开测试密钥（00..1F），**禁止进入量产固件**。
 * 0 = 保持原样（ATECC 路径）。 */
#ifndef DSO_AUTH_DEV_SOFTKEY
#define DSO_AUTH_DEV_SOFTKEY               1u
#endif

/* ---- Rack control-plane test build (TEST ONLY) ----
 * 1 = rack test build:
 *     - skip the legacy FMC telemetry client (its EWOULDBLOCK handling causes a
 *       50 ms connect/close storm and consumes a LwIP netconn/TCP PCB);
 *     - skip the UDP-5006 throughput test receiver (frees one netconn);
 *     - raise the ETH_Up task priority from 2 to 4 so the single-client 5005
 *       listener is not starved by the priority-3 telemetry tasks.
 * 0 = normal firmware. Set back to 0 and rebuild after the rack test.
 * Module telemetry and the watchdog are unaffected. */
#ifndef DSO_RACK_TEST_BUILD
#define DSO_RACK_TEST_BUILD                0
#endif

/* Disable internal 100 kHz DAC test source when chasing external high-speed input */
#ifndef DSO_TESTGEN_ENABLE
/* 宏定义: DSO_TESTGEN_ENABLE - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_TESTGEN_ENABLE          0u
#endif

/* DAC waveform generator: PA4 / DAC1_CH1 / TIM5_TRGO / DMA1_Stream5. */
#ifndef DSO_WAVEGEN_ENABLE
/* 宏定义: DSO_WAVEGEN_ENABLE - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_WAVEGEN_ENABLE                 1u
#endif

/* 宏定义: DSO_WAVEGEN_PATH_DAC_PA4 - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PATH_DAC_PA4           0u
/* 宏定义: DSO_WAVEGEN_PATH_PWM_TIM5 - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_WAVEGEN_PATH_PWM_TIM5          1u

/* 宏定义: DSO_WAVEGEN_WAVE_SINE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_WAVE_SINE              0u
/* 宏定义: DSO_WAVEGEN_WAVE_TRIANGLE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_WAVE_TRIANGLE          1u
/* 宏定义: DSO_WAVEGEN_WAVE_SQUARE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_WAVE_SQUARE            2u

/* 宏定义: DSO_WAVEGEN_PROFILE_SINE_32_468K - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_SINE_32_468K       0u
/* 宏定义: DSO_WAVEGEN_PROFILE_SINE_16_937K - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_SINE_16_937K       1u
/* 宏定义: DSO_WAVEGEN_PROFILE_SINE_10_1500K - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_SINE_10_1500K      2u
/* 宏定义: DSO_WAVEGEN_PROFILE_TRIANGLE_20_750K - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_TRIANGLE_20_750K   3u
/* 宏定义: DSO_WAVEGEN_PROFILE_TRIANGLE_32_468K - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_TRIANGLE_32_468K   4u
/* 宏定义: DSO_WAVEGEN_PROFILE_SQUARE_2_7500K - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_SQUARE_2_7500K     5u
/* 宏定义: DSO_WAVEGEN_PROFILE_BRINGUP_SQUARE_1K - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_BRINGUP_SQUARE_1K  6u
/* 宏定义: DSO_WAVEGEN_PROFILE_SQUARE_100K - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_SQUARE_100K        7u
/* 宏定义: DSO_WAVEGEN_PROFILE_SQUARE_500K - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_SQUARE_500K        8u
/* 宏定义: DSO_WAVEGEN_PROFILE_SQUARE_1M - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_SQUARE_1M          9u
/* 宏定义: DSO_WAVEGEN_PROFILE_SQUARE_2M - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_SQUARE_2M          10u
/* 宏定义: DSO_WAVEGEN_PROFILE_SQUARE_3M - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_SQUARE_3M          11u
/* 宏定义: DSO_WAVEGEN_PROFILE_SQUARE_3500K - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_SQUARE_3500K       12u
/* 宏定义: DSO_WAVEGEN_PROFILE_SQUARE_250K - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE_SQUARE_250K        13u

#ifndef DSO_WAVEGEN_PATH
/* 宏定义: DSO_WAVEGEN_PATH - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PATH                   DSO_WAVEGEN_PATH_DAC_PA4
#endif

#ifndef DSO_WAVEGEN_PROFILE
/* 宏定义: DSO_WAVEGEN_PROFILE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_PROFILE                DSO_WAVEGEN_PROFILE_SQUARE_250K
#endif

#ifndef DSO_WAVEGEN_DAC_UPDATE_BUDGET_HZ
/* 宏定义: DSO_WAVEGEN_DAC_UPDATE_BUDGET_HZ - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_DAC_UPDATE_BUDGET_HZ   15000000u
#endif

#ifndef DSO_WAVEGEN_SQUARE_DUTY_PERMILLE
/* 宏定义: DSO_WAVEGEN_SQUARE_DUTY_PERMILLE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_SQUARE_DUTY_PERMILLE   500u
#endif

#ifndef DSO_WAVEGEN_VREF_MV
/* 宏定义: DSO_WAVEGEN_VREF_MV - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_VREF_MV                3287u
#endif

#ifndef DSO_WAVEGEN_POLL_MS
/* 宏定义: DSO_WAVEGEN_POLL_MS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_WAVEGEN_POLL_MS                500u
#endif

#ifndef DSO_WAVEGEN_HOST_SYNTAX
/* 宏定义: DSO_WAVEGEN_HOST_SYNTAX - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_HOST_SYNTAX            0u
#endif

#if (DSO_WAVEGEN_ENABLE != 0u) && (DSO_TESTGEN_ENABLE != 0u)
#error "DSO_WAVEGEN_ENABLE and DSO_TESTGEN_ENABLE both use PA4/DAC1/DMA1_Stream5"
#endif

#if (DSO_WAVEGEN_PATH != DSO_WAVEGEN_PATH_DAC_PA4) && \
    (DSO_WAVEGEN_PATH != DSO_WAVEGEN_PATH_PWM_TIM5)
#error "DSO_WAVEGEN_PATH must be DAC_PA4 or PWM_TIM5"
#endif

#if DSO_WAVEGEN_PATH == DSO_WAVEGEN_PATH_PWM_TIM5
#error "DSO_WAVEGEN_PATH_PWM_TIM5 is reserved; current migration supports DAC_PA4 only"
#endif

#if (DSO_WAVEGEN_PROFILE > DSO_WAVEGEN_PROFILE_SQUARE_250K)
#error "DSO_WAVEGEN_PROFILE is out of range"
#endif

#if (DSO_WAVEGEN_SQUARE_DUTY_PERMILLE == 0u) || \
    (DSO_WAVEGEN_SQUARE_DUTY_PERMILLE >= 1000u)
#error "DSO_WAVEGEN_SQUARE_DUTY_PERMILLE must be in 1..999"
#endif

/* SAR conversion cycles per resolution (STM32H743, regular ADC) */
#define DSO_ADC_SAR_CYCLES_X10_16BIT  85u
/* 宏定义: DSO_ADC_SAR_CYCLES_X10_14BIT - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ADC_SAR_CYCLES_X10_14BIT  75u
/* 宏定义: DSO_ADC_SAR_CYCLES_X10_12BIT - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ADC_SAR_CYCLES_X10_12BIT  65u
/* 宏定义: DSO_ADC_SAR_CYCLES_X10_10BIT - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ADC_SAR_CYCLES_X10_10BIT  55u
/* 宏定义: DSO_ADC_SAR_CYCLES_X10_8BIT - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ADC_SAR_CYCLES_X10_8BIT   45u

/* 宏定义: DSO_ADC_CONVERSION_CYCLES_X10 - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ADC_CONVERSION_CYCLES_X10  (DSO_ADC_SAMPLE_CYCLES_X10 + DSO_ADC_SAR_CYCLES_X10_12BIT)

/*
 * Timer-triggered mode: the displayed sample rate must follow TIM8, not a
 * nominal ADC throughput assumption.
 */
#if DSO_SINGLE_ADC_TEST
/* 宏定义: DSO_SAMPLE_RATE_NOMINAL - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SAMPLE_RATE_NOMINAL  (DSO_TIM8_CLK_HZ / (DSO_TIM8_TRIGGER_ARR + 1u))
#else
/* 宏定义: DSO_SAMPLE_RATE_NOMINAL - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SAMPLE_RATE_NOMINAL  (2u * DSO_TIM8_CLK_HZ / (DSO_TIM8_TRIGGER_ARR + 1u))
#endif

/*
 * 通用运行时 printf（Engine 启动行、前几帧 dump、testgen 等）：默认关闭。
 * Roll / gentle 诊断单独由 DSO_ROLL_SERIAL_DIAG_ENABLE 控制（见下）。
 */
#define DSO_RUNTIME_LOG_ENABLE         0u
/* 宏定义: DSO_DEBUG_PIPELINE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DEBUG_PIPELINE             0u
/* 宏定义: DSO_DEBUG_BOOT_WORDS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DEBUG_BOOT_WORDS             30u
/* 宏定义: DSO_DEBUG_BOOT_SEQ_WORDS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DEBUG_BOOT_SEQ_WORDS         40u
/* 宏定义: DSO_DEBUG_ZC_WORDS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DEBUG_ZC_WORDS               4096u
#define DSO_DEBUG_FRAME_LIMIT            0u   /* 0 = unlimited diagnostic frames */
#define DSO_DEBUG_DMA_RATE_INTERVAL_MS   1u
/* 宏定义: DSO_DEBUG_DMA_RATE_SAMPLES - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_DEBUG_DMA_RATE_SAMPLES       8u
/* 宏定义: DSO_DEBUG_DIAG_WORD_PREVIEW - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DEBUG_DIAG_WORD_PREVIEW      64u
/* 宏定义: DSO_DEBUG_DIAG_SAMPLE_PREVIEW - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DEBUG_DIAG_SAMPLE_PREVIEW    24u
/* 宏定义: DSO_DEBUG_DIAG_CROSS_PREVIEW - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_DEBUG_DIAG_CROSS_PREVIEW     16u

/*
 * LTDC 显示保护：仅 FAST/Y-T（sec/div 不超过本阈值）暂停后台 record/roll-history 写入。
 * NORMAL 50 us..2 ms 仍走 record+DMA 刷新，不可与 FAST 共用同一保护策略。
 */
#ifndef DSO_ENGINE_DISPLAY_PROTECT_MAX_SEC_DIV_NS
/* 宏定义: DSO_ENGINE_DISPLAY_PROTECT_MAX_SEC_DIV_NS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ENGINE_DISPLAY_PROTECT_MAX_SEC_DIV_NS  2000000u
#endif
#ifndef DSO_ENGINE_HIGH_BW_LIVE_RECORD_ENABLE
/* 宏定义: DSO_ENGINE_HIGH_BW_LIVE_RECORD_ENABLE - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_ENGINE_HIGH_BW_LIVE_RECORD_ENABLE      0u
#endif
#ifndef DSO_ENGINE_HIGH_BW_ROLL_PREFEED_ENABLE
/* 宏定义: DSO_ENGINE_HIGH_BW_ROLL_PREFEED_ENABLE - 编译期开关，用于启用或关闭对应功能。 */
#define DSO_ENGINE_HIGH_BW_ROLL_PREFEED_ENABLE     0u
#endif

/* Engine 向 UI 发布 DSO_Frame 的最小间隔（ms）；25 = 40 Hz，与 MAINTASK_FRAME_PERIOD_MS 对齐。 */
#ifndef DSO_ENGINE_FRAME_INTERVAL_MS
/* 宏定义: DSO_ENGINE_FRAME_INTERVAL_MS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_ENGINE_FRAME_INTERVAL_MS               25u
#endif

/*
 * Roll 串口诊断（COM1，与 printf 同一物理口）：[DSO] roll … / gentle roll …
 * 波特率：默认 = BSP UART1_BAUD（bsp_uart_fifo.h，通常 115200）；仅当 DSO_VOFA_FIREWATER_ENABLE
 * 为 1 时 engine 会对 COM1 再调 comSetBaud（DSO_VOFA_FIREWATER_UART_BAUD 等）。
 * 看文本诊断时请保持 VoFA 关闭，避免 CSV 波形流占满串口。量产可改为 0。
 */
#define DSO_ROLL_SERIAL_DIAG_ENABLE    1u

/* DMA ring buffer */
#define DSO_DMA_BUF_SIZE        10240u

/* Processing */
#define DSO_UNPACK_SIZE         4096u
/* 宏定义: DSO_FFT_SIZE - 计数、阈值、容量或深度配置常量。 */
#define DSO_FFT_SIZE            2048u

/* Display（须与 MainTask 水平布局一致：24+WIDTH+8+156+4=800；与 dma2d_wave.h / LCDConf 旋转区一致） */
#define DSO_DISPLAY_WIDTH       608u
/* 须与 dma2d_wave.h PLOT_BUF_W、LCDConf ROTATE_PLOT_H/PHYS_PLOT_W 一致（逻辑 plot 垂直像素） */
#define DSO_DISPLAY_HEIGHT      248u

/* Gentle Roll ring resync: max raw samples fed per engine frame (CH1+CH2 split ~50/50). */
#define DSO_ENGINE_ROLL_RESYNC_MAX_SAMPLES_PER_FRAME  8192u
/* First-frame approximate roll line from ring (total CopySpan reads budget, spread across columns). */
#define DSO_ROLL_QUICK_SEED_MAX_RAW_SAMPLES             4096u
/* 宏定义: DSO_PIXEL_MARGIN - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_PIXEL_MARGIN        8u
/* 宏定义: DSO_VOFA_FIREWATER_LINE_BUF_SIZE - 计数、阈值、容量或深度配置常量。 */
#define DSO_VOFA_FIREWATER_LINE_BUF_SIZE ((DSO_DISPLAY_WIDTH * 8u) + 2u)

/* Trigger */
#define DSO_TRIG_VPP_MIN        80u
/* 宏定义: DSO_TRIG_HYST_MIN - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_TRIG_HYST_MIN       20u

/* ETS (Equivalent Time Sampling) */
#define DSO_ETS_PHASE_BINS      512u
/* 宏定义: DSO_ETS_SPC_THRESH - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ETS_SPC_THRESH      8.0f
/* 宏定义: DSO_ETS_COVERAGE_READY - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ETS_COVERAGE_READY  0.85f
/* 宏定义: DSO_ETS_FREQ_RESET_TOL - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ETS_FREQ_RESET_TOL  0.10f
/* 宏定义: DSO_ETS_TARGET_CYCLES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ETS_TARGET_CYCLES   2.5f

#endif /* DSO_CONFIG_H */
