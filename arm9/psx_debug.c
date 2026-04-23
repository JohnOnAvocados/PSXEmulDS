#include "psx_debug.h"

#include <nds.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#define DEBUG_LOG_SIZE 16384
#define DEBUG_LOG_PATH "/PSX/logs.txt"
#define MAX_LOG_ENTRY 256

static char g_debug_buffer[DEBUG_LOG_SIZE];
static uint32_t g_debug_pos = 0;
static bool g_debug_ready = false;
static bool g_fat_ready = false;
static uint32_t g_boot_time = 0;

void debug_init(void) {
    g_debug_pos = 0;
    g_debug_buffer[0] = '\0';
    g_debug_ready = true;
    g_boot_time = 0;
    
    debug_log("========================================");
    debug_log("PSXEmulDS Debug Log");
    debug_log("========================================");
}

void debug_set_fat_ready(bool ready) {
    g_fat_ready = ready;
    if (ready) {
        debug_log("DEBUG: FAT filesystem ready");
    } else {
        debug_log("DEBUG: FAT filesystem NOT available");
    }
}

void debug_log(const char *fmt, ...) {
    if (!g_debug_ready) return;

    char tmp[MAX_LOG_ENTRY];
    va_list args;
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    uint32_t len = strlen(tmp);
    if (g_debug_pos + len + 4 >= DEBUG_LOG_SIZE) {
        g_debug_pos = 0;
    }

    strncpy(&g_debug_buffer[g_debug_pos], tmp, DEBUG_LOG_SIZE - g_debug_pos - 1);
    g_debug_pos += len;
    g_debug_buffer[g_debug_pos++] = '\r';
    g_debug_buffer[g_debug_pos++] = '\n';
    g_debug_buffer[g_debug_pos] = '\0';
}

void debug_error(const char *fmt, ...) {
    if (!g_debug_ready) return;

    char tmp[MAX_LOG_ENTRY];
    char full_msg[MAX_LOG_ENTRY + 32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    snprintf(full_msg, sizeof(full_msg), "[ERROR] %s", tmp);
    debug_log("%s", full_msg);
}

void debug_warning(const char *fmt, ...) {
    if (!g_debug_ready) return;

    char tmp[MAX_LOG_ENTRY];
    char full_msg[MAX_LOG_ENTRY + 32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    snprintf(full_msg, sizeof(full_msg), "[WARN] %s", tmp);
    debug_log("%s", full_msg);
}

void debug_info(const char *fmt, ...) {
    if (!g_debug_ready) return;

    char tmp[MAX_LOG_ENTRY];
    char full_msg[MAX_LOG_ENTRY + 32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    snprintf(full_msg, sizeof(full_msg), "[INFO] %s", tmp);
    debug_log("%s", full_msg);
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
             p += snprintf(p, 4, "%02x ", buf[i + j]);
        }

        *p = '\0';
        debug_log("%s", line);
    }
}

void debug_flush(void) {
    if (!g_debug_ready || !g_fat_ready) return;

    FILE *fp = fopen(DEBUG_LOG_PATH, "w");
    if (fp != NULL) {
        fwrite(g_debug_buffer, 1, g_debug_pos, fp);
        fclose(fp);
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

void debug_clear_logfile(void) {
    if (!g_fat_ready) return;

    FILE *fp = fopen(DEBUG_LOG_PATH, "w");
    if (fp != NULL) {
        fprintf(fp, "=== PSXEmulDS Log Cleared ===\n");
        fclose(fp);
    }
    
    g_debug_pos = 0;
    g_debug_buffer[0] = '\0';
}

const char* debug_get_log_path(void) {
    return DEBUG_LOG_PATH;
}