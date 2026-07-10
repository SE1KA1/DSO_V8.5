#include <stdio.h>

#include "dso/dso_acq8_refresh_policy.h"

static int test_preview_refresh_preserves_main_lane_dma_state(void)
{
    DSO_Acq8PreviewRefreshPolicy policy = DSO_Acq8_GetPreviewRefreshPolicy();

    if (!policy.pause_timer) {
        fprintf(stderr, "preview refresh must pause the trigger timer\n");
        return 1;
    }
    if (policy.allow_runtime_refresh) {
        fprintf(stderr, "preview refresh must stay disabled while 8-bit CH1 is streaming\n");
        return 1;
    }
    if (policy.stop_adc1_dma || policy.stop_adc2_dma) {
        fprintf(stderr, "preview refresh must not stop ADC1/ADC2 DMA\n");
        return 1;
    }
    if (!policy.stop_adc3_dma) {
        fprintf(stderr, "preview refresh must stop ADC3 DMA\n");
        return 1;
    }
    if (policy.restart_adc1_dma || policy.restart_adc2_dma) {
        fprintf(stderr, "preview refresh must not restart ADC1/ADC2 DMA\n");
        return 1;
    }
    if (!policy.restart_adc3_dma) {
        fprintf(stderr, "preview refresh must restart ADC3 DMA\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_preview_refresh_preserves_main_lane_dma_state() != 0) {
        return 1;
    }

    printf("all acq8 refresh policy tests passed\n");
    return 0;
}
