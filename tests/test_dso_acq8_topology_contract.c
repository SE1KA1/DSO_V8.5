#include <stdio.h>

#include "dso/dso_config.h"

#ifndef DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED
#error "DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED must be defined"
#endif

#ifndef DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED
#error "DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED must be defined"
#endif

int main(void)
{
    if (DSO_ACQ8_TOPOLOGY_DEFAULT != DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED) {
        fprintf(stderr, "8-bit topology default must be TRI_INTERLEAVED, got: %u\n",
                (unsigned)DSO_ACQ8_TOPOLOGY_DEFAULT);
        return 1;
    }

    if ((DSO_ACQ8_TOPOLOGY_SINGLE_SAFE == DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED) ||
        (DSO_ACQ8_TOPOLOGY_SINGLE_SAFE == DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED) ||
        (DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED == DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED)) {
        fprintf(stderr, "8-bit topology ids must be distinct\n");
        return 1;
    }

    puts("all acq8 topology contract tests passed");
    return 0;
}
