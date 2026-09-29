#include "emfi.h"
#include "pins.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#if !defined(SIMULATION_MODE) || SIMULATION_MODE != 1
#error "This milestone only supports SIMULATION_MODE=1 (3.3 V GPIO output)"
#endif

static bool armed;

void emfi_init(void) {
    gpio_init(EMFI_TRIGGER_OUT);
    gpio_put(EMFI_TRIGGER_OUT, 0);
    gpio_set_dir(EMFI_TRIGGER_OUT, GPIO_OUT);
    armed = false;
}

void emfi_arm(void) {
    gpio_put(EMFI_TRIGGER_OUT, 0);
    armed = true;
}

void emfi_disarm(void) {
    gpio_put(EMFI_TRIGGER_OUT, 0);
    armed = false;
}

bool emfi_is_armed(void) { return armed; }

bool emfi_fire(uint32_t width_us) {
    if (!armed || width_us == 0u) return false;
    gpio_put(EMFI_TRIGGER_OUT, 1);
    sleep_us(width_us);
    gpio_put(EMFI_TRIGGER_OUT, 0);
    // A manual pulse consumes the arm operation; another pulse needs ARM.
    armed = false;
    return true;
}
