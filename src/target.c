#include "target.h"
#include "pins.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include <string.h>

#define RESPONSE_CAPACITY 40u
static char response[RESPONSE_CAPACITY];
static size_t response_length;
static bool response_ready;
static bool discard_line;

void target_init(void) {
    uart_init(uart1, 115200);
    gpio_set_function(TARGET_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(TARGET_UART_RX, GPIO_FUNC_UART);
    gpio_init(TARGET_RESET_OUT);
    gpio_put(TARGET_RESET_OUT, 1);
    gpio_set_dir(TARGET_RESET_OUT, GPIO_OUT);
    response_length = 0;
    response_ready = false;
    discard_line = false;
}

void target_start(void) {
    // Discard stale bytes from a prior trial before asking the target to run.
    while (uart_is_readable(uart1)) (void)uart_getc(uart1);
    response_length = 0;
    response_ready = false;
    discard_line = false;
    uart_puts(uart1, "RUN\n");
}

void target_tick(void) {
    while (uart_is_readable(uart1)) {
        char ch = (char)uart_getc(uart1);
        if (ch == '\n' || ch == '\r') {
            if (response_length != 0 && !discard_line) {
                response[response_length] = '\0';
                response_ready = true;
                return;
            }
            response_length = 0;
            discard_line = false;
        } else if (ch >= 32 && ch <= 126 && !discard_line) {
            if (response_length + 1u < RESPONSE_CAPACITY) response[response_length++] = ch;
            else discard_line = true;
        } else {
            discard_line = true;
        }
    }
}

bool target_take_response(char *out, size_t capacity) {
    if (!response_ready || capacity <= response_length) return false;
    memcpy(out, response, response_length + 1u);
    response_ready = false;
    response_length = 0;
    return true;
}

void target_reset(void) {
    // This is a dedicated active-low *software reset request* to our test
    // firmware, not a direct connection to the Pico RUN or power pin.
    gpio_put(TARGET_RESET_OUT, 0);
    sleep_ms(10);
    gpio_put(TARGET_RESET_OUT, 1);
}
