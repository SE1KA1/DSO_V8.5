#include "ui/ui_freq_format.h"
#include "dso/dso_freq_meter.h"
#include "dso/dso_freq_meter_math.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_str(const char *name, const char *actual, const char *expected)
{
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", name, expected, actual);
        exit(1);
    }
}

static void test_frequency_units(void)
{
    char buf[32];

    UI_FreqFormat_ExtMilliHz(1000ull,
                             true,
                             (uint8_t)DSO_FREQ_METER_STATE_VALID,
                             buf,
                             sizeof(buf));
    expect_str("1 Hz", buf, "1.000Hz");

    UI_FreqFormat_ExtMilliHz(1000000ull,
                             true,
                             (uint8_t)DSO_FREQ_METER_STATE_VALID,
                             buf,
                             sizeof(buf));
    expect_str("1 kHz", buf, "1.000kHz");

    UI_FreqFormat_ExtMilliHz(29999921270ull,
                             true,
                             (uint8_t)DSO_FREQ_METER_STATE_VALID,
                             buf,
                             sizeof(buf));
    expect_str("30 MHz", buf, "29.999921MHz");

    UI_FreqFormat_ExtMilliHz(50000000000ull,
                             true,
                             (uint8_t)DSO_FREQ_METER_STATE_VALID,
                             buf,
                             sizeof(buf));
    expect_str("50 MHz", buf, "50.000000MHz");
}

static void test_header_frequency_uses_mhz_with_six_decimals(void)
{
    char buf[20];

    UI_FreqFormat_ExtMilliHzCompact(1000ull,
                                    true,
                                    (uint8_t)DSO_FREQ_METER_STATE_VALID,
                                    buf,
                                    sizeof(buf));
    expect_str("header 1 Hz", buf, "0.000001MHz");

    UI_FreqFormat_ExtMilliHzCompact(1000000ull,
                                    true,
                                    (uint8_t)DSO_FREQ_METER_STATE_VALID,
                                    buf,
                                    sizeof(buf));
    expect_str("header 1 kHz", buf, "0.001000MHz");

    UI_FreqFormat_ExtMilliHzCompact(29999921270ull,
                                    true,
                                    (uint8_t)DSO_FREQ_METER_STATE_VALID,
                                    buf,
                                    sizeof(buf));
    expect_str("header 30 MHz", buf, "29.999921MHz");

    UI_FreqFormat_ExtMilliHzCompact(0ull,
                                    false,
                                    (uint8_t)DSO_FREQ_METER_STATE_MEASURING,
                                    buf,
                                    sizeof(buf));
    expect_str("header measuring", buf, "...");

    UI_FreqFormat_ExtMilliHzCompact(0ull,
                                    false,
                                    (uint8_t)DSO_FREQ_METER_STATE_NO_SIGNAL,
                                    buf,
                                    sizeof(buf));
    expect_str("header no signal", buf, "--");
}

static void test_frequency_states(void)
{
    char buf[32];

    UI_FreqFormat_ExtMilliHz(0ull,
                             false,
                             (uint8_t)DSO_FREQ_METER_STATE_WAIT_EDGE,
                             buf,
                             sizeof(buf));
    expect_str("wait edge", buf, "...");

    UI_FreqFormat_ExtMilliHz(0ull,
                             false,
                             (uint8_t)DSO_FREQ_METER_STATE_MEASURING,
                             buf,
                             sizeof(buf));
    expect_str("measuring", buf, "...");

    UI_FreqFormat_ExtMilliHz(0ull,
                             false,
                             (uint8_t)DSO_FREQ_METER_STATE_NO_SIGNAL,
                             buf,
                             sizeof(buf));
    expect_str("no signal", buf, "--");

    UI_FreqFormat_ExtMilliHz(0ull,
                             false,
                             (uint8_t)DSO_FREQ_METER_STATE_STOPPED,
                             buf,
                             sizeof(buf));
    expect_str("stopped", buf, "--");

    UI_FreqFormat_ExtMilliHz(0ull,
                             false,
                             (uint8_t)DSO_FREQ_METER_STATE_OVERRANGE,
                             buf,
                             sizeof(buf));
    expect_str("overrange", buf, "ERR");

    UI_FreqFormat_ExtMilliHz(0ull,
                             false,
                             (uint8_t)DSO_FREQ_METER_STATE_FAULT,
                             buf,
                             sizeof(buf));
    expect_str("fault", buf, "ERR");
}

static void test_period_and_mode(void)
{
    char buf[32];

    UI_FreqFormat_ExtPeriod(29999921270ull,
                            true,
                            (uint8_t)DSO_FREQ_METER_STATE_VALID,
                            buf,
                            sizeof(buf));
    expect_str("30 MHz period", buf, "33.333ns");

    UI_FreqFormat_ExtPeriod(1000000ull,
                            true,
                            (uint8_t)DSO_FREQ_METER_STATE_VALID,
                            buf,
                            sizeof(buf));
    expect_str("1 kHz period", buf, "1000.000us");

    UI_FreqFormat_ExtPeriod(0ull,
                            false,
                            (uint8_t)DSO_FREQ_METER_STATE_NO_SIGNAL,
                            buf,
                            sizeof(buf));
    expect_str("invalid period", buf, "--");

    UI_FreqFormat_ExtMode((uint8_t)DSO_FREQ_METER_STATE_VALID,
                          (uint8_t)DSO_FREQ_METER_MODE_GATED_COUNT,
                          buf,
                          sizeof(buf));
    expect_str("gate mode", buf, "GATE");

    UI_FreqFormat_ExtMode((uint8_t)DSO_FREQ_METER_STATE_VALID,
                          (uint8_t)DSO_FREQ_METER_MODE_LOW_CAPTURE,
                          buf,
                          sizeof(buf));
    expect_str("capture mode", buf, "CAP");

    UI_FreqFormat_ExtMode((uint8_t)DSO_FREQ_METER_STATE_MEASURING,
                          (uint8_t)DSO_FREQ_METER_MODE_GATED_COUNT,
                          buf,
                          sizeof(buf));
    expect_str("measuring mode", buf, "MEAS");

    UI_FreqFormat_ExtMode((uint8_t)DSO_FREQ_METER_STATE_NO_SIGNAL,
                          (uint8_t)DSO_FREQ_METER_MODE_GATED_COUNT,
                          buf,
                          sizeof(buf));
    expect_str("no signal mode", buf, "NO SIG");
}

static void test_hash_fold(void)
{
    uint32_t folded = UI_FreqFormat_FoldMilliHz(0x123456789ABCDEF0ull);

    if (folded != (0x9ABCDEF0u ^ 0x12345678u)) {
        fprintf(stderr, "fold mismatch: got 0x%08lX\n", (unsigned long)folded);
        exit(1);
    }
}

int main(void)
{
    test_frequency_units();
    test_header_frequency_uses_mhz_with_six_decimals();
    test_frequency_states();
    test_period_and_mode();
    test_hash_fold();
    return 0;
}
