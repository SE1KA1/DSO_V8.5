#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "dso/dso_freq_meter_math.h"

static void expect_u64(const char *label, uint64_t actual, uint64_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: actual=%llu expected=%llu\n",
                label,
                (unsigned long long)actual,
                (unsigned long long)expected);
        exit(1);
    }
}

static void expect_u32(const char *label, uint32_t actual, uint32_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: actual=%lu expected=%lu\n",
                label,
                (unsigned long)actual,
                (unsigned long)expected);
        exit(1);
    }
}

int main(void)
{
    expect_u64("1Hz period",
               DSO_FreqMeterMath_PeriodToMilliHz(200000000u, 1u, 200000000ull),
               1000ull);
    expect_u64("5Hz period",
               DSO_FreqMeterMath_PeriodToMilliHz(200000000u, 1u, 40000000ull),
               5000ull);
    expect_u64("10 periods at 1kHz",
               DSO_FreqMeterMath_PeriodToMilliHz(200000000u, 10u, 2000000ull),
               1000000ull);
    expect_u64("50MHz gate 100ms",
               DSO_FreqMeterMath_GateToMilliHz(5000000ull, 100000u),
               50000000000ull);
    expect_u64("10MHz gate 10ms",
               DSO_FreqMeterMath_GateToMilliHz(100000ull, 10000u),
               10000000000ull);
    expect_u64("extend 16-bit",
               DSO_FreqMeterMath_Extend16(76u, 0x4B40u),
               ((uint64_t)76u << 16) + 0x4B40u);
    expect_u64("zero gate invalid",
               DSO_FreqMeterMath_GateToMilliHz(1ull, 0u),
               0ull);
    expect_u64("zero period invalid",
               DSO_FreqMeterMath_PeriodToMilliHz(200000000u, 1u, 0ull),
               0ull);
    expect_u64("capture no pending overflow",
               DSO_FreqMeterMath_ExtendCapture(3u, 0x9000u, false),
               ((uint64_t)3u << 16) + 0x9000u);
    expect_u64("capture pending overflow before wrap",
               DSO_FreqMeterMath_ExtendCapture(3u, 0x9000u, true),
               ((uint64_t)3u << 16) + 0x9000u);
    expect_u64("capture pending overflow after wrap",
               DSO_FreqMeterMath_ExtendCapture(3u, 0x1000u, true),
               ((uint64_t)4u << 16) + 0x1000u);
    expect_u32("capture 1Hz stays capture",
               (uint32_t)DSO_FreqMeterMath_SelectMode(DSO_FREQ_METER_MODE_LOW_CAPTURE,
                                                       1000ull,
                                                       0u),
               (uint32_t)DSO_FREQ_METER_MODE_LOW_CAPTURE);
    expect_u32("capture 1kHz stays capture",
               (uint32_t)DSO_FreqMeterMath_SelectMode(DSO_FREQ_METER_MODE_LOW_CAPTURE,
                                                       1000000ull,
                                                       0u),
               (uint32_t)DSO_FREQ_METER_MODE_LOW_CAPTURE);
    expect_u32("capture 2kHz moves gate",
               (uint32_t)DSO_FreqMeterMath_SelectMode(DSO_FREQ_METER_MODE_LOW_CAPTURE,
                                                       2000000ull,
                                                       0u),
               (uint32_t)DSO_FREQ_METER_MODE_GATED_COUNT);
    expect_u32("gate 10kHz stays gate",
               (uint32_t)DSO_FreqMeterMath_SelectMode(DSO_FREQ_METER_MODE_GATED_COUNT,
                                                       10000000ull,
                                                       1000u),
               (uint32_t)DSO_FREQ_METER_MODE_GATED_COUNT);
    expect_u32("gate low edge count drops capture",
               (uint32_t)DSO_FreqMeterMath_SelectMode(DSO_FREQ_METER_MODE_GATED_COUNT,
                                                       10000ull,
                                                       1u),
               (uint32_t)DSO_FREQ_METER_MODE_LOW_CAPTURE);
    expect_u32("gate 1kHz drops capture",
               (uint32_t)DSO_FreqMeterMath_SelectMode(DSO_FREQ_METER_MODE_GATED_COUNT,
                                                       1000000ull,
                                                       100u),
               (uint32_t)DSO_FREQ_METER_MODE_LOW_CAPTURE);
    return 0;
}
