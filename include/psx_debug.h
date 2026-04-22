#ifndef PSX_DEBUG_H
#define PSX_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_LOG_MESSAGE 256
#define MAX_LOG_ENTRIES 100

void debug_init(void);
void debug_set_fat_ready(bool ready);
void debug_log(const char *fmt, ...);
void debug_log_hexdump(const char *label, const void *data, uint32_t len);
void debug_error(const char *fmt, ...);
void debug_warning(const char *fmt, ...);
void debug_info(const char *fmt, ...);
void debug_flush(void);
void debug_save(void);
void debug_clear_logfile(void);
const char* debug_get_log_path(void);

#endif