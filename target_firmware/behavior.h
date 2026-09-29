#ifndef TARGET_BEHAVIOR_H
#define TARGET_BEHAVIOR_H

#include <stdint.h>

// Shared expected values and simulated timing. The fault value is artificial.
#define TARGET_RESULT_OK UINT32_C(0x02FB0408)
#define TARGET_RESULT_FAULT UINT32_C(0xDEADBEEF)
#define TARGET_PREPARE_US 2000u
#define TARGET_EXECUTION_US 5000u
#define TARGET_FINISH_US 3000u

static inline uint32_t target_test_sum(void) {
    uint32_t result = 0;
    for (uint32_t i = 1; i <= 10000u; ++i) result += i;
    return result;
}

#endif
