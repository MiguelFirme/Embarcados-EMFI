#include "protocol.h"
#include "emfi.h"
#include "experiment.h"
#include "target.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool parse_uint(const char **cursor, uint32_t *out) {
    const char *p = *cursor;
    if (*p < '0' || *p > '9') return false;
    uint32_t value = 0;
    do {
        uint32_t digit = (uint32_t)(*p - '0');
        if (value > (UINT32_MAX - digit) / 10u) return false;
        value = value * 10u + digit;
        ++p;
    } while (*p >= '0' && *p <= '9');
    *cursor = p;
    *out = value;
    return true;
}

static bool parse_one(const char *text, uint32_t *value) {
    return parse_uint(&text, value) && *text == '\0';
}

static bool parse_four(const char *text, uint32_t values[4]) {
    for (unsigned i = 0; i < 4; ++i) {
        if (!parse_uint(&text, &values[i])) return false;
        if (i < 3 && *text++ != ' ') return false;
    }
    return *text == '\0';
}

static void set_number(const char *text, bool (*setter)(uint32_t)) {
    uint32_t value;
    if (!parse_one(text, &value) || !setter(value)) puts("ERROR INVALID_OR_BUSY");
    else puts("OK");
}

void protocol_handle_line(char *line) {
    const experiment_config_t *config = experiment_config();
    if (strcmp(line, "PING") == 0) {
        puts("OK PONG");
    } else if (strcmp(line, "STATUS") == 0) {
        const char *state = experiment_busy() ? experiment_state_name() :
                            emfi_is_armed() ? "ARMED" : experiment_state_name();
        printf("STATUS %s DELAY_US=%" PRIu32 " WIDTH_US=%" PRIu32
               " TRIGGER_TIMEOUT_MS=%" PRIu32 " RESULT_TIMEOUT_MS=%" PRIu32
               " RESET_TARGET=%u SIMULATION_MODE=1\n",
               state, config->delay_us, config->pulse_width_us,
               config->trigger_timeout_ms, config->result_timeout_ms,
               config->reset_target ? 1u : 0u);
    } else if (strcmp(line, "ARM") == 0) {
        if (experiment_busy()) puts("ERROR BUSY");
        else { emfi_arm(); puts("OK"); }
    } else if (strcmp(line, "DISARM") == 0) {
        experiment_stop();
        puts("OK");
    } else if (strcmp(line, "PULSE") == 0) {
        if (experiment_busy()) puts("ERROR BUSY");
        else if (!emfi_is_armed()) puts("ERROR NOT_ARMED");
        else if (emfi_fire(config->pulse_width_us)) {
            puts("EVENT PULSE");
            puts("OK");
        } else puts("ERROR PULSE_FAILED");
    } else if (strncmp(line, "SET WIDTH_US ", 13) == 0) {
        set_number(line + 13, experiment_set_width);
    } else if (strncmp(line, "SET DELAY_US ", 13) == 0) {
        set_number(line + 13, experiment_set_delay);
    } else if (strncmp(line, "SET TRIGGER_TIMEOUT_MS ", 23) == 0) {
        set_number(line + 23, experiment_set_trigger_timeout);
    } else if (strncmp(line, "SET RESULT_TIMEOUT_MS ", 22) == 0) {
        set_number(line + 22, experiment_set_result_timeout);
    } else if (strcmp(line, "SET RESET_TARGET 1") == 0 ||
               strcmp(line, "SET RESET_TARGET 0") == 0) {
        if (experiment_busy()) puts("ERROR BUSY");
        else { experiment_set_reset_target(line[17] == '1'); puts("OK"); }
    } else if (strcmp(line, "RESET TARGET") == 0) {
        if (experiment_busy()) puts("ERROR BUSY");
        else { target_reset(); puts("OK"); }
    } else if (strcmp(line, "RUN") == 0) {
        if (experiment_run()) puts("OK");
        else puts("ERROR BUSY");
    } else if (strncmp(line, "SWEEP START ", 12) == 0) {
        uint32_t values[4];
        if (!parse_four(line + 12, values) ||
            !experiment_start_sweep(values[0], values[1], values[2], values[3]))
            puts("ERROR INVALID_SWEEP_OR_BUSY");
        else puts("OK");
    } else {
        puts("ERROR UNKNOWN_COMMAND");
    }
}
