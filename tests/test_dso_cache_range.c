#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uintptr_t start;
    uint32_t length;
} DSO_CacheRange;

bool DSO_CacheRange_Align(const void *address,
                          uint32_t length,
                          DSO_CacheRange *out_range);

static int test_align_keeps_aligned_range_unchanged(void)
{
    DSO_CacheRange range = {0};

    if (!DSO_CacheRange_Align((const void *)0x240349E0u, 64u, &range)) {
        fprintf(stderr, "aligned range unexpectedly rejected\n");
        return 1;
    }

    if ((range.start != (uintptr_t)0x240349E0u) || (range.length != 64u)) {
        fprintf(stderr,
                "unexpected aligned range: start=0x%Ix length=%u\n",
                range.start,
                (unsigned)range.length);
        return 1;
    }

    return 0;
}

static int test_align_expands_unaligned_range_to_cache_lines(void)
{
    DSO_CacheRange range = {0};

    if (!DSO_CacheRange_Align((const void *)0x240349E3u, 33u, &range)) {
        fprintf(stderr, "unaligned range unexpectedly rejected\n");
        return 1;
    }

    if ((range.start != (uintptr_t)0x240349E0u) || (range.length != 64u)) {
        fprintf(stderr,
                "unexpected expanded range: start=0x%Ix length=%u\n",
                range.start,
                (unsigned)range.length);
        return 1;
    }

    return 0;
}

static int test_align_rejects_zero_length(void)
{
    DSO_CacheRange range = {
        .start = 0xAAAAAAAAu,
        .length = 123u,
    };

    if (DSO_CacheRange_Align((const void *)0x240349E0u, 0u, &range)) {
        fprintf(stderr, "zero-length range unexpectedly accepted\n");
        return 1;
    }

    if ((range.start != 0u) || (range.length != 0u)) {
        fprintf(stderr,
                "zero-length rejection did not clear outputs: start=0x%Ix length=%u\n",
                range.start,
                (unsigned)range.length);
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_align_keeps_aligned_range_unchanged() != 0) {
        return 1;
    }
    if (test_align_expands_unaligned_range_to_cache_lines() != 0) {
        return 1;
    }
    if (test_align_rejects_zero_length() != 0) {
        return 1;
    }

    printf("all cache range tests passed\n");
    return 0;
}
