#include "psx_debug.h"

#include <nds.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define DEBUG_LOG_SIZE 8192
#define DEBUG_LOG_PATH "/psx/emulator.log"

static char g_debug_buffer[DEBUG_LOG_SIZE];
static uint32_t g_debug_pos = 0;
static bool g_debug_ready = false;

void debug_init(void) {
    g_debug_pos = 0;
    g_debug_buffer[0] = '\0';
    g_debug_ready = true;

    debug_log("=== PSX Emulator Debug Log ===");
    debug_log("Boot time: %ld", (long int)0);
}

void debug_log(const char *fmt, ...) {
    if (!g_debug_ready) return;

    char tmp[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    uint32_t len = strlen(tmp);
    if (g_debug_pos + len + 2 >= DEBUG_LOG_SIZE) {
        g_debug_pos = 0;
    }

    strncpy(&g_debug_buffer[g_debug_pos], tmp, DEBUG_LOG_SIZE - g_debug_pos - 1);
    g_debug_pos += len;
    g_debug_buffer[g_debug_pos++] = '\n';
    g_debug_buffer[g_debug_pos] = '\0';
}

void debug_log_hexdump(const char *label, const void *data, uint32_t len) {
    if (!g_debug_ready) return;

    debug_log("--- %s hexdump (%lu bytes) ---", label, (unsigned long)len);

    const uint8_t *buf = (const uint8_t *)data;
    for (uint32_t i = 0; i < len; i += 16) {
        char line[80];
        char *p = line;
        p += snprintf(p, sizeof(line), "%08lx: ", (unsigned long)i);

        for (uint32_t j = 0; j < 16 && i + j < len; j++) {
            p += snprintf(p, 3, "%02x ", buf[i + j]);
        }

        *p = '\0';
        debug_log("%s", line);
    }
}

void debug_save(void) {
    if (!g_debug_ready) return;

    FILE *fp = fopen(DEBUG_LOG_PATH, "w");
    if (fp != NULL) {
        fwrite(g_debug_buffer, 1, g_debug_pos, fp);
        fclose(fp);
    }
}