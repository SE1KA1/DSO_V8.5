#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dso/dso_acq.h"
#include "dso/dso_sample_spec.h"
#include "ui/ui_system_setup.h"

static DSO_SampleFormat g_pending_format;
static DSO_SampleFormat g_active_format;
static uint8_t g_pending_topology;
static uint8_t g_active_topology;
static DSO_SampleFormat g_set_format;
static uint8_t g_set_topology;
static uint8_t g_request_topology;
static int g_dirty_count;

static void test_reset(DSO_SampleFormat pending_format,
                       uint8_t pending_topology,
                       DSO_SampleFormat active_format,
                       uint8_t active_topology)
{
    g_pending_format = pending_format;
    g_active_format = active_format;
    g_pending_topology = pending_topology;
    g_active_topology = active_topology;
    g_set_format = (DSO_SampleFormat)99;
    g_set_topology = 0xFFu;
    g_request_topology = 0xFFu;
    g_dirty_count = 0;
}

DSO_SampleFormat DSO_Acq_GetPendingSampleFormat(void)
{
    return g_pending_format;
}

uint8_t DSO_Acq_GetPendingHs8Topology(void)
{
    return g_pending_topology;
}

void DSO_Acq_SetPendingHs8Topology(uint8_t topology)
{
    g_set_topology = topology;
    g_pending_topology = topology;
}

void DSO_Acq_SetSampleFormat(DSO_SampleFormat format)
{
    g_set_format = format;
    g_pending_format = format;
}

DSO_SampleFormat DSO_Acq_GetSampleFormat(void)
{
    return g_active_format;
}

uint8_t DSO_Acq_GetHs8Topology(void)
{
    return g_active_topology;
}

bool DSO_Acq_RequestHs8Topology(uint8_t topology)
{
    g_request_topology = topology;
    g_active_topology = topology;
    return true;
}

void UI_Nav_MarkDirty(void)
{
    g_dirty_count++;
}

static int test_forward_cycle_is_10bit_tri_toggle(void)
{
    test_reset(DSO_SAMPLE_FORMAT_10BIT,
               DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED,
               DSO_SAMPLE_FORMAT_10BIT,
               DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED);

    UI_System_OnScreenShown();
    UI_System_Ec2Detent(+1);

    if (g_set_format != DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED ||
        g_set_topology != DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED) {
        fprintf(stderr, "10-bit -> 8-bit(3L) forward transition mismatch\n");
        return 1;
    }
    if (g_request_topology != 0xFFu) {
        fprintf(stderr, "10-bit -> 8-bit(3L) should not request live hs8 topology switch\n");
        return 1;
    }
    if (g_dirty_count != 1) {
        fprintf(stderr, "forward transition should mark dirty exactly once, got=%d\n", g_dirty_count);
        return 1;
    }

    g_active_format = DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED;
    g_active_topology = DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED;
    UI_System_Ec2Detent(+1);

    if (g_set_format != DSO_SAMPLE_FORMAT_10BIT ||
        g_set_topology != DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED) {
        fprintf(stderr, "8-bit(3L) -> 10-bit forward transition mismatch\n");
        return 1;
    }

    return 0;
}

static int test_reverse_cycle_is_10bit_tri_toggle(void)
{
    test_reset(DSO_SAMPLE_FORMAT_10BIT,
               DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED,
               DSO_SAMPLE_FORMAT_10BIT,
               DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED);

    UI_System_OnScreenShown();
    UI_System_Ec2Detent(-1);

    if (g_set_format != DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED ||
        g_set_topology != DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED) {
        fprintf(stderr, "10-bit -> 8-bit(3L) reverse transition mismatch\n");
        return 1;
    }

    g_active_format = DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED;
    g_active_topology = DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED;
    UI_System_Ec2Detent(-1);

    if (g_set_format != DSO_SAMPLE_FORMAT_10BIT ||
        g_set_topology != DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED) {
        fprintf(stderr, "8-bit(3L) -> 10-bit reverse transition mismatch\n");
        return 1;
    }

    return 0;
}

static int test_label_reports_10bit(void)
{
    const char *label;

    test_reset(DSO_SAMPLE_FORMAT_10BIT,
               DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED,
               DSO_SAMPLE_FORMAT_10BIT,
               DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED);

    label = UI_System_AdcFormatLabel();
    if (strcmp(label, "10-bit") != 0) {
        fprintf(stderr, "expected 10-bit label, got=%s\n", label);
        return 1;
    }

    return 0;
}

static int test_label_reports_8bit_tri_only(void)
{
    const char *label;

    test_reset(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED,
               DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED,
               DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED,
               DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED);

    label = UI_System_AdcFormatLabel();
    if (strcmp(label, "8-bit (3L) *") != 0 &&
        strcmp(label, "8-bit (3L)") != 0) {
        fprintf(stderr, "expected 8-bit (3L) label, got=%s\n", label);
        return 1;
    }
    if (strstr(label, "2L") != NULL) {
        fprintf(stderr, "8-bit(2L) label must not be reachable, got=%s\n", label);
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_forward_cycle_is_10bit_tri_toggle() != 0) {
        return 1;
    }
    if (test_reverse_cycle_is_10bit_tri_toggle() != 0) {
        return 1;
    }
    if (test_label_reports_10bit() != 0) {
        return 1;
    }
    if (test_label_reports_8bit_tri_only() != 0) {
        return 1;
    }

    printf("all ui system setup 10bit tests passed\n");
    return 0;
}
