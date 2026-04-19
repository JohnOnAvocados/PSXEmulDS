#ifndef PSX_DEBUG_H
#define PSX_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

void debug_init(void);
void debug_log(const char *fmt, ...);
void debug_log_hexdump(const char *label, const void *data, uint32_t len);
void debug_save(void);

#endif