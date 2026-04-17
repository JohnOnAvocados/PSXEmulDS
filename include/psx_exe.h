#ifndef PSX_EXE_H
#define PSX_EXE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "psx.h"

typedef struct {
    uint32_t entry_pc;
    uint32_t global_pointer;
    uint32_t load_addr;
    uint32_t load_size;
    uint32_t stack_addr;
    uint32_t stack_size;
} PsxExeInfo;

bool psx_exe_parse(const uint8_t *data, size_t size, PsxExeInfo *out_info);
bool psx_exe_load(PsxState *psx, const uint8_t *data, size_t size, PsxExeInfo *out_info);

#endif
