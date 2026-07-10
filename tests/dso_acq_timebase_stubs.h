#ifndef TESTS_DSO_ACQ_TIMEBASE_STUBS_H
#define TESTS_DSO_ACQ_TIMEBASE_STUBS_H

#include <stdbool.h>
#include <stdint.h>

void Test_AcqStub_Reset(void);
void Test_AcqStub_SetLaneCount(uint8_t lane_count);
void Test_AcqStub_SetTimClkHz(uint32_t tim_clk_hz);
void Test_AcqStub_SetRuntimeInfo(uint32_t tim_arr,
                                 uint32_t sample_rate_hz,
                                 bool enable);

#endif /* TESTS_DSO_ACQ_TIMEBASE_STUBS_H */
