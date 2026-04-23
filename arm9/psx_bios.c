#include "psx.h"
#include "psx_bios.h"
#include "psx_exe.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PSX_BIOS_FILE_READ   0x01
#define PSX_BIOS_FILE_WRITE 0x02
#define PSX_BIOS_FILE_CREAT 0x200
#define PSX_BIOS_FILE_TRUNC 0x400

#define PSX_BIOS_SEEK_SET 0x00
#define PSX_BIOS_SEEK_CUR 0x01
#define PSX_BIOS_SEEK_END 0x02

static PsxBiosState g_bios;

static int g_evcb_count = 8;
static int g_tcb_count = 1;
static uint32_t g_stack_top = 0x801FFFF0;

void psx_bios_init(PsxState *psx) {
    (void)psx;
    memset(&g_bios, 0, sizeof(g_bios));
    g_bios.initialized = true;
}

void psx_bios_reset(PsxState *psx) {
    (void)psx;
    for (int i = 0; i < PSX_BIOS_MAX_FILES; i++) {
        if (g_bios.files[i].buffer) {
            free(g_bios.files[i].buffer);
            g_bios.files[i].buffer = NULL;
        }
    }
    memset(&g_bios, 0, sizeof(g_bios));
    g_bios.initialized = true;
}

int psx_bios_open(PsxState *psx, const char *filename, int mode) {
    (void)psx;
    int fd;
    for (fd = 0; fd < PSX_BIOS_MAX_FILES; fd++) {
        if (!g_bios.files[fd].valid) {
            break;
        }
    }
    if (fd >= PSX_BIOS_MAX_FILES) {
        return -1;
    }
    
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        if (mode & PSX_BIOS_FILE_CREAT) {
            fp = fopen(filename, "wb");
            if (fp == NULL) {
                return -1;
            }
            fclose(fp);
            fp = fopen(filename, "rb");
        }
    }
    if (fp == NULL) {
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *buffer = (uint8_t *)malloc(size > 0 ? (size_t)size : 1);
    if (buffer == NULL) {
        fclose(fp);
        return -1;
    }
    
    if (size > 0 && fread(buffer, 1, size, fp) != (size_t)size) {
        free(buffer);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    g_bios.files[fd].buffer = buffer;
    g_bios.files[fd].size = (size_t)size;
    g_bios.files[fd].pos = 0;
    g_bios.files[fd].eof = false;
    g_bios.files[fd].valid = true;
    
    return fd;
}

int psx_bios_read(PsxState *psx, int fd, void *dst, int length) {
    (void)psx;
    if (fd < 0 || fd >= PSX_BIOS_MAX_FILES || !g_bios.files[fd].valid) {
        return -1;
    }
    
    PsxBiosFile *f = &g_bios.files[fd];
    if (f->pos >= f->size) {
        f->eof = true;
        return 0;
    }
    
    int to_read = length;
    if (f->pos + (size_t)to_read > f->size) {
        to_read = (int)(f->size - f->pos);
    }
    
    memcpy(dst, f->buffer + f->pos, to_read);
    f->pos += to_read;
    
    return to_read;
}

int psx_bios_write(PsxState *psx, int fd, const void *src, int length) {
    (void)psx;
    if (fd < 0 || fd >= PSX_BIOS_MAX_FILES || !g_bios.files[fd].valid) {
        return -1;
    }
    
    PsxBiosFile *f = &g_bios.files[fd];
    
    if (f->pos + (size_t)length > f->size) {
        size_t new_size = f->pos + length;
        uint8_t *new_buffer = (uint8_t *)realloc(f->buffer, new_size);
        if (new_buffer == NULL) {
            return -1;
        }
        f->buffer = new_buffer;
        f->size = new_size;
    }
    
    memcpy(f->buffer + f->pos, src, length);
    f->pos += length;
    
    FILE *fp = fopen("/psx/temp.bin", "wb");
    if (fp) {
        fwrite(f->buffer, 1, f->pos, fp);
        fclose(fp);
    }
    
    return length;
}

int psx_bios_close(PsxState *psx, int fd) {
    (void)psx;
    if (fd < 0 || fd >= PSX_BIOS_MAX_FILES || !g_bios.files[fd].valid) {
        return -1;
    }
    
    if (g_bios.files[fd].buffer) {
        free(g_bios.files[fd].buffer);
    }
    memset(&g_bios.files[fd], 0, sizeof(PsxBiosFile));
    
    return 0;
}

int psx_bios_lseek(PsxState *psx, int fd, int offset, int seektype) {
    (void)psx;
    if (fd < 0 || fd >= PSX_BIOS_MAX_FILES || !g_bios.files[fd].valid) {
        return -1;
    }
    
    PsxBiosFile *f = &g_bios.files[fd];
    size_t new_pos;
    
    switch (seektype) {
        case PSX_BIOS_SEEK_SET:
            new_pos = (size_t)offset;
            break;
        case PSX_BIOS_SEEK_CUR:
            new_pos = f->pos + offset;
            break;
        case PSX_BIOS_SEEK_END:
            new_pos = f->size + offset;
            break;
        default:
            return -1;
    }
    
    if (new_pos > f->size) {
        new_pos = f->size;
    }
    f->pos = new_pos;
    f->eof = (f->pos >= f->size);
    
    return (int)f->pos;
}

int psx_bios_getc(PsxState *psx, int fd) {
    uint8_t byte;
    int result = psx_bios_read(psx, fd, &byte, 1);
    return result == 1 ? (int)byte : -1;
}

int psx_bios_putc(PsxState *psx, int ch, int fd) {
    uint8_t byte = (uint8_t)ch;
    return psx_bios_write(psx, fd, &byte, 1);
}

void *psx_bios_malloc(PsxState *psx, size_t size) {
    (void)psx;
    if (!g_bios.initialized || g_bios.heap_base == NULL) {
        return NULL;
    }
    
    size_t total_size = sizeof(PsxBiosBlock) + size;
    if (g_bios.heap_used + total_size > g_bios.heap_size) {
        return NULL;
    }
    
    PsxBiosBlock *block = (PsxBiosBlock *)((uint8_t *)g_bios.heap_base + g_bios.heap_used);
    block->next = NULL;
    block->prev = NULL;
    block->size = total_size;
    
    g_bios.heap_used += total_size;
    
    return (uint8_t *)block + sizeof(PsxBiosBlock);
}

void psx_bios_free(PsxState *psx, void *ptr) {
    (void)psx;
    if (ptr == NULL) {
        return;
    }
    
    PsxBiosBlock *block = (PsxBiosBlock *)((uint8_t *)ptr - sizeof(PsxBiosBlock));
    if (block->next) {
        ((PsxBiosBlock *)block->next)->prev = block->prev;
    }
    if (block->prev) {
        ((PsxBiosBlock *)block->prev)->next = block->next;
    }
}

void *psx_bios_calloc(PsxState *psx, size_t nmemb, size_t size) {
    void *ptr = psx_bios_malloc(psx, nmemb * size);
    if (ptr) {
        memset(ptr, 0, nmemb * size);
    }
    return ptr;
}

void *psx_bios_realloc(PsxState *psx, void *ptr, size_t size) {
    if (ptr == NULL) {
        return psx_bios_malloc(psx, size);
    }
    
    PsxBiosBlock *old_block = (PsxBiosBlock *)((uint8_t *)ptr - sizeof(PsxBiosBlock));
    size_t old_size = old_block->size - sizeof(PsxBiosBlock);
    
    void *new_ptr = psx_bios_malloc(psx, size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    memcpy(new_ptr, ptr, old_size < size ? old_size : size);
    psx_bios_free(psx, ptr);
    
    return new_ptr;
}

void psx_bios_initheap(PsxState *psx, void *base, size_t size) {
    (void)psx;
    g_bios.heap_base = (uint8_t *)base;
    g_bios.heap_size = size;
    g_bios.heap_used = 0;
}

void *psx_bios_memcpy(void *dst, const void *src, size_t n) {
    return memcpy(dst, src, n);
}

void *psx_bios_memset(void *s, int c, size_t n) {
    return memset(s, c, n);
}

void *psx_bios_memmove(void *dst, const void *src, size_t n) {
    return memmove(dst, src, n);
}

int psx_bios_memcmp(const void *s1, const void *s2, size_t n) {
    return memcmp(s1, s2, n);
}

void *psx_bios_memchr(const void *s, int c, size_t n) {
    return memchr(s, c, n);
}

char *psx_bios_strcpy(char *dst, const char *src) {
    return strcpy(dst, src);
}

char *psx_bios_strncpy(char *dst, const char *src, size_t n) {
    return strncpy(dst, src, n);
}

size_t psx_bios_strlen(const char *s) {
    return strlen(s);
}

int psx_bios_strcmp(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

int psx_bios_strncmp(const char *s1, const char *s2, size_t n) {
    return strncmp(s1, s2, n);
}

char *psx_bios_strchr(const char *s, int c) {
    return strchr(s, c);
}

char *psx_bios_strrchr(const char *s, int c) {
    return strrchr(s, c);
}

int psx_bios_atoi(const char *s) {
    return atoi(s);
}

long psx_bios_atol(const char *s) {
    return atol(s);
}

int psx_bios_abs(int n) {
    return abs(n);
}

long psx_bios_labs(long n) {
    return labs(n);
}

int psx_bios_exec(PsxState *psx, const void *header, uint32_t param1, uint32_t param2) {
    (void)psx;
    (void)header;
    (void)param1;
    (void)param2;
    return 0;
}

int psx_bios_load(PsxState *psx, const char *filename, void *headerbuf) {
    (void)psx;
    return psx_bios_loadtest(psx, filename, headerbuf);
}

int psx_bios_loadtest(PsxState *psx, const char *filename, void *headerbuf) {
    (void)psx;
    int fd = psx_bios_open(psx, filename, PSX_BIOS_FILE_READ);
    if (fd < 0) {
        return -1;
    }
    
    int result = psx_bios_read(psx, fd, headerbuf, 8);
    psx_bios_close(psx, fd);
    
    if (result < 0) {
        return -1;
    }
    
    uint8_t *magic = (uint8_t *)headerbuf;
    if (magic[0] != 0x50 || magic[1] != 0x53 || magic[2] != 0x2D || magic[3] != 0x45) {
        return -2;
    }
    
    return 0;
}

int psx_bios_loadexec(PsxState *psx, const char *filename, uint32_t stackbase, uint32_t stoffset) {
    (void)psx;
    (void)filename;
    (void)stackbase;
    (void)stoffset;
    return 0;
}

void psx_bios_setconf(PsxState *psx, int num_evcb, int num_tcb, uint32_t stacktop) {
    (void)psx;
    if (num_evcb > 0) {
        g_evcb_count = num_evcb;
    }
    if (num_tcb > 0) {
        g_tcb_count = num_tcb;
    }
    if (stacktop != 0) {
        g_stack_top = stacktop;
    }
}

void psx_bios_getconf(PsxState *psx, int *num_evcb, int *num_tcb, uint32_t *stacktop) {
    (void)psx;
    if (num_evcb) {
        *num_evcb = g_evcb_count;
    }
    if (num_tcb) {
        *num_tcb = g_tcb_count;
    }
    if (stacktop) {
        *stacktop = g_stack_top;
    }
}

void psx_bios_flush_cache(PsxState *psx) {
    (void)psx;
}