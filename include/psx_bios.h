#ifndef PSX_BIOS_H
#define PSX_BIOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PSX_BIOS_HEAP_START 0x00010000
#define PSX_BIOS_HEAP_SIZE 0x00100000

#define PSX_BIOS_MAX_FILES 16
#define PSX_BIOS_MAX_PATH 128

typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t pos;
    bool eof;
    bool valid;
} PsxBiosFile;

typedef struct {
    void *next;
    void *prev;
    size_t size;
} PsxBiosBlock;

typedef struct {
    PsxBiosFile files[PSX_BIOS_MAX_FILES];
    uint8_t *heap_base;
    size_t heap_size;
    size_t heap_used;
    bool initialized;
} PsxBiosState;

void psx_bios_init(PsxState *psx);
void psx_bios_reset(PsxState *psx);

int psx_bios_open(PsxState *psx, const char *filename, int mode);
int psx_bios_read(PsxState *psx, int fd, void *dst, int length);
int psx_bios_write(PsxState *psx, int fd, const void *src, int length);
int psx_bios_close(PsxState *psx, int fd);
int psx_bios_lseek(PsxState *psx, int fd, int offset, int seektype);
int psx_bios_getc(PsxState *psx, int fd);
int psx_bios_putc(PsxState *psx, int ch, int fd);

void *psx_bios_malloc(PsxState *psx, size_t size);
void psx_bios_free(PsxState *psx, void *ptr);
void *psx_bios_calloc(PsxState *psx, size_t nmemb, size_t size);
void *psx_bios_realloc(PsxState *psx, void *ptr, size_t size);
void psx_bios_initheap(PsxState *psx, void *base, size_t size);

void *psx_bios_memcpy(void *dst, const void *src, size_t n);
void *psx_bios_memset(void *s, int c, size_t n);
void *psx_bios_memmove(void *dst, const void *src, size_t n);
int psx_bios_memcmp(const void *s1, const void *s2, size_t n);
void *psx_bios_memchr(const void *s, int c, size_t n);

char *psx_bios_strcpy(char *dst, const char *src);
char *psx_bios_strncpy(char *dst, const char *src, size_t n);
size_t psx_bios_strlen(const char *s);
int psx_bios_strcmp(const char *s1, const char *s2);
int psx_bios_strncmp(const char *s1, const char *s2, size_t n);
char *psx_bios_strchr(const char *s, int c);
char *psx_bios_strrchr(const char *s, int c);

int psx_bios_atoi(const char *s);
long psx_bios_atol(const char *s);
int psx_bios_abs(int n);
long psx_bios_labs(long n);

int psx_bios_exec(PsxState *psx, const void *header, uint32_t param1, uint32_t param2);
int psx_bios_load(PsxState *psx, const char *filename, void *headerbuf);
int psx_bios_loadtest(PsxState *psx, const char *filename, void *headerbuf);
int psx_bios_loadexec(PsxState *psx, const char *filename, uint32_t stackbase, uint32_t stoffset);

void psx_bios_setconf(PsxState *psx, int num_evcb, int num_tcb, uint32_t stacktop);
void psx_bios_getconf(PsxState *psx, int *num_evcb, int *num_tcb, uint32_t *stacktop);

void psx_bios_flush_cache(PsxState *psx);

void psx_handle_bios_call(PsxState *psx);

#endif