#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "behavior.h"

#define TARGET_TRIGGER_OUT 14u
#define SOFT_RESET_IN 16u
#define TARGET_UART_TX 4u
#define TARGET_UART_RX 5u
#define TARGET_EXECUTION_OUT 18u

static uint32_t test_function(void) {
    return target_test_sum();
}

static void run_test(void) {
    gpio_put(TARGET_TRIGGER_OUT, 1);
    busy_wait_us_32(TARGET_PREPARE_US);
    gpio_put(TARGET_EXECUTION_OUT, 1);
    uint32_t result = test_function();
    // Hold the execution marker for a measurable teaching window.
    uint64_t execution_start = time_us_64();
    while (time_us_64() - execution_start < TARGET_EXECUTION_US) {
        tight_loop_contents();
    }
    gpio_put(TARGET_EXECUTION_OUT, 0);
    busy_wait_us_32(TARGET_FINISH_US);
    gpio_put(TARGET_TRIGGER_OUT, 0);
    char reply[24];
    snprintf(reply, sizeof(reply), "RESULT:%08" PRIX32 "\n", result);
    uart_puts(uart1, reply);
}

int main(void) {
    gpio_init(TARGET_TRIGGER_OUT);
    gpio_put(TARGET_TRIGGER_OUT, 0);
    gpio_set_dir(TARGET_TRIGGER_OUT, GPIO_OUT);
    gpio_init(TARGET_EXECUTION_OUT);
    gpio_put(TARGET_EXECUTION_OUT, 0);
    gpio_set_dir(TARGET_EXECUTION_OUT, GPIO_OUT);
    gpio_init(SOFT_RESET_IN);
    gpio_set_dir(SOFT_RESET_IN, GPIO_IN);
    gpio_pull_up(SOFT_RESET_IN);
    uart_init(uart1, 115200);
    gpio_set_function(TARGET_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(TARGET_UART_RX, GPIO_FUNC_UART);

    char command[12];
    unsigned length = 0;
    for (;;) {
        if (!gpio_get(SOFT_RESET_IN)) {
            gpio_put(TARGET_TRIGGER_OUT, 0);
            gpio_put(TARGET_EXECUTION_OUT, 0);
            length = 0;
            while (uart_is_readable(uart1)) (void)uart_getc(uart1);
            continue;
        }
        if (!uart_is_readable(uart1)) continue;
        char ch = (char)uart_getc(uart1);
        if (ch == '\n' || ch == '\r') {
            command[length] = '\0';
            if (strcmp(command, "RUN") == 0) run_test();
            length = 0;
        } else if (ch >= 32 && ch <= 126) {
            if (length + 1u < sizeof(command)) command[length++] = ch;
            else length = 0;
        } else length = 0;
    }
}
