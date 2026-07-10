#include <stdio.h>
#include <stdlib.h>

#include "dso/dso_10bit_display_sync.h"

static void expect_int(const char *label, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: actual=%d expected=%d\n", label, actual, expected);
        exit(1);
    }
}

int main(void)
{
    DSO_ProcessAnalysis src = {0};
    DSO_ProcessAnalysis dst = {0};

    src.triggered = true;
    src.trigger_pos_samples = 100.5f;
    src.trigger_index = 100;

    expect_int("map by rate",
               DSO_TriggerDisplaySync_MapByRate(&src, 20000000u, 10000000u, 1024u, &dst),
               1);
    expect_int("mapped index", dst.trigger_index, 50);
    return 0;
}
