#ifndef ENFI_EMFI_H
#define ENFI_EMFI_H

#include <stdbool.h>
#include <stdint.h>

// A 3.3 V logic GPIO interface only. Never connect a high-voltage stage directly.
void emfi_init(void);
void emfi_arm(void);
void emfi_disarm(void);
bool emfi_is_armed(void);
bool emfi_fire(uint32_t width_us);

#endif
