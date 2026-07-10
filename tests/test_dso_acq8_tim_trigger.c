#include <stdio.h>

#include "dso/dso_acq8_tim_trigger.h"

int main(void)
{
    if (DSO_Acq8_GetTimTriggerMode() != DSO_ACQ8_TIM_TRIGGER_MODE_PWM1) {
        fprintf(stderr, "8-bit TIM trigger mode must be PWM1\n");
        return 1;
    }

    puts("all acq8 tim trigger tests passed");
    return 0;
}
