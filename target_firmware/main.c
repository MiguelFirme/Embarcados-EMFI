#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define TARGET_TRIGGER_OUT 14u
#define SOFT_RESET_IN 16u
#define TARGET_UART_TX 4u
#define TARGET_UART_RX 5u
#define TEST_WINDOW_US 10000u

static uint32_t test_function(void) {
    // 1 + ... + 10000 = 50005000 = 0x02FB0408.
    volatile uint32_t result = 0;
    for (uint32_t i = 1; i <= 10000u; ++i) result += i;
    return result;
}

static void run_test(void) {
    gpio_put(TARGET_TRIGGER_OUT, 1);
    uint32_t result = test_function();
    // Gives the controller a 10 ms test window for educational measurements.
    // This delay is part of the test fixture, not an injection timing guarantee.
    busy_wait_us_32(TEST_WINDOW_US);
    gpio_put(TARGET_TRIGGER_OUT, 0);
    char reply[24];
    snprintf(reply, sizeof(reply), "RESULT:%08" PRIX32 "\n", result);
    uart_puts(uart1, reply);
}

int main(void) {
    gpio_init(TARGET_TRIGGER_OUT);
    gpio_put(TARGET_TRIGGER_OUT, 0);
    gpio_set_dir(TARGET_TRIGGER_OUT, GPIO_OUT);
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
