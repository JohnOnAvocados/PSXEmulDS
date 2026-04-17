#include "psx_exe.h"

#include <string.h>

enum {
    PSX_EXE_HEADER_SIZE = 0x800,
};

static uint32_t read_le32(const uint8_t *src) {
    return (uint32_t)src[0]
        | ((uint32_t)src[1] << 8)
        | ((uint32_t)src[2] << 16)
        | ((uint32_t)src[3] << 24);
}

bool psx_exe_parse(const uint8_t *data, size_t size, PsxExeInfo *out_info) {
    if (data == NULL || out_info == NULL || size < PSX_EXE_HEADER_SIZE) {
        return false;
    }

    if (memcmp(data, "PS-X EXE", 8) != 0) {
        return false;
    }

    memset(out_info, 0, sizeof(*out_info));
    out_info->entry_pc = read_le32(&data[0x10]);
    out_info->global_pointer = read_le32(&data[0x14]);
    out_info->load_addr = read_le32(&data[0x18]);
    out_info->load_size = read_le32(&data[0x1C]);
    out_info->stack_addr = read_le32(&data[0x30]);
    out_info->stack_size = read_le32(&data[0x34]);

    if (size < PSX_EXE_HEADER_SIZE + out_info->load_size) {
        return false;
    }

    return true;
}

bool psx_exe_load(PsxState *psx, const uint8_t *data, size_t size, PsxExeInfo *out_info) {
    PsxExeInfo info;

    if (psx == NULL) {
        return false;
    }

    if (!psx_exe_parse(data, size, &info)) {
        return false;
    }

    if (info.load_addr + info.load_size > psx->ram_size) {
        return false;
    }

    memset(psx->ram, 0, psx->ram_size);
    memset(psx->scratchpad, 0, sizeof(psx->scratchpad));
    memset(psx->io_regs, 0, sizeof(psx->io_regs));
    memcpy(&psx->ram[info.load_addr], &data[PSX_EXE_HEADER_SIZE], info.load_size);

    memset(&psx->cpu, 0, sizeof(psx->cpu));
    psx->cpu.pc = info.entry_pc;
    psx->cpu.next_pc = info.entry_pc + 4;
    psx->cpu.gpr[28] = info.global_pointer;

    if (info.stack_addr != 0) {
        psx->cpu.gpr[29] = info.stack_addr + info.stack_size;
        psx->cpu.gpr[30] = psx->cpu.gpr[29];
    }

    psx->halted = false;
    psx->cycles = 0;
    psx->last_pc = 0;
    psx->last_opcode = 0;
    psx->halt_pc = 0;
    strncpy(psx->halt_reason, "ps-x exe loaded", sizeof(psx->halt_reason) - 1);
    psx->halt_reason[sizeof(psx->halt_reason) - 1] = '\0';
    strncpy(psx->last_disasm, "none", sizeof(psx->last_disasm) - 1);
    psx->last_disasm[sizeof(psx->last_disasm) - 1] = '\0';
    psx->trace_pos = 0;
    psx->trace_count = 0;
    memset(psx->trace, 0, sizeof(psx->trace));
    psx->last_io_addr = 0;
    psx->last_io_value = 0;
    psx->last_io_write = false;

    if (out_info != NULL) {
        *out_info = info;
    }

    return true;
}
