/**
 * @file module_5ch_stub.c
 * @brief 精简五模块交付工程的符号桩实现（见 module_5ch_stub.h）。
 */
#include "module_5ch_stub.h"
#include <string.h>

bool DSO_FreqMeter_GetSnapshot(DSO_FreqMeterSnapshot *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    return false;   /* 无真实频率计：上报代码走 freq_valid:0 占位分支 */
}

uint32_t DSO_Acq_GetSampleFormat(void)
{
    return 0u;
}

uint32_t DSO_Acq_GetSampleRate(void)
{
    return 0u;
}

uint32_t DSO_Acq_GetOverrunCount(void)
{
    return 0u;
}

uint8_t DSO_Switch_UsbActive(void)
{
    return 0u;
}

uint8_t DSO_Switch_BuzzerMute(void)
{
    return 0u;
}
