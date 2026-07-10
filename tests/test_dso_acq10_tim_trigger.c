#include <stdio.h>

#include "dso/dso_acq10_tim_trigger.h"

int main(void)
{
    if (DSO_Acq10_GetTimTriggerMode() != DSO_ACQ10_TIM_TRIGGER_MODE_PWM1) {
        fprintf(stderr, "10-bit TIM trigger mode must be PWM1 to match 8-bit dual interleave\n");
        return 1;
    }

    puts("all acq10 tim trigger tests passed");
    return 0;
}
