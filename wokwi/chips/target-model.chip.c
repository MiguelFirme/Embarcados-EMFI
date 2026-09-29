#include "wokwi-api.h"
#include "../../target_firmware/behavior.h"
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Wokwi cannot connect two simulated MCUs in one circuit. This chip models
// only the target's external behavior; it never reads controller variables.
typedef struct {
    pin_t trigger;
    pin_t fire;
    pin_t execution;
    pin_t reset;
    pin_t hold;
    uart_dev_t uart;
    timer_t start_execution;
    timer_t end_execution;
    timer_t end_trial;
    char command[12];
    unsigned command_length;
    bool running;
    bool injected;
    bool suppress_response;
    uint8_t reply[24];
} target_chip_t;

static void stop_trial(target_chip_t *chip) {
    timer_stop(chip->start_execution);
    timer_stop(chip->end_execution);
    timer_stop(chip->end_trial);
    pin_write(chip->trigger, LOW);
    pin_write(chip->execution, LOW);
    chip->running = false;
    chip->injected = false;
}

static void on_execution_start(void *user_data) {
    target_chip_t *chip = user_data;
    if (chip->running) pin_write(chip->execution, HIGH);
}

static void on_execution_end(void *user_data) {
    target_chip_t *chip = user_data;
    pin_write(chip->execution, LOW);
}

static void on_trial_end(void *user_data) {
    target_chip_t *chip = user_data;
    bool injected = chip->injected;
    bool suppress_response = chip->suppress_response;
    stop_trial(chip);
    if (!suppress_response) {
        uint32_t value = injected ? TARGET_RESULT_FAULT : TARGET_RESULT_OK;
        snprintf((char *)chip->reply, sizeof(chip->reply),
                 "RESULT:%08" PRIX32 "\n", value);
        uart_write(chip->uart, chip->reply,
                   (uint32_t)strlen((char *)chip->reply));
    }
}

static void on_pin_change(void *user_data, pin_t pin, uint32_t value) {
    target_chip_t *chip = user_data;
    if (pin == chip->reset && value == LOW) {
        stop_trial(chip);
        chip->command_length = 0;
    } else if (pin == chip->fire && value == HIGH && chip->running &&
               pin_read(chip->execution) == HIGH) {
        // Artificial fault: FIRE overlaps the execution marker.
        chip->injected = true;
    }
}

static void begin_trial(target_chip_t *chip) {
    if (chip->running || pin_read(chip->reset) == LOW) return;
    chip->running = true;
    chip->injected = false;
    // Hold the button at RUN to produce a result timeout with a normal trigger.
    chip->suppress_response = pin_read(chip->hold) == LOW;
    pin_write(chip->trigger, HIGH);
    timer_start(chip->start_execution, TARGET_PREPARE_US, false);
    timer_start(chip->end_execution,
                TARGET_PREPARE_US + TARGET_EXECUTION_US, false);
    timer_start(chip->end_trial,
                TARGET_PREPARE_US + TARGET_EXECUTION_US + TARGET_FINISH_US,
                false);
}

static void on_uart_rx_data(void *user_data, uint8_t byte) {
    target_chip_t *chip = user_data;
    if (byte == '\n' || byte == '\r') {
        chip->command[chip->command_length] = '\0';
        if (strcmp(chip->command, "RUN") == 0) begin_trial(chip);
        chip->command_length = 0;
    } else if (byte >= 32 && byte <= 126) {
        if (chip->command_length + 1u < sizeof(chip->command))
            chip->command[chip->command_length++] = (char)byte;
        else chip->command_length = 0;
    } else chip->command_length = 0;
}

void chip_init(void) {
    target_chip_t *chip = calloc(1, sizeof(*chip));
    chip->trigger = pin_init("TRIGGER", OUTPUT_LOW);
    chip->fire = pin_init("FIRE", INPUT_PULLDOWN);
    chip->execution = pin_init("EXEC", OUTPUT_LOW);
    chip->reset = pin_init("RESET", INPUT_PULLUP);
    chip->hold = pin_init("HOLD", INPUT_PULLUP);

    const pin_watch_config_t watch = {
        .edge = BOTH, .pin_change = on_pin_change, .user_data = chip
    };
    pin_watch(chip->fire, &watch);
    pin_watch(chip->reset, &watch);

    const timer_config_t start = {
        .callback = on_execution_start, .user_data = chip
    };
    const timer_config_t end = {
        .callback = on_execution_end, .user_data = chip
    };
    const timer_config_t done = {
        .callback = on_trial_end, .user_data = chip
    };
    chip->start_execution = timer_init(&start);
    chip->end_execution = timer_init(&end);
    chip->end_trial = timer_init(&done);

    const uart_config_t uart_config = {
        .rx = pin_init("RX", INPUT),
        .tx = pin_init("TX", INPUT_PULLUP),
        .baud_rate = 115200,
        .rx_data = on_uart_rx_data,
        .user_data = chip
    };
    chip->uart = uart_init(&uart_config);
}
