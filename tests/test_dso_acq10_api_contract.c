#include "dso/dso_acq.h"
#include "dso/dso_acq_10bit.h"

int main(void)
{
    DSO_AcqDebugSnapshot snap = {0};

    DSO_Acq10_Init();
    (void)DSO_Acq10_GetDebugSnapshot(&snap);
    return 0;
}
