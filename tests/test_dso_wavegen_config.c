#include <stdio.h>

#include "dso/dso_config.h"

int main(void)
{
#if DSO_WAVEGEN_ENABLE != 1u
    fprintf(stderr, "DSO_WAVEGEN_ENABLE must default to 1\n");
    return 1;
#endif

#if DSO_WAVEGEN_PATH != DSO_WAVEGEN_PATH_DAC_PA4
    fprintf(stderr, "DSO_WAVEGEN_PATH must default to PA4 DAC\n");
    return 1;
#endif

#ifndef DSO_WAVEGEN_PROFILE_SQUARE_250K
#error "DSO_WAVEGEN_PROFILE_SQUARE_250K must be defined"
#endif

#if DSO_WAVEGEN_PROFILE != DSO_WAVEGEN_PROFILE_SQUARE_250K
    fprintf(stderr, "default profile must be square 250 kHz\n");
    return 1;
#endif

#if DSO_WAVEGEN_DAC_UPDATE_BUDGET_HZ != 15000000u
    fprintf(stderr, "DAC update budget must default to 15 MSPS\n");
    return 1;
#endif

#if DSO_WAVEGEN_SQUARE_DUTY_PERMILLE != 500u
    fprintf(stderr, "square duty must default to 50%%\n");
    return 1;
#endif

    printf("all wavegen config tests passed\n");
    return 0;
}
