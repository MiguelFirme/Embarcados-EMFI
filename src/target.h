#ifndef ENFI_TARGET_H
#define ENFI_TARGET_H

#include <stdbool.h>
#include <stddef.h>

void target_init(void);
void target_start(void);
void target_tick(void);
bool target_take_response(char *out, size_t capacity);
void target_reset(void);

#endif
