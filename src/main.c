#include "emfi.h"
#include "protocol.h"
#include "experiment.h"
#include "target.h"
#include "pico/stdlib.h"
#include <stddef.h>
#include <stdio.h>

#define LINE_CAPACITY 96u

int main(void) {
    emfi_init();
    target_init();
    experiment_init();
    stdio_init_all();
    char line[LINE_CAPACITY];
    size_t length = 0;
    bool overflow = false;

    for (;;) {
        experiment_tick();
        int ch = getchar_timeout_us(50);
        if (ch == PICO_ERROR_TIMEOUT) continue;
        if (ch == '\n' || ch == '\r') {
            if (overflow) {
                puts("ERROR LINE_TOO_LONG");
            } else if (length != 0u) {
                line[length] = '\0';
                protocol_handle_line(line);
            }
            length = 0;
            overflow = false;
        } else if (ch >= 32 && ch <= 126 && !overflow) {
            if (length + 1u < LINE_CAPACITY) line[length++] = (char)ch;
            else overflow = true;
        } else if (ch < 32 || ch > 126) {
            overflow = true;
        }
    }
}
