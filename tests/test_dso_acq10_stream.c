#include <stdint.h>
#include <stdio.h>

#include "dso/dso_acq10_stream.h"

static int test_interleave_dual_lanes_preserves_lane_order(void)
{
    static const uint16_t lane0[] = {100u, 200u, 300u, 400u};
    static const uint16_t lane1[] = {110u, 210u, 310u, 410u};
    uint16_t merged[8] = {0};

    DSO_Acq10Stream_InterleaveDual(lane0, lane1, 4u, merged);

    if (merged[0] != 100u || merged[1] != 110u ||
        merged[2] != 200u || merged[3] != 210u ||
        merged[4] != 300u || merged[5] != 310u ||
        merged[6] != 400u || merged[7] != 410u) {
        fprintf(stderr, "interleave order mismatch\n");
        return 1;
    }

    return 0;
}

static int test_copy_new_merged_wraps_cleanly(void)
{
    static const uint16_t lane0[8] = {10u, 20u, 30u, 40u, 50u, 60u, 70u, 80u};
    static const uint16_t lane1[8] = {11u, 21u, 31u, 41u, 51u, 61u, 71u, 81u};
    uint16_t out[8] = {0};
    uint32_t n = DSO_Acq10Stream_CopyNewMerged(6u, 2u, 8u, lane0, lane1, out, 8u);

    if (n != 8u ||
        out[0] != 70u || out[1] != 71u ||
        out[2] != 80u || out[3] != 81u ||
        out[4] != 10u || out[5] != 11u ||
        out[6] != 20u || out[7] != 21u) {
        fprintf(stderr, "wrapped merged copy mismatch n=%u\n", (unsigned)n);
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_interleave_dual_lanes_preserves_lane_order() != 0) {
        return 1;
    }
    if (test_copy_new_merged_wraps_cleanly() != 0) {
        return 1;
    }

    printf("all acq10 stream tests passed\n");
    return 0;
}
