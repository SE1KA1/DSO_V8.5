#include <stdio.h>
#include <string.h>

#include "dso/dso_acq8_sample_unpack.h"

static int test_copy_aligned3_bytes_expands_to_u16(void)
{
    const uint8_t lane0[4] = { 1u, 2u, 3u, 4u };
    const uint8_t lane1[4] = { 11u, 12u, 13u, 14u };
    const uint8_t lane2[4] = { 21u, 22u, 23u, 24u };
    uint16_t out[12] = {0};
    uint16_t expected[6] = { 2u, 12u, 22u, 3u, 13u, 23u };
    uint32_t copied = DSO_Acq8_CopyAligned3Bytes(1u,
                                                 1u,
                                                 1u,
                                                 2u,
                                                 4u,
                                                 lane0,
                                                 lane1,
                                                 lane2,
                                                 out,
                                                 12u);

    if (copied != 6u) {
        fprintf(stderr, "unexpected aligned copy count: %lu\n", (unsigned long)copied);
        return 1;
    }
    if (memcmp(out, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "aligned byte copy did not widen correctly\n");
        return 1;
    }

    return 0;
}

static int test_unpack_dual_packed_low16_word_uses_only_low_halfword(void)
{
    uint16_t lane0 = 0u;
    uint16_t lane1 = 0u;

    DSO_Acq8_UnpackDualPackedLow16Word(0x66552211u, &lane0, &lane1);

    if ((lane0 != 0x11u) || (lane1 != 0x22u)) {
        fprintf(stderr, "dual packed low16 word unpack is incorrect\n");
        return 1;
    }

    return 0;
}

static int test_unpack_dual_packed8_preserves_master_slave_order(void)
{
    uint16_t lane0 = 0u;
    uint16_t lane1 = 0u;

    DSO_Acq8_UnpackDualPacked8(0x2211u, &lane0, &lane1);

    if ((lane0 != 0x11u) || (lane1 != 0x22u)) {
        fprintf(stderr, "dual packed8 unpack order is incorrect\n");
        return 1;
    }

    return 0;
}

static int test_copy_dual_packed8_ring_expands_pairs_to_samples(void)
{
    const uint16_t ring[4] = {
        0x2211u,
        0x4433u,
        0x6655u,
        0x8877u
    };
    uint16_t out[6] = {0};
    uint16_t expected[6] = {
        0x33u, 0x44u,
        0x55u, 0x66u,
        0x77u, 0x88u
    };
    uint32_t copied = DSO_Acq8_CopyDualPacked8(1u,
                                               3u,
                                               4u,
                                               ring,
                                               out,
                                               6u);

    if (copied != 6u) {
        fprintf(stderr, "unexpected dual packed8 copy count: %lu\n",
                (unsigned long)copied);
        return 1;
    }
    if (memcmp(out, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "dual packed8 ring copy order is incorrect\n");
        return 1;
    }

    return 0;
}

static int test_copy_dual_packed_low16_word_ring_expands_words_to_samples(void)
{
    const uint32_t ring[4] = {
        0x66552211u,
        0xAA994433u,
        0xEEDD6655u,
        0x33227711u
    };
    uint16_t out[4] = {0};
    uint16_t expected[4] = {
        0x33u, 0x44u,
        0x55u, 0x66u
    };
    uint32_t copied = DSO_Acq8_CopyDualPackedLow16WordRing(1u,
                                                           2u,
                                                           4u,
                                                           ring,
                                                           out,
                                                           4u);

    if (copied != 4u) {
        fprintf(stderr, "unexpected packed low16 word ring copy count: %lu\n",
                (unsigned long)copied);
        return 1;
    }
    if (memcmp(out, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "packed low16 word ring copy order is incorrect\n");
        return 1;
    }

    return 0;
}

static int test_copy_linear_bytes_expands_preview_samples(void)
{
    const uint8_t samples[5] = { 7u, 128u, 255u, 3u, 42u };
    uint16_t out[5] = {0};
    uint16_t expected[5] = { 7u, 128u, 255u, 3u, 42u };
    uint32_t copied = DSO_Acq8_CopyLinearBytes(samples, 5u, out, 5u);

    if (copied != 5u) {
        fprintf(stderr, "unexpected linear copy count: %lu\n", (unsigned long)copied);
        return 1;
    }
    if (memcmp(out, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "linear byte copy did not widen correctly\n");
        return 1;
    }

    return 0;
}

static int test_summarize_ring_bytes_reports_8bit_range(void)
{
    const uint8_t samples[8] = { 10u, 20u, 30u, 40u, 50u, 60u, 70u, 80u };
    uint16_t vmin = 0u;
    uint16_t vmax = 0u;
    uint16_t mean = 0u;

    DSO_Acq8_SummarizeRingBytes(samples, 8u, 6u, 4u, &vmin, &vmax, &mean);

    if ((vmin != 30u) || (vmax != 60u) || (mean != 45u)) {
        fprintf(stderr, "unexpected ring summary: %u/%u/%u\n",
                (unsigned)vmin, (unsigned)vmax, (unsigned)mean);
        return 1;
    }

    return 0;
}

static int test_low_byte_from_u16_masks_off_upper_bits(void)
{
    if (DSO_Acq8_LowByteFromU16(0x12A5u) != 0xA5u) {
        fprintf(stderr, "unexpected low-byte extract result\n");
        return 1;
    }

    return 0;
}

static int test_copy_linear_low_byte16_expands_dma_halfwords(void)
{
    const uint16_t samples[5] = { 0x1207u, 0xAB80u, 0x00FFu, 0x3403u, 0xFE2Au };
    uint16_t out[5] = {0};
    uint16_t expected[5] = { 7u, 128u, 255u, 3u, 42u };
    uint32_t copied = DSO_Acq8_CopyLinearLowByte16(samples, 5u, out, 5u);

    if (copied != 5u) {
        fprintf(stderr, "unexpected low-byte linear copy count: %lu\n", (unsigned long)copied);
        return 1;
    }
    if (memcmp(out, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "linear low-byte16 copy did not narrow correctly\n");
        return 1;
    }

    return 0;
}

static int test_copy_linear_low_byte16_normalizes_shift2_encoded_samples(void)
{
    const uint16_t samples[5] = {
        (uint16_t)(7u << 2),
        (uint16_t)(128u << 2),
        (uint16_t)(255u << 2),
        (uint16_t)(3u << 2),
        (uint16_t)(42u << 2)
    };
    uint16_t out[5] = {0};
    uint16_t expected[5] = { 7u, 128u, 255u, 3u, 42u };
    uint32_t copied = DSO_Acq8_CopyLinearLowByte16(samples, 5u, out, 5u);

    if (copied != 5u) {
        fprintf(stderr, "unexpected shift2-normalized copy count: %lu\n", (unsigned long)copied);
        return 1;
    }
    if (memcmp(out, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "linear low-byte16 copy did not normalize shift2 data\n");
        return 1;
    }

    return 0;
}

static int test_copy_linear_low_byte16_normalizes_high_byte_encoded_samples(void)
{
    const uint16_t samples[5] = {
        (uint16_t)(7u << 8),
        (uint16_t)(128u << 8),
        (uint16_t)(255u << 8),
        (uint16_t)(3u << 8),
        (uint16_t)(42u << 8)
    };
    uint16_t out[5] = {0};
    uint16_t expected[5] = { 7u, 128u, 255u, 3u, 42u };
    uint32_t copied = DSO_Acq8_CopyLinearLowByte16(samples, 5u, out, 5u);

    if (copied != 5u) {
        fprintf(stderr, "unexpected high-byte-normalized copy count: %lu\n", (unsigned long)copied);
        return 1;
    }
    if (memcmp(out, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "linear low-byte16 copy did not normalize high-byte data\n");
        return 1;
    }

    return 0;
}

static int test_summarize_ring_low_byte16_reports_8bit_range(void)
{
    const uint16_t samples[8] = {
        0xAA0Au, 0xBB14u, 0xCC1Eu, 0xDD28u,
        0xEE32u, 0xFF3Cu, 0x1146u, 0x2250u
    };
    uint16_t vmin = 0u;
    uint16_t vmax = 0u;
    uint16_t mean = 0u;

    DSO_Acq8_SummarizeRingLowByte16(samples, 8u, 6u, 4u, &vmin, &vmax, &mean);

    if ((vmin != 30u) || (vmax != 60u) || (mean != 45u)) {
        fprintf(stderr, "unexpected low-byte16 ring summary: %u/%u/%u\n",
                (unsigned)vmin, (unsigned)vmax, (unsigned)mean);
        return 1;
    }

    return 0;
}

static int test_summarize_ring_low_byte16_normalizes_shift2_encoded_window(void)
{
    const uint16_t samples[8] = {
        (uint16_t)(10u << 2), (uint16_t)(20u << 2), (uint16_t)(30u << 2), (uint16_t)(40u << 2),
        (uint16_t)(50u << 2), (uint16_t)(60u << 2), (uint16_t)(70u << 2), (uint16_t)(80u << 2)
    };
    uint16_t vmin = 0u;
    uint16_t vmax = 0u;
    uint16_t mean = 0u;

    DSO_Acq8_SummarizeRingLowByte16(samples, 8u, 6u, 4u, &vmin, &vmax, &mean);

    if ((vmin != 30u) || (vmax != 60u) || (mean != 45u)) {
        fprintf(stderr, "unexpected normalized shift2 ring summary: %u/%u/%u\n",
                (unsigned)vmin, (unsigned)vmax, (unsigned)mean);
        return 1;
    }

    return 0;
}

static int test_build_ring_spans_returns_single_linear_span(void)
{
    DSO_Acq8RingSpans spans = {0};

    DSO_Acq8_BuildRingSpans(16u, 5u, 9u, &spans);

    if ((spans.start0 != 5u) || (spans.count0 != 4u) ||
        (spans.start1 != 0u) || (spans.count1 != 0u)) {
        fprintf(stderr,
                "unexpected single ring span: [%lu,%lu] [%lu,%lu]\n",
                (unsigned long)spans.start0,
                (unsigned long)spans.count0,
                (unsigned long)spans.start1,
                (unsigned long)spans.count1);
        return 1;
    }

    return 0;
}

static int test_build_ring_spans_splits_wraparound_delta(void)
{
    DSO_Acq8RingSpans spans = {0};

    DSO_Acq8_BuildRingSpans(16u, 14u, 3u, &spans);

    if ((spans.start0 != 14u) || (spans.count0 != 2u) ||
        (spans.start1 != 0u) || (spans.count1 != 3u)) {
        fprintf(stderr,
                "unexpected wrapped ring spans: [%lu,%lu] [%lu,%lu]\n",
                (unsigned long)spans.start0,
                (unsigned long)spans.count0,
                (unsigned long)spans.start1,
                (unsigned long)spans.count1);
        return 1;
    }

    return 0;
}

static int test_build_ring_spans_returns_empty_delta_for_equal_positions(void)
{
    DSO_Acq8RingSpans spans = {1u, 1u, 1u, 1u};

    DSO_Acq8_BuildRingSpans(16u, 7u, 7u, &spans);

    if ((spans.start0 != 0u) || (spans.count0 != 0u) ||
        (spans.start1 != 0u) || (spans.count1 != 0u)) {
        fprintf(stderr,
                "unexpected empty ring spans: [%lu,%lu] [%lu,%lu]\n",
                (unsigned long)spans.start0,
                (unsigned long)spans.count0,
                (unsigned long)spans.start1,
                (unsigned long)spans.count1);
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_copy_aligned3_bytes_expands_to_u16() != 0) {
        return 1;
    }
    if (test_unpack_dual_packed_low16_word_uses_only_low_halfword() != 0) {
        return 1;
    }
    if (test_unpack_dual_packed8_preserves_master_slave_order() != 0) {
        return 1;
    }
    if (test_copy_dual_packed8_ring_expands_pairs_to_samples() != 0) {
        return 1;
    }
    if (test_copy_dual_packed_low16_word_ring_expands_words_to_samples() != 0) {
        return 1;
    }
    if (test_copy_linear_bytes_expands_preview_samples() != 0) {
        return 1;
    }
    if (test_summarize_ring_bytes_reports_8bit_range() != 0) {
        return 1;
    }
    if (test_low_byte_from_u16_masks_off_upper_bits() != 0) {
        return 1;
    }
    if (test_copy_linear_low_byte16_expands_dma_halfwords() != 0) {
        return 1;
    }
    if (test_copy_linear_low_byte16_normalizes_shift2_encoded_samples() != 0) {
        return 1;
    }
    if (test_copy_linear_low_byte16_normalizes_high_byte_encoded_samples() != 0) {
        return 1;
    }
    if (test_summarize_ring_low_byte16_reports_8bit_range() != 0) {
        return 1;
    }
    if (test_summarize_ring_low_byte16_normalizes_shift2_encoded_window() != 0) {
        return 1;
    }
    if (test_build_ring_spans_returns_single_linear_span() != 0) {
        return 1;
    }
    if (test_build_ring_spans_splits_wraparound_delta() != 0) {
        return 1;
    }
    if (test_build_ring_spans_returns_empty_delta_for_equal_positions() != 0) {
        return 1;
    }

    puts("all acq8 sample unpack tests passed");
    return 0;
}
