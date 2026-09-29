#include "experiment.h"
#include "emfi.h"
#include "pins.h"
#include "target.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define MAX_DELAY_US 1000000u
#define MAX_WIDTH_US 1000000u
#define MAX_TIMEOUT_MS 60000u
#define MAX_SWEEP_TRIALS 10000u
#define EXPECTED_RESPONSE "RESULT:02FB0408"

static experiment_config_t config = {0u, 10u, 500u, 500u, false};
static experiment_state_t state = EXP_IDLE;
static volatile bool trigger_seen;
static volatile uint64_t trigger_at_us;
static uint64_t trial_at_us;
static uint64_t pulse_at_us;
static uint32_t experiment_id;
static bool sweep_active;
static uint32_t sweep_end;
static uint32_t sweep_step;
static uint32_t sweep_repetitions;
static uint32_t sweep_repetition;
static uint32_t sweep_delay;

static void trigger_irq(uint gpio, uint32_t events) {
    if (gpio == TARGET_TRIGGER_IN && (events & GPIO_IRQ_EDGE_RISE) &&
        state == EXP_WAIT_TRIGGER && !trigger_seen) {
        trigger_at_us = time_us_64();
        trigger_seen = true;
    }
}

void experiment_init(void) {
    gpio_init(TARGET_TRIGGER_IN);
    gpio_set_dir(TARGET_TRIGGER_IN, GPIO_IN);
    gpio_pull_down(TARGET_TRIGGER_IN);
    gpio_set_irq_enabled_with_callback(TARGET_TRIGGER_IN, GPIO_IRQ_EDGE_RISE,
                                       true, &trigger_irq);
}

bool experiment_busy(void) {
    return sweep_active || state == EXP_WAIT_TRIGGER || state == EXP_DELAY ||
           state == EXP_PULSE || state == EXP_WAIT_RESULT;
}

experiment_state_t experiment_state(void) { return state; }
const experiment_config_t *experiment_config(void) { return &config; }

const char *experiment_state_name(void) {
    static const char *names[] = {
        "IDLE", "CONFIGURED", "ARMED", "WAIT_TRIGGER", "DELAY",
        "PULSE", "WAIT_RESULT", "COMPLETE", "ERROR"
    };
    return names[state];
}

bool experiment_set_delay(uint32_t value) {
    if (experiment_busy() || value > MAX_DELAY_US) return false;
    config.delay_us = value;
    state = EXP_CONFIGURED;
    return true;
}
bool experiment_set_width(uint32_t value) {
    if (experiment_busy() || value == 0 || value > MAX_WIDTH_US) return false;
    config.pulse_width_us = value;
    state = EXP_CONFIGURED;
    return true;
}
bool experiment_set_trigger_timeout(uint32_t value) {
    if (experiment_busy() || value == 0 || value > MAX_TIMEOUT_MS) return false;
    config.trigger_timeout_ms = value;
    state = EXP_CONFIGURED;
    return true;
}
bool experiment_set_result_timeout(uint32_t value) {
    if (experiment_busy() || value == 0 || value > MAX_TIMEOUT_MS) return false;
    config.result_timeout_ms = value;
    state = EXP_CONFIGURED;
    return true;
}
void experiment_set_reset_target(bool enabled) { config.reset_target = enabled; }

static void launch_trial(void) {
    if (config.reset_target) target_reset();
    emfi_arm();
    uint32_t saved = save_and_disable_interrupts();
    trigger_seen = false;
    state = EXP_WAIT_TRIGGER;
    restore_interrupts(saved);
    trial_at_us = time_us_64();
    ++experiment_id;
    target_start();
}

bool experiment_run(void) {
    if (experiment_busy() || sweep_active) return false;
    sweep_active = false;
    launch_trial();
    return true;
}

bool experiment_start_sweep(uint32_t start, uint32_t end, uint32_t step,
                            uint32_t repetitions) {
    if (experiment_busy() || sweep_active || step == 0 || repetitions == 0 ||
        start > end || end > MAX_DELAY_US) return false;
    uint64_t points = ((uint64_t)end - start) / step + 1u;
    if (points * repetitions > MAX_SWEEP_TRIALS) return false;
    sweep_active = true;
    sweep_end = end;
    sweep_step = step;
    sweep_repetitions = repetitions;
    sweep_repetition = 0;
    sweep_delay = start;
    config.delay_us = start;
    launch_trial();
    return true;
}

void experiment_stop(void) {
    emfi_disarm();
    sweep_active = false;
    state = EXP_IDLE;
    uint32_t saved = save_and_disable_interrupts();
    trigger_seen = false;
    restore_interrupts(saved);
}

static const char *classify(const char *response) {
    if (strcmp(response, EXPECTED_RESPONSE) == 0) return "OK";
    if (strcmp(response, "RESET") == 0) return "RESET";
    if (strncmp(response, "RESULT:", 7) == 0) return "FAULT";
    return "UNKNOWN";
}

static void finish_trial(const char *result, const char *response) {
    emfi_disarm();
    uint64_t elapsed = time_us_64() - trial_at_us;
    printf("RESULT %05" PRIu32 " %" PRIu32 " %" PRIu32 " %s %" PRIu64 " %s\n",
           experiment_id, config.delay_us, config.pulse_width_us,
           result, elapsed, response);
    state = EXP_COMPLETE;
    if (!sweep_active) return;
    if (++sweep_repetition >= sweep_repetitions) {
        sweep_repetition = 0;
        if (sweep_step > sweep_end - sweep_delay) {
            sweep_active = false;
            puts("SWEEP COMPLETE");
            return;
        }
        sweep_delay += sweep_step;
    }
    config.delay_us = sweep_delay;
}

void experiment_tick(void) {
    target_tick();
    uint64_t now = time_us_64();
    if (state == EXP_COMPLETE && sweep_active) {
        launch_trial();
    } else if (state == EXP_WAIT_TRIGGER) {
        bool seen;
        uint64_t at;
        uint32_t saved = save_and_disable_interrupts();
        seen = trigger_seen;
        at = trigger_at_us;
        restore_interrupts(saved);
        if (seen) {
            pulse_at_us = at + config.delay_us;
            state = EXP_DELAY;
        } else if (now - trial_at_us >= (uint64_t)config.trigger_timeout_ms * 1000u) {
            finish_trial("TIMEOUT", "-");
        }
    }
    if (state == EXP_DELAY) {
        char early_response[40];
        if (target_take_response(early_response, sizeof(early_response))) {
            // A response before the planned pulse means the test window ended.
            puts("EVENT TRIGGER");
            finish_trial("UNKNOWN", early_response);
            return;
        }
        if (now >= pulse_at_us) {
            state = EXP_PULSE;
            if (emfi_fire(config.pulse_width_us)) {
                puts("EVENT TRIGGER");
                puts("EVENT PULSE");
                pulse_at_us = time_us_64();
                state = EXP_WAIT_RESULT;
            } else {
                state = EXP_ERROR;
                finish_trial("UNKNOWN", "PULSE_FAILED");
            }
        }
    }
    if (state == EXP_WAIT_RESULT) {
        char response[40];
        if (target_take_response(response, sizeof(response))) {
            finish_trial(classify(response), response);
        } else if (now - pulse_at_us >= (uint64_t)config.result_timeout_ms * 1000u) {
            finish_trial("TIMEOUT", "-");
        }
    }
}
