#include <stdio.h>

#include "dso/dso_acq8_fault_policy.h"

int main(void)
{
    DSO_Acq8FaultPolicy policy = DSO_Acq8_GetFaultPolicy();

    if (policy.phase_sync_error_limit != 8u) {
        fprintf(stderr, "unexpected phase sync error limit: %lu\n",
                (unsigned long)policy.phase_sync_error_limit);
        return 1;
    }

    if (policy.auto_fallback_to_12bit) {
        fprintf(stderr, "8-bit mode must not auto-fallback to 12-bit\n");
        return 1;
    }

    puts("all acq8 fault policy tests passed");
    return 0;
}
