#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dso/dso_config.h"
#include "dso/dso_freq_meter.h"
#include "dso/dso_serial_debug_math.h"
#include "dso/dso_serial_debug_format.h"

static int test_format_acq_line_contains_expected_fields(void)
{
    char line[256];
    DSO_SerialDebugAcqSummary summary = {
        .frame_id = 12u,
        .channel = 1u,
        .sample_count = 4096u,
        .sample_rate_hz = 9302325u,
        .vmin = 102u,
        .vmax = 3888u,
        .mean = 2041u,
        .zero_crossings = 17u,
        .clip_low = false,
        .clip_high = true,
        .cal_applied = true,
        .cal_state = 3u,
    };

    if (DSO_SerialDebug_FormatAcqLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format acq returned 0\n");
        return 1;
    }

    if ((strstr(line, "ACQ") == NULL) ||
        (strstr(line, "frame=12") == NULL) ||
        (strstr(line, "ch=1") == NULL) ||
        (strstr(line, "fs=9302325") == NULL) ||
        (strstr(line, "clip=0/1") == NULL) ||
        (strstr(line, "cal=1") == NULL)) {
        fprintf(stderr, "unexpected acq line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_proc_line_marks_mode_quality_and_ets(void)
{
    char line[256];
    DSO_SerialDebugProcSummary summary = {
        .frame_id = 33u,
        .channel = 1u,
        .freq_valid = true,
        .freq_hz = 4000000.0f,
        .triggered = true,
        .trigger_index = 108u,
        .trigger_pos_samples = 108.37f,
        .samples_per_cycle = 2.33f,
        .period_jitter_pct = 1.8f,
        .repetitive_confidence = 0.94f,
        .recon_mode = 2u,
        .display_quality = 0u,
        .ets_active = true,
        .ets_building = false,
    };

    if (DSO_SerialDebug_FormatProcLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format proc returned 0\n");
        return 1;
    }

    if ((strstr(line, "PROC") == NULL) ||
        (strstr(line, "freq=4000000.00") == NULL) ||
        (strstr(line, "trig=1") == NULL) ||
        (strstr(line, "spc=2.33") == NULL) ||
        (strstr(line, "mode=ETS") == NULL) ||
        (strstr(line, "quality=GOOD") == NULL) ||
        (strstr(line, "ets=1/0") == NULL)) {
        fprintf(stderr, "unexpected proc line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_proc_line_marks_hs8_mode(void)
{
    char line[256];
    DSO_SerialDebugProcSummary summary = {
        .frame_id = 34u,
        .channel = 1u,
        .freq_valid = true,
        .freq_hz = 6250000.0f,
        .triggered = true,
        .trigger_index = 96u,
        .trigger_pos_samples = 96.12f,
        .samples_per_cycle = 3.10f,
        .period_jitter_pct = 0.8f,
        .repetitive_confidence = 0.97f,
        .recon_mode = 3u,
        .display_quality = 0u,
        .ets_active = false,
        .ets_building = false,
    };

    if (DSO_SerialDebug_FormatProcLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format proc hs8 returned 0\n");
        return 1;
    }

    if ((strstr(line, "mode=HS8") == NULL) ||
        (strstr(line, "quality=GOOD") == NULL) ||
        (strstr(line, "ets=0/0") == NULL)) {
        fprintf(stderr, "unexpected hs8 proc line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_freq_line_contains_pb15_state(void)
{
    char line[160];
    DSO_FreqMeterSnapshot snap = {0};

    snap.valid = true;
    snap.state = DSO_FREQ_METER_STATE_VALID;
    snap.mode = DSO_FREQ_METER_MODE_GATED_COUNT;
    snap.freq_millihz = 50000000000ull;
    snap.gate_us = 100000u;
    snap.edge_count = 5000000u;
    snap.ref_ticks = 20000000u;
    snap.overflow_count = 76u;
    snap.last_edge_age_ms = 3u;
    snap.overrange = false;

    if (DSO_SerialDebug_FormatFreqLine(&snap, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format freq returned 0\n");
        return 1;
    }

    if (strcmp(line,
               "[FREQ] state=VALID mode=GATE src=PB15 freq_mHz=50000000000 gate_us=100000 edges=5000000 ref=20000000 ovf=76 age_ms=3 over=0\r\n") != 0) {
        fprintf(stderr, "unexpected freq line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_freq_debug_line_exposes_tim12_when_invalid(void)
{
    char line[512];
    DSO_SerialDebugFreqSummary summary = {0};

    summary.now_ms = 1234u;
    summary.meter.valid = false;
    summary.meter.state = DSO_FREQ_METER_STATE_NO_SIGNAL;
    summary.meter.mode = DSO_FREQ_METER_MODE_GATED_COUNT;
    summary.meter.freq_millihz = 0ull;
    summary.meter.gate_us = 100000u;
    summary.meter.edge_count = 0u;
    summary.meter.ref_ticks = 40000000u;
    summary.meter.overflow_count = 5u;
    summary.meter.last_edge_age_ms = 2501u;
    summary.meter.overrange = false;
    summary.hw.counter = 42u;
    summary.hw.capture = 77u;
    summary.hw.overflow_count = 5u;
    summary.hw.pending_update = true;
    summary.hw.capture_ready = false;
    summary.hw.capture_overrun = true;
    summary.hw.cr1 = 0x00000081u;
    summary.hw.smcr = 0x00000067u;
    summary.hw.dier = 0x00000001u;
    summary.hw.sr = 0x00000201u;
    summary.hw.ccmr1 = 0x00000100u;
    summary.hw.ccer = 0x00000000u;

    if (DSO_SerialDebug_FormatFreqDebugLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format freq debug returned 0\n");
        return 1;
    }

    if ((strstr(line, "[FM]") == NULL) ||
        (strstr(line, "t=1234") == NULL) ||
        (strstr(line, "st=NO_SIGNAL") == NULL) ||
        (strstr(line, "md=GATE") == NULL) ||
        (strstr(line, "v=0") == NULL) ||
        (strstr(line, "f_mHz=0") == NULL) ||
        (strstr(line, "cnt=42") == NULL) ||
        (strstr(line, "gate=100000") == NULL) ||
        (strstr(line, "edges=0") == NULL) ||
        (strstr(line, "err=0") == NULL) ||
        (strstr(line, "cr1=0x00000081") == NULL) ||
        (strstr(line, "smcr=0x00000067") == NULL) ||
        (strstr(line, "sr=0x00000201") == NULL)) {
        fprintf(stderr, "unexpected freq debug line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_freq_debug_line_reports_capture_result(void)
{
    char line[512];
    DSO_SerialDebugFreqSummary summary = {0};

    summary.now_ms = 1234u;
    summary.meter.valid = true;
    summary.meter.state = DSO_FREQ_METER_STATE_VALID;
    summary.meter.mode = DSO_FREQ_METER_MODE_LOW_CAPTURE;
    summary.meter.freq_millihz = 1000000ull;
    summary.meter.gate_us = 0u;
    summary.meter.edge_count = 1u;
    summary.meter.ref_ticks = 10000u;
    summary.meter.last_edge_age_ms = 0u;
    summary.hw.capture_ready = true;
    summary.hw.capture_ticks = 10000ull;
    summary.hw.capture_periods = 1u;
    summary.hw.capture_irq_count = 1001u;
    summary.hw.update_irq_count = 153u;

    if (DSO_SerialDebug_FormatFreqDebugLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format cap freq debug returned 0\n");
        return 1;
    }
    if ((strstr(line, "md=CAP") == NULL) ||
        (strstr(line, "f_mHz=1000000") == NULL) ||
        (strstr(line, "ticks=10000") == NULL) ||
        (strstr(line, "periods=1") == NULL) ||
        (strstr(line, "cap_irq=1001") == NULL) ||
        (strstr(line, "upd_irq=153") == NULL)) {
        fprintf(stderr, "unexpected cap freq debug line: %s\n", line);
        return 1;
    }
    return 0;
}

static int test_snapshot_decision_triggers_on_freq_jump_and_limited_quality(void)
{
    DSO_SerialDebugSnapshotInput in = {
        .force_snapshot = false,
        .clipped = false,
        .freq_valid = true,
        .prev_freq_hz = 3900000.0f,
        .cur_freq_hz = 4500000.0f,
        .prev_recon_mode = 0u,
        .cur_recon_mode = 1u,
        .prev_ets_active = false,
        .cur_ets_active = false,
        .cur_display_quality = 1u,
    };
    DSO_SerialDebugSnapshotDecision out = {0};

    DSO_SerialDebug_DecideSnapshot(&in, &out);

    if (!out.emit || !out.freq_jump || !out.recon_changed || !out.limited_quality) {
        fprintf(stderr,
                "unexpected snapshot decision: emit=%d freq_jump=%d recon_changed=%d limited=%d\n",
                out.emit ? 1 : 0,
                out.freq_jump ? 1 : 0,
                out.recon_changed ? 1 : 0,
                out.limited_quality ? 1 : 0);
        return 1;
    }

    return 0;
}

static int test_format_ui_line_contains_final_frame_values(void)
{
    char line[256];
    DSO_SerialDebugUiSummary summary = {
        .frame_id = 44u,
        .channel = 1u,
        .ui_freq_hz = 4000000.0f,
        .ui_freq_valid = true,
        .display_vpp = 1880u,
        .display_vrms = 665.5f,
        .effective_sample_rate_hz = 102400000.0f,
        .tb_sec_div_ns = 100u,
        .vert_mv_per_div = 500u,
        .y_min = 24,
        .y_max = 361,
    };

    if (DSO_SerialDebug_FormatUiLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format ui returned 0\n");
        return 1;
    }

    if ((strstr(line, "UI") == NULL) ||
        (strstr(line, "eff_fs=102400000.00") == NULL) ||
        (strstr(line, "tb_ns_div=100") == NULL) ||
        (strstr(line, "mv_div=500") == NULL) ||
        (strstr(line, "y=24:361") == NULL)) {
        fprintf(stderr, "unexpected ui line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_raw_dump_line_contains_format_trigger_and_window(void)
{
    char line[512];
    const uint16_t samples[] = {2040u, 2055u, 2071u, 2086u, 2102u, 2118u};
    DSO_SerialDebugRawDumpSummary summary = {
        .frame_id = 45u,
        .channel = 1u,
        .adc_bits = 12u,
        .lane_count = 2u,
        .sample_rate_hz = 9302325u,
        .total_count = 10240u,
        .trigger_index = 5119,
        .window_start = 5104u,
        .window_count = 6u,
        .samples = samples,
    };

    if (DSO_SerialDebug_FormatRawDumpLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format raw dump returned 0\n");
        return 1;
    }

    if ((strstr(line, "RAWD") == NULL) ||
        (strstr(line, "frame=45") == NULL) ||
        (strstr(line, "ch=1") == NULL) ||
        (strstr(line, "bits=12") == NULL) ||
        (strstr(line, "lanes=2") == NULL) ||
        (strstr(line, "fs=9302325") == NULL) ||
        (strstr(line, "total=10240") == NULL) ||
        (strstr(line, "trig=5119") == NULL) ||
        (strstr(line, "ofs=5104") == NULL) ||
        (strstr(line, "count=6") == NULL) ||
        (strstr(line, "data=2040,2055,2071,2086,2102,2118") == NULL)) {
        fprintf(stderr, "unexpected raw dump line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_snapshot_gate_rate_limits_non_forced_events(void)
{
    DSO_SerialDebugSnapshotDecision decision = {
        .emit = true,
        .limited_quality = true,
        .forced = false,
    };

    if (DSO_SerialDebug_ShouldEmitSnapshot(&decision, 500u, 100u, 1000u)) {
        fprintf(stderr, "snapshot gate emitted too early\n");
        return 1;
    }

    if (!DSO_SerialDebug_ShouldEmitSnapshot(&decision, 1201u, 100u, 1000u)) {
        fprintf(stderr, "snapshot gate failed to emit after interval\n");
        return 1;
    }

    decision.forced = true;
    if (!DSO_SerialDebug_ShouldEmitSnapshot(&decision, 200u, 100u, 1000u)) {
        fprintf(stderr, "forced snapshot was unexpectedly gated\n");
        return 1;
    }

    return 0;
}

static int test_format_hs8_line_contains_lane_and_ch2_state(void)
{
    char line[512];
    DSO_SerialDebugHs8Summary summary = {
        .frame_id = 52u,
        .running = true,
        .sample_rate_hz = 25000000u,
        .ch2_sample_rate_hz = 8333333u,
        .adc_kernel_clock_hz = 50000000u,
        .dma_pos = 128u,
        .lane0_pos = 128u,
        .lane1_pos = 128u,
        .lane2_pos = 128u,
        .ch2_valid = true,
        .ch2_capture_active = false,
        .ch2_count = 5120u,
        .phase_sync_errors = 0u,
        .overrun_count = 0u,
        .lane0_vmin = 11u,
        .lane0_vmax = 244u,
        .lane0_mean = 126u,
        .lane1_vmin = 10u,
        .lane1_vmax = 243u,
        .lane1_mean = 125u,
        .lane2_vmin = 12u,
        .lane2_vmax = 245u,
        .lane2_mean = 127u,
        .ch2_vmin = 14u,
        .ch2_vmax = 220u,
        .ch2_mean = 118u,
    };

    if (DSO_SerialDebug_FormatHs8Line(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format hs8 returned 0\n");
        return 1;
    }

    if ((strstr(line, "HS8") == NULL) ||
        (strstr(line, "frame=52") == NULL) ||
        (strstr(line, "run=1") == NULL) ||
        (strstr(line, "fs=25000000") == NULL) ||
        (strstr(line, "lanes=128/128/128") == NULL) ||
        (strstr(line, "ch2=1/0/5120") == NULL) ||
        (strstr(line, "l0=11:244:126") == NULL) ||
        (strstr(line, "ch2v=14:220:118") == NULL)) {
        fprintf(stderr, "unexpected hs8 line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_hs8regs_line_contains_timer_adc_and_dma_state(void)
{
    char line[896];
    DSO_SerialDebugHs8RegsSummary summary = {
        .frame_id = 53u,
        .tim_enabled = true,
        .tim_counter = 17u,
        .tim_arr = 119u,
        .tim_ccr1 = 20u,
        .tim_ccr2 = 60u,
        .tim_ccr3 = 100u,
        .tim_cr1 = 0x00000081u,
        .tim_ccmr1 = 0x00006868u,
        .tim_ccmr2 = 0x00000068u,
        .tim_ccer = 0x00001111u,
        .tim_bdtr = 0x00008000u,
        .tim_sr = 0x0000001Fu,
        .dma_ndtr0 = 10240u,
        .dma_ndtr1 = 10240u,
        .dma_ndtr2 = 10240u,
        .adc1_cr = 0x80000005u,
        .adc2_cr = 0x80000005u,
        .adc3_cr = 0x80000005u,
        .adc1_cfgr = 0x80100000u,
        .adc2_cfgr = 0x80100010u,
        .adc3_cfgr = 0x80100020u,
        .adc1_isr = 0x00000801u,
        .adc2_isr = 0x00000801u,
        .adc3_isr = 0x00000801u,
    };

    if (DSO_SerialDebug_FormatHs8RegsLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format hs8regs returned 0\n");
        return 1;
    }

    if ((strstr(line, "HS8R") == NULL) ||
        (strstr(line, "frame=53") == NULL) ||
        (strstr(line, "tim=1/17/119") == NULL) ||
        (strstr(line, "ccr=20/60/100") == NULL) ||
        (strstr(line, "timr=0x00000081/0x00006868/0x00000068/0x00001111/0x00008000/0x0000001F") == NULL) ||
        (strstr(line, "ndtr=10240/10240/10240") == NULL) ||
        (strstr(line, "cr=0x80000005/0x80000005/0x80000005") == NULL) ||
        (strstr(line, "cfgr=0x80100000/0x80100010/0x80100020") == NULL) ||
        (strstr(line, "isr=0x00000801/0x00000801/0x00000801") == NULL)) {
        fprintf(stderr, "unexpected hs8regs line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_hs8diag_line_contains_alignment_and_trigger_fields(void)
{
    char line[1024];
    DSO_SerialDebugHs8DiagSummary summary = {
        .frame_id = 54u,
        .hs8_topology = 2u,
        .hs8_lane_count = 2u,
        .data_ready = false,
        .data_ready_lane_count = 3u,
        .sample_rate_hz = 25000000u,
        .dma_sample_rate_hz = 24980000u,
        .dma_sample_count = 123456u,
        .lane0_dma_sample_rate_hz = 8333333u,
        .lane1_dma_sample_rate_hz = 8330000u,
        .lane2_dma_sample_rate_hz = 8316667u,
        .lane0_dma_pos_sample_rate_hz = 8333333u,
        .lane1_dma_pos_sample_rate_hz = 8333333u,
        .lane2_dma_pos_sample_rate_hz = 8333333u,
        .lane0_dma_accept_per_mille = 1000u,
        .lane1_dma_accept_per_mille = 1000u,
        .lane2_dma_accept_per_mille = 998u,
        .lane0_dma_pos_accept_per_mille = 1000u,
        .lane1_dma_pos_accept_per_mille = 1000u,
        .lane2_dma_pos_accept_per_mille = 1000u,
        .dma_pos_rate_max_elapsed_ms = 1u,
        .lane0_dma_sample_count = 41152u,
        .lane1_dma_sample_count = 41150u,
        .lane2_dma_sample_count = 41154u,
        .lane0_dma_half_count = 5u,
        .lane1_dma_half_count = 4u,
        .lane2_dma_half_count = 5u,
        .lane0_dma_full_count = 5u,
        .lane1_dma_full_count = 4u,
        .lane2_dma_full_count = 5u,
        .trigger_rate_hz = 8333333u,
        .accepted_trigger_div = 3u,
        .common_pos = 126u,
        .lane0_pos = 128u,
        .lane1_pos = 127u,
        .lane2_pos = 126u,
        .lane0_delta = 8u,
        .lane1_delta = 7u,
        .lane2_delta = 6u,
        .common_delta = 6u,
        .phase_sync_error = true,
        .overrun_count = 2u,
        .lane0_overrun_count = 0u,
        .lane1_overrun_count = 2u,
        .lane2_overrun_count = 0u,
        .freq_valid = true,
        .freq_hz = 6250000.0f,
        .samples_per_cycle = 4.00f,
        .trig_pos_pct = 37.50f,
        .trigger_pos_samples = 96.12f,
        .trigger_grid_phase = 0.12f,
        .allow_ets_requested = true,
        .allow_ets_effective = true,
        .fast_mode = true,
        .recon_fast_mode = true,
        .sample_mode = true,
        .rep_gate_ok = true,
        .repetitive_confidence_min = 0.50f,
        .hs8_spc_gate_ok = true,
        .ets_spc_gate_ok = true,
        .hs8_candidate = true,
        .hs8_ready = false,
        .ets_candidate = true,
        .hs8_frame_local = true,
        .hs8_accum_start = 8064u,
        .hs8_accum_count = 320u,
    };

    if (DSO_SerialDebug_FormatHs8DiagLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format hs8diag returned 0\n");
        return 1;
    }

    if ((strstr(line, "HS8D") == NULL) ||
        (strstr(line, "frame=54") == NULL) ||
        (strstr(line, "top=2") == NULL) ||
        (strstr(line, "lc=2") == NULL) ||
        (strstr(line, "ready=0") == NULL) ||
        (strstr(line, "rlanes=3") == NULL) ||
        (strstr(line, "fs=25000000") == NULL) ||
        (strstr(line, "dmafs=24980000") == NULL) ||
        (strstr(line, "dmac=123456") == NULL) ||
        (strstr(line, "dmafs_l=8333333/8330000/8316667") == NULL) ||
        (strstr(line, "posfs_l=8333333/8333333/8333333") == NULL) ||
        (strstr(line, "acc_l=1000/1000/998") == NULL) ||
        (strstr(line, "pacc_l=1000/1000/1000") == NULL) ||
        (strstr(line, "posdt=1") == NULL) ||
        (strstr(line, "dmac_l=41152/41150/41154") == NULL) ||
        (strstr(line, "ht_l=5/4/5") == NULL) ||
        (strstr(line, "tc_l=5/4/5") == NULL) ||
        (strstr(line, "trigfs=8333333") == NULL) ||
        (strstr(line, "div=3") == NULL) ||
        (strstr(line, "pos=126") == NULL) ||
        (strstr(line, "lanes=128/127/126") == NULL) ||
        (strstr(line, "d=8/7/6") == NULL) ||
        (strstr(line, "cd=6") == NULL) ||
        (strstr(line, "phase_err=1") == NULL) ||
        (strstr(line, "ovr=2") == NULL) ||
        (strstr(line, "ovr_l=0/2/0") == NULL) ||
        (strstr(line, "freq=6250000.00") == NULL) ||
        (strstr(line, "spc=4.00") == NULL) ||
        (strstr(line, "trig=37.50") == NULL) ||
        (strstr(line, "tpos=96.12") == NULL) ||
        (strstr(line, "grid=0.120") == NULL) ||
        (strstr(line, "req=1") == NULL) ||
        (strstr(line, "eff=1") == NULL) ||
        (strstr(line, "fast=1") == NULL) ||
        (strstr(line, "rfast=1") == NULL) ||
        (strstr(line, "samp=1") == NULL) ||
        (strstr(line, "repok=1") == NULL) ||
        (strstr(line, "repmin=0.50") == NULL) ||
        (strstr(line, "hs8spc=1") == NULL) ||
        (strstr(line, "etsspc=1") == NULL) ||
        (strstr(line, "hs8=1/0") == NULL) ||
        (strstr(line, "etsc=1") == NULL) ||
        (strstr(line, "hloc=1") == NULL) ||
        (strstr(line, "hwin=8064+320") == NULL)) {
        fprintf(stderr, "unexpected hs8diag line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_phase_line_contains_single_safe_phase_fields(void)
{
    char line[384];
    DSO_SerialDebugPhaseSummary summary = {
        .frame_id = 56u,
        .hs8_topology = 1u,
        .hs8_lane_count = 1u,
        .sample_rate_hz = 25000000u,
        .dma_pos = 3708u,
        .lane0_pos = 3708u,
        .common_delta = 9u,
        .phase_sync_error = false,
        .freq_valid = true,
        .triggered = true,
        .trigger_index = 52,
        .trigger_pos_samples = 51.10f,
        .trigger_grid_phase = 0.100f,
        .samples_per_cycle = 4.00f,
        .freq_hz = 5000000.0f,
    };

    if (DSO_SerialDebug_FormatPhaseLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format phase returned 0\n");
        return 1;
    }

    if ((strstr(line, "PHASE") == NULL) ||
        (strstr(line, "frame=56") == NULL) ||
        (strstr(line, "top=1") == NULL) ||
        (strstr(line, "lc=1") == NULL) ||
        (strstr(line, "fs=25000000") == NULL) ||
        (strstr(line, "pos=3708") == NULL) ||
        (strstr(line, "lane=3708") == NULL) ||
        (strstr(line, "d=9") == NULL) ||
        (strstr(line, "phase_err=0") == NULL) ||
        (strstr(line, "valid=1") == NULL) ||
        (strstr(line, "trig=1") == NULL) ||
        (strstr(line, "trig_i=52") == NULL) ||
        (strstr(line, "tpos=51.10") == NULL) ||
        (strstr(line, "grid=0.100") == NULL) ||
        (strstr(line, "spc=4.00") == NULL) ||
        (strstr(line, "freq=5000000.00") == NULL)) {
        fprintf(stderr, "unexpected phase line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_phase_stats_line_contains_grid_summary_fields(void)
{
    char line[384];
    DSO_SerialDebugPhaseStatsSummary summary = {
        .frame_id = 88u,
        .hs8_topology = 1u,
        .hs8_lane_count = 1u,
        .sample_rate_hz = 25000000u,
        .freq_hz = 5000000.0f,
        .valid_count = 32u,
        .grid_min = 0.071f,
        .grid_max = 0.214f,
        .grid_span = 0.143f,
        .grid_mean = 0.128f,
    };

    if (DSO_SerialDebug_FormatPhaseStatsLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format phase stats returned 0\n");
        return 1;
    }

    if ((strstr(line, "PSTAT") == NULL) ||
        (strstr(line, "frame=88") == NULL) ||
        (strstr(line, "top=1") == NULL) ||
        (strstr(line, "lc=1") == NULL) ||
        (strstr(line, "fs=25000000") == NULL) ||
        (strstr(line, "freq=5000000.00") == NULL) ||
        (strstr(line, "count=32") == NULL) ||
        (strstr(line, "min=0.071") == NULL) ||
        (strstr(line, "max=0.214") == NULL) ||
        (strstr(line, "span=0.143") == NULL) ||
        (strstr(line, "mean=0.128") == NULL)) {
        fprintf(stderr, "unexpected phase stats line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_interleave_phase_line_contains_lane_phase_fields(void)
{
    char line[384];
    DSO_SerialDebugInterleavePhaseSummary summary = {
        .frame_id = 90u,
        .hs8_topology = 2u,
        .hs8_lane_count = 2u,
        .sample_rate_hz = 25000000u,
        .lane0_pos = 128u,
        .lane1_pos = 127u,
        .common_delta = 6u,
        .phase_sync_error = false,
        .freq_valid = true,
        .triggered = true,
        .trigger_index = 96,
        .trigger_pos_samples = 96.12f,
        .samples_per_cycle = 4.00f,
        .freq_hz = 6250000.0f,
        .lane_phase_sep = 0.250f,
        .common_phase = 0.095f,
        .rel_lane0_phase = -0.030f,
        .rel_lane1_phase = 0.220f,
    };

    if (DSO_SerialDebug_FormatInterleavePhaseLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format interleave phase returned 0\n");
        return 1;
    }

    if ((strstr(line, "IPHASE") == NULL) ||
        (strstr(line, "frame=90") == NULL) ||
        (strstr(line, "top=2") == NULL) ||
        (strstr(line, "lc=2") == NULL) ||
        (strstr(line, "fs=25000000") == NULL) ||
        (strstr(line, "lanes=128/127") == NULL) ||
        (strstr(line, "cd=6") == NULL) ||
        (strstr(line, "phase_err=0") == NULL) ||
        (strstr(line, "valid=1") == NULL) ||
        (strstr(line, "trig=1") == NULL) ||
        (strstr(line, "trig_i=96") == NULL) ||
        (strstr(line, "tpos=96.12") == NULL) ||
        (strstr(line, "spc=4.00") == NULL) ||
        (strstr(line, "freq=6250000.00") == NULL) ||
        (strstr(line, "sep=0.250") == NULL) ||
        (strstr(line, "cm=0.095") == NULL) ||
        (strstr(line, "r0=-0.030") == NULL) ||
        (strstr(line, "r1=0.220") == NULL)) {
        fprintf(stderr, "unexpected interleave phase line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_interleave_stats_line_contains_lane_span_fields(void)
{
    char line[384];
    DSO_SerialDebugInterleaveStatsSummary summary = {
        .frame_id = 91u,
        .hs8_topology = 2u,
        .hs8_lane_count = 2u,
        .sample_rate_hz = 25000000u,
        .freq_hz = 6250000.0f,
        .valid_count = 32u,
        .sep_min = 0.250f,
        .sep_max = 0.250f,
        .sep_span = 0.000f,
        .sep_mean = 0.250f,
        .common_min = 0.090f,
        .common_max = 0.100f,
        .common_span = 0.010f,
        .common_mean = 0.095f,
        .rel_lane0_min = -0.030f,
        .rel_lane0_max = 0.020f,
        .rel_lane0_span = 0.050f,
        .rel_lane0_mean = -0.005f,
        .rel_lane1_min = 0.220f,
        .rel_lane1_max = 0.270f,
        .rel_lane1_span = 0.050f,
        .rel_lane1_mean = 0.245f,
    };

    if (DSO_SerialDebug_FormatInterleaveStatsLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format interleave stats returned 0\n");
        return 1;
    }

    if ((strstr(line, "ISTAT") == NULL) ||
        (strstr(line, "frame=91") == NULL) ||
        (strstr(line, "top=2") == NULL) ||
        (strstr(line, "lc=2") == NULL) ||
        (strstr(line, "fs=25000000") == NULL) ||
        (strstr(line, "freq=6250000.00") == NULL) ||
        (strstr(line, "count=32") == NULL) ||
        (strstr(line, "sep=0.250/0.250/0.000/0.250") == NULL) ||
        (strstr(line, "cm=0.090/0.100/0.010/0.095") == NULL) ||
        (strstr(line, "r0=-0.030/0.020/0.050/-0.005") == NULL) ||
        (strstr(line, "r1=0.220/0.270/0.050/0.245") == NULL)) {
        fprintf(stderr, "unexpected interleave stats line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_hs8window_line_contains_lane_windows_and_merge_preview(void)
{
    char line[512];
    const uint16_t lane0[] = {100u, 103u, 106u, 109u, 112u, 115u};
    const uint16_t lane1[] = {101u, 104u, 107u, 110u, 113u, 116u};
    const uint16_t lane2[] = {102u, 105u, 108u, 111u, 114u, 117u};
    DSO_SerialDebugHs8Window summary = {
        .frame_id = 55u,
        .hs8_topology = 2u,
        .hs8_lane_count = 2u,
        .lane0_start = 220u,
        .lane1_start = 219u,
        .lane2_start = 218u,
        .sample_count = 6u,
        .lane0 = lane0,
        .lane1 = lane1,
        .lane2 = lane2,
    };

    if (DSO_SerialDebug_FormatHs8WindowLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format hs8window returned 0\n");
        return 1;
    }

    if ((strstr(line, "HS8W") == NULL) ||
        (strstr(line, "frame=55") == NULL) ||
        (strstr(line, "top=2") == NULL) ||
        (strstr(line, "lc=2") == NULL) ||
        (strstr(line, "ofs=220/219/218") == NULL) ||
        (strstr(line, "count=6") == NULL) ||
        (strstr(line, "l0=100,103,106,109,112,115") == NULL) ||
        (strstr(line, "l1=101,104,107,110,113,116") == NULL) ||
        (strstr(line, "l2=102,105,108,111,114,117") == NULL) ||
        (strstr(line, "m=100,101,103,104,106,107,109,110,112,113,115,116") == NULL)) {
        fprintf(stderr, "unexpected hs8window line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_hs8order_line_contains_permutation_frequency_estimates(void)
{
    char line[384];
    const uint16_t lane0[] = {128u, 220u, 128u, 36u, 128u, 220u, 128u, 36u};
    const uint16_t lane1[] = {220u, 128u, 36u, 128u, 220u, 128u, 36u, 128u};
    const uint16_t lane2[] = {128u, 36u, 128u, 220u, 128u, 36u, 128u, 220u};
    DSO_SerialDebugHs8OrderSummary summary = {
        .frame_id = 56u,
        .hs8_topology = 3u,
        .hs8_lane_count = 3u,
        .sample_rate_hz = 24000000u,
        .current_freq_valid = true,
        .current_freq_hz = 6000000.0f,
        .sample_count = 8u,
        .lane0 = lane0,
        .lane1 = lane1,
        .lane2 = lane2,
    };

    if (DSO_SerialDebug_FormatHs8OrderLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format hs8order returned 0\n");
        return 1;
    }

    if ((strstr(line, "ORD") == NULL) ||
        (strstr(line, "frame=56") == NULL) ||
        (strstr(line, "top=3") == NULL) ||
        (strstr(line, "lc=3") == NULL) ||
        (strstr(line, "fs=24000000") == NULL) ||
        (strstr(line, "cur=6000000") == NULL) ||
        (strstr(line, "p012=") == NULL) ||
        (strstr(line, "p021=") == NULL) ||
        (strstr(line, "p102=") == NULL) ||
        (strstr(line, "p120=") == NULL) ||
        (strstr(line, "p201=") == NULL) ||
        (strstr(line, "p210=") == NULL)) {
        fprintf(stderr, "unexpected hs8order line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_hs8order_line_can_mark_physical_offsets(void)
{
    char line[384];
    const uint16_t lane0[] = {128u, 220u, 128u, 36u, 128u, 220u, 128u, 36u};
    const uint16_t lane1[] = {220u, 128u, 36u, 128u, 220u, 128u, 36u, 128u};
    const uint16_t lane2[] = {128u, 36u, 128u, 220u, 128u, 36u, 128u, 220u};
    DSO_SerialDebugHs8OrderSummary summary = {
        .line_tag = "ORDP",
        .frame_id = 57u,
        .hs8_topology = 3u,
        .hs8_lane_count = 3u,
        .sample_rate_hz = 24000000u,
        .current_freq_valid = true,
        .current_freq_hz = 6000000.0f,
        .lane0_start = 10u,
        .lane1_start = 13u,
        .lane2_start = 16u,
        .sample_count = 8u,
        .lane0 = lane0,
        .lane1 = lane1,
        .lane2 = lane2,
    };

    if (DSO_SerialDebug_FormatHs8OrderLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format physical hs8order returned 0\n");
        return 1;
    }

    if ((strstr(line, "ORDP") == NULL) ||
        (strstr(line, "frame=57") == NULL) ||
        (strstr(line, "ofs=10/13/16") == NULL) ||
        (strstr(line, "cur=6000000") == NULL) ||
        (strstr(line, "p210=") == NULL)) {
        fprintf(stderr, "unexpected physical hs8order line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_dbg_init_line_contains_hs8_config(void)
{
    char line[256];
    DSO_SerialDebugInitSummary summary = {
        .hs8_stage = 2u,
        .hs8_topology = 3u,
        .hs8_lane_count = 3u,
        .hs8_ch2_realtime = false,
        .acq8_lane2_adc_channel = 1u,
    };

    if (DSO_SerialDebug_FormatInitLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format init returned 0\n");
        return 1;
    }

    if ((strstr(line, "DBG init=1") == NULL) ||
        (strstr(line, "hs8_stage=2") == NULL) ||
        (strstr(line, "hs8_top=3") == NULL) ||
        (strstr(line, "lc=3") == NULL) ||
        (strstr(line, "hs8_ch2=0") == NULL) ||
        (strstr(line, "lane2_ch=1") == NULL) ||
        (strstr(line, "hs8_dual_ch2") != NULL)) {
        fprintf(stderr, "unexpected init line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_format_hs8cal_line_contains_solver_state(void)
{
    char line[384];
    DSO_SerialDebugHs8CalSummary summary = {
        .frame_id = 50u,
        .hs8_topology = 2u,
        .hs8_lane_count = 2u,
        .service_calls = 51u,
        .stage = 6u,
        .good_frames = 0u,
        .ready = false,
        .lane1_used = 6649u,
        .lane1_fail = 0u,
        .lane2_used = 6711u,
        .lane2_fail = 4u,
        .gain1 = 1.000f,
        .offset1 = 0.0f,
        .skew1 = 0.00f,
        .gain2 = 1.000f,
        .offset2 = 0.0f,
        .skew2 = 0.00f,
        .repetitive_confidence = 0.55f,
        .locked_freq_hz = 0.0f,
    };

    if (DSO_SerialDebug_FormatHs8CalLine(&summary, line, sizeof(line)) == 0u) {
        fprintf(stderr, "format hs8cal returned 0\n");
        return 1;
    }

    if ((strstr(line, "[HS8CAL]") == NULL) ||
        (strstr(line, "f=50") == NULL) ||
        (strstr(line, "top=2") == NULL) ||
        (strstr(line, "lc=2") == NULL) ||
        (strstr(line, "calls=51") == NULL) ||
        (strstr(line, "stage=6") == NULL) ||
        (strstr(line, "l2f=4") == NULL) ||
        (strstr(line, "rep=0.55") == NULL) ||
        (strstr(line, "lock=0") == NULL)) {
        fprintf(stderr, "unexpected hs8cal line: %s\n", line);
        return 1;
    }

    return 0;
}

static int test_unwrap_to_anchor_handles_periodic_wraparound(void)
{
    float unwrapped = DSO_SerialDebug_UnwrapToAnchor(0.098f, -0.095f, 0.200f);

    if ((unwrapped < -0.103f) || (unwrapped > -0.101f)) {
        fprintf(stderr, "unexpected unwrapped value: %.6f\n", (double)unwrapped);
        return 1;
    }

    return 0;
}

static int test_normalize_to_centered_period_handles_total_sample_phase(void)
{
    float normalized = DSO_SerialDebug_NormalizeToCenteredPeriod(0.071f, 0.100f);

    if ((normalized < -0.030f) || (normalized > -0.028f)) {
        fprintf(stderr, "unexpected normalized value: %.6f\n", (double)normalized);
        return 1;
    }

    return 0;
}

static int test_default_serial_debug_config_keeps_runtime_log_compact(void)
{
    if (DSO_SERIAL_DEBUG_RAW_DUMP_ENABLE != 0u) {
        fprintf(stderr,
                "expected RAWD default disabled after order diagnosis, got %u\n",
                (unsigned)DSO_SERIAL_DEBUG_RAW_DUMP_ENABLE);
        return 1;
    }

    if (DSO_SERIAL_DEBUG_HS8_STAGE != 1u) {
        fprintf(stderr,
                "expected HS8 debug default to compact HS8D stage 1, got %u\n",
                (unsigned)DSO_SERIAL_DEBUG_HS8_STAGE);
        return 1;
    }

    if ((DSO_FREQ_METER_SERIAL_DEBUG_ENABLE != 0u) ||
        (DSO_FREQ_METER_SERIAL_DEBUG_ONLY != 0u) ||
        (DSO_FREQ_METER_SERIAL_DEBUG_INTERVAL_MS != 100u)) {
        fprintf(stderr,
                "unexpected frequency debug defaults: enable=%u only=%u interval=%u\n",
                (unsigned)DSO_FREQ_METER_SERIAL_DEBUG_ENABLE,
                (unsigned)DSO_FREQ_METER_SERIAL_DEBUG_ONLY,
                (unsigned)DSO_FREQ_METER_SERIAL_DEBUG_INTERVAL_MS);
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_format_acq_line_contains_expected_fields() != 0) {
        return 1;
    }
    if (test_format_proc_line_marks_mode_quality_and_ets() != 0) {
        return 1;
    }
    if (test_format_proc_line_marks_hs8_mode() != 0) {
        return 1;
    }
    if (test_format_freq_line_contains_pb15_state() != 0) {
        return 1;
    }
    if (test_format_freq_debug_line_exposes_tim12_when_invalid() != 0) {
        return 1;
    }
    if (test_format_freq_debug_line_reports_capture_result() != 0) {
        return 1;
    }
    if (test_snapshot_decision_triggers_on_freq_jump_and_limited_quality() != 0) {
        return 1;
    }
    if (test_format_ui_line_contains_final_frame_values() != 0) {
        return 1;
    }
    if (test_format_raw_dump_line_contains_format_trigger_and_window() != 0) {
        return 1;
    }
    if (test_snapshot_gate_rate_limits_non_forced_events() != 0) {
        return 1;
    }
    if (test_format_hs8_line_contains_lane_and_ch2_state() != 0) {
        return 1;
    }
    if (test_format_hs8regs_line_contains_timer_adc_and_dma_state() != 0) {
        return 1;
    }
    if (test_format_hs8diag_line_contains_alignment_and_trigger_fields() != 0) {
        return 1;
    }
    if (test_format_phase_line_contains_single_safe_phase_fields() != 0) {
        return 1;
    }
    if (test_format_phase_stats_line_contains_grid_summary_fields() != 0) {
        return 1;
    }
    if (test_format_interleave_phase_line_contains_lane_phase_fields() != 0) {
        return 1;
    }
    if (test_format_interleave_stats_line_contains_lane_span_fields() != 0) {
        return 1;
    }
    if (test_format_hs8window_line_contains_lane_windows_and_merge_preview() != 0) {
        return 1;
    }
    if (test_format_hs8order_line_contains_permutation_frequency_estimates() != 0) {
        return 1;
    }
    if (test_format_hs8order_line_can_mark_physical_offsets() != 0) {
        return 1;
    }
    if (test_format_dbg_init_line_contains_hs8_config() != 0) {
        return 1;
    }
    if (test_format_hs8cal_line_contains_solver_state() != 0) {
        return 1;
    }
    if (test_unwrap_to_anchor_handles_periodic_wraparound() != 0) {
        return 1;
    }
    if (test_normalize_to_centered_period_handles_total_sample_phase() != 0) {
        return 1;
    }
    if (test_default_serial_debug_config_keeps_runtime_log_compact() != 0) {
        return 1;
    }

    printf("all serial debug tests passed\n");
    return 0;
}
