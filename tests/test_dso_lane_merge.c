#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dso_acq8_sample_unpack.h"
#include "dso_lane_merge.h"

static int test_common_delta2_uses_min_delta_and_reports_sync_mismatch(void)
{
    DSO_LaneDeltaInfo info = DSO_LaneMerge_CommonDelta2(100u, 110u, 108u, 256u);

    if ((info.lane0_delta != 10u) ||
        (info.lane1_delta != 8u) ||
        (info.common_delta != 8u) ||
        !info.phase_sync_error ||
        (info.lane2_delta != 0u)) {
        fprintf(stderr, "unexpected CommonDelta2 result: %lu/%lu/%lu common=%lu err=%u\n",
                (unsigned long)info.lane0_delta,
                (unsigned long)info.lane1_delta,
                (unsigned long)info.lane2_delta,
                (unsigned long)info.common_delta,
                info.phase_sync_error ? 1u : 0u);
        return 1;
    }

    return 0;
}

static int test_copy_aligned2_bytes_interleaves_two_lanes(void)
{
    uint8_t lane0[8] = {10u, 20u, 30u, 40u, 50u, 60u, 70u, 80u};
    uint8_t lane1[8] = {11u, 21u, 31u, 41u, 51u, 61u, 71u, 81u};
    uint16_t out[8] = {0};
    uint16_t expect[8] = {30u, 31u, 40u, 41u, 50u, 51u, 60u, 61u};
    uint32_t copied = DSO_Acq8_CopyAligned2Bytes(2u,
                                                 2u,
                                                 4u,
                                                 8u,
                                                 lane0,
                                                 lane1,
                                                 out,
                                                 8u);

    if (copied != 8u) {
        fprintf(stderr, "unexpected copied count: %lu\n", (unsigned long)copied);
        return 1;
    }
    if (memcmp(out, expect, sizeof(expect)) != 0) {
        fprintf(stderr, "unexpected interleave output\n");
        return 1;
    }

    return 0;
}

static int test_resolve_recent_window2_uses_lane_specific_end_positions(void)
{
    DSO_LaneMergeWindow win = DSO_LaneMerge_ResolveRecentWindow2(13u, 10u, 6u, 16u);

    if ((win.lane_count != 6u) ||
        (win.start0 != 7u) ||
        (win.start1 != 4u) ||
        (win.start2 != 0u)) {
        fprintf(stderr, "unexpected recent2 window: count=%lu start=%lu/%lu/%lu\n",
                (unsigned long)win.lane_count,
                (unsigned long)win.start0,
                (unsigned long)win.start1,
                (unsigned long)win.start2);
        return 1;
    }

    return 0;
}

static int test_resolve_recent_window3_wraps_each_lane_independently(void)
{
    DSO_LaneMergeWindow win = DSO_LaneMerge_ResolveRecentWindow3(13u, 6u, 0u, 4u, 16u);

    if ((win.lane_count != 4u) ||
        (win.start0 != 9u) ||
        (win.start1 != 2u) ||
        (win.start2 != 12u)) {
        fprintf(stderr, "unexpected recent3 window: count=%lu start=%lu/%lu/%lu\n",
                (unsigned long)win.lane_count,
                (unsigned long)win.start0,
                (unsigned long)win.start1,
                (unsigned long)win.start2);
        return 1;
    }

    return 0;
}

static int test_resolve_new_window2_uses_common_delta_and_lane_offsets(void)
{
    DSO_LaneMergeWindow win = DSO_LaneMerge_ResolveNewWindow2(4u, 10u, 13u, 10u, 8u, 16u);

    if ((win.lane_count != 6u) ||
        (win.start0 != 7u) ||
        (win.start1 != 4u) ||
        (win.start2 != 0u)) {
        fprintf(stderr, "unexpected new2 window: count=%lu start=%lu/%lu/%lu\n",
                (unsigned long)win.lane_count,
                (unsigned long)win.start0,
                (unsigned long)win.start1,
                (unsigned long)win.start2);
        return 1;
    }

    return 0;
}

static int test_resolve_new_window3_uses_common_delta_and_wraps(void)
{
    DSO_LaneMergeWindow win = DSO_LaneMerge_ResolveNewWindow3(4u, 10u, 13u, 0u, 10u, 8u, 16u);

    if ((win.lane_count != 6u) ||
        (win.start0 != 7u) ||
        (win.start1 != 10u) ||
        (win.start2 != 4u)) {
        fprintf(stderr, "unexpected new3 window: count=%lu start=%lu/%lu/%lu\n",
                (unsigned long)win.lane_count,
                (unsigned long)win.start0,
                (unsigned long)win.start1,
                (unsigned long)win.start2);
        return 1;
    }

    return 0;
}

static int test_resolve_common_recent_window3_uses_shared_common_end(void)
{
    DSO_LaneMergeWindow win = DSO_LaneMerge_ResolveCommonRecentWindow3(10u, 6u, 16u);

    if ((win.lane_count != 6u) ||
        (win.start0 != 4u) ||
        (win.start1 != 4u) ||
        (win.start2 != 4u)) {
        fprintf(stderr, "unexpected common recent3 window: count=%lu start=%lu/%lu/%lu\n",
                (unsigned long)win.lane_count,
                (unsigned long)win.start0,
                (unsigned long)win.start1,
                (unsigned long)win.start2);
        return 1;
    }

    return 0;
}

static int test_resolve_common_new_window3_uses_shared_common_delta(void)
{
    DSO_LaneMergeWindow win = DSO_LaneMerge_ResolveCommonNewWindow3(4u, 10u, 8u, 16u);

    if ((win.lane_count != 6u) ||
        (win.start0 != 4u) ||
        (win.start1 != 4u) ||
        (win.start2 != 4u)) {
        fprintf(stderr, "unexpected common new3 window: count=%lu start=%lu/%lu/%lu\n",
                (unsigned long)win.lane_count,
                (unsigned long)win.start0,
                (unsigned long)win.start1,
                (unsigned long)win.start2);
        return 1;
    }

    return 0;
}

static int test_unpack_dual_packed8_splits_master_and_slave_bytes(void)
{
    uint16_t lane0 = 0u;
    uint16_t lane1 = 0u;

    DSO_Acq8_UnpackDualPacked8(0xAB34u, &lane0, &lane1);
    if ((lane0 != 0x34u) || (lane1 != 0xABu)) {
        fprintf(stderr, "unexpected packed8 unpack result: %u/%u\n",
                (unsigned)lane0,
                (unsigned)lane1);
        return 1;
    }

    return 0;
}

static int test_copy_dual_packed8_interleaves_ring_words(void)
{
    uint16_t packed[4] = {0x2211u, 0x4433u, 0x6655u, 0x8877u};
    uint16_t out[8] = {0};
    uint16_t expect[8] = {0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u, 0x11u, 0x22u};
    uint32_t copied = DSO_Acq8_CopyDualPacked8(1u,
                                               4u,
                                               4u,
                                               packed,
                                               out,
                                               8u);

    if (copied != 8u) {
        fprintf(stderr, "unexpected packed8 copied count: %lu\n",
                (unsigned long)copied);
        return 1;
    }
    if (memcmp(out, expect, sizeof(expect)) != 0) {
        fprintf(stderr, "unexpected packed8 interleave output\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    int rc = 0;

    rc |= test_common_delta2_uses_min_delta_and_reports_sync_mismatch();
    rc |= test_copy_aligned2_bytes_interleaves_two_lanes();
    rc |= test_resolve_recent_window2_uses_lane_specific_end_positions();
    rc |= test_resolve_recent_window3_wraps_each_lane_independently();
    rc |= test_resolve_new_window2_uses_common_delta_and_lane_offsets();
    rc |= test_resolve_new_window3_uses_common_delta_and_wraps();
    rc |= test_resolve_common_recent_window3_uses_shared_common_end();
    rc |= test_resolve_common_new_window3_uses_shared_common_delta();
    rc |= test_unpack_dual_packed8_splits_master_and_slave_bytes();
    rc |= test_copy_dual_packed8_interleaves_ring_words();

    if (rc == 0) {
        puts("all lane merge tests passed");
    }

    return rc;
}
