#ifndef ENFI_EXPERIMENT_H
#define ENFI_EXPERIMENT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EXP_IDLE, EXP_CONFIGURED, EXP_ARMED, EXP_WAIT_TRIGGER,
    EXP_DELAY, EXP_PULSE, EXP_WAIT_RESULT, EXP_COMPLETE, EXP_ERROR
} experiment_state_t;

typedef struct {
    uint32_t delay_us;
    uint32_t pulse_width_us;
    uint32_t trigger_timeout_ms;
    uint32_t result_timeout_ms;
    bool reset_target;
} experiment_config_t;

void experiment_init(void);
void experiment_tick(void);
bool experiment_busy(void);
experiment_state_t experiment_state(void);
const char *experiment_state_name(void);
const experiment_config_t *experiment_config(void);
bool experiment_set_delay(uint32_t value);
bool experiment_set_width(uint32_t value);
bool experiment_set_trigger_timeout(uint32_t value);
bool experiment_set_result_timeout(uint32_t value);
void experiment_set_reset_target(bool enabled);
bool experiment_run(void);
bool experiment_start_sweep(uint32_t start, uint32_t end, uint32_t step,
                            uint32_t repetitions);
void experiment_stop(void);

#endif
