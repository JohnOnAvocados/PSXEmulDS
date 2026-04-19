#include "psx.h"
#include "psx_gpu.h"
#include "psx_dma.h"
#include "psx_cdrom.h"
#include "psx_slot2.h"

#include <stdio.h>
#include <string.h>

// Note: Memory constants are now defined in psx.h

#define PSX_GPU_BASE_ADDR 0x1F801810
#define PSX_CDROM_BASE_ADDR 0x1F801800
#define PSX_DMA_BASE_ADDR 0x1F801080

static const char *const psx_reg_names[32] = {
    "zr", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

static inline uint32_t psx_to_physical(uint32_t addr) {
    // Handle KSEG0 and KSEG1 (cached and uncached kernel spaces)
    if (addr >= 0x80000000 && addr < 0xA0000000) { // KSEG0
        return addr & 0x007FFFFF;
    }
    if (addr >= 0xA0000000) { // KSEG1 and KSEG2
        return addr & 0x007FFFFF;
    }
    // KUSEG (user space) - same physical mapping as KSEG0/KSEG1
    return addr & 0x007FFFFF;
}

static inline uint32_t psx_translate_ram(const PsxState *psx, uint32_t addr) {
    uint32_t phys = addr & 0x007FFFFF;
    // Handle RAM mirroring: 2MB can mirror to first 8MB
    // For simplicity, allow access to full 8MB region if RAM >= 2MB
    if (phys < 0x00800000 && psx->ram_size >= 0x00200000) {
        return phys % (uint32_t)psx->ram_size;
    }
    // Direct access without mirroring for smaller RAM
    if (psx->ram_size > 0 && phys < (uint32_t)psx->ram_size) {
        return phys;
    }
    return UINT32_MAX;
}

static inline uint32_t psx_translate_scratchpad(uint32_t addr) {
    uint32_t phys = addr & 0x007FFFFF;
    if (phys >= 0x1F800000 && phys < 0x1F800000 + PSX_SCRATCHPAD_SIZE) {
        return phys - 0x1F800000;
    }
    return UINT32_MAX;
}

static inline uint32_t psx_translate_io(uint32_t addr) {
    uint32_t phys = addr & 0x007FFFFF;
    if (phys >= 0x1F801000 && phys < 0x1F801000 + PSX_IO_SIZE) {
        return phys - 0x1F801000;
    }
    return UINT32_MAX;
}

static inline uint32_t psx_translate_bios(uint32_t addr) {
    uint32_t phys = addr & 0x007FFFFF;
    if (phys >= 0x1FC00000 && phys < 0x1FC00000 + PSX_BIOS_SIZE) {
        return phys - 0x1FC00000;
    }
    return UINT32_MAX;
}

static void psx_note_io(PsxState *psx, uint32_t addr, uint32_t value, bool is_write) {
    psx->last_io_addr = psx_to_physical(addr);
    psx->last_io_value = value;
    psx->last_io_write = is_write;
}

static void psx_halt(PsxState *psx, uint32_t pc, const char *reason) {
    psx->halted = true;
    psx->halt_pc = pc;
    strncpy(psx->halt_reason, reason, sizeof(psx->halt_reason) - 1);
    psx->halt_reason[sizeof(psx->halt_reason) - 1] = '\0';
    
    if (psx->test_mode) {
        psx_record_error(psx, 0x0001, reason);
    }
}

void psx_trigger_irq(PsxState *psx, uint16_t irq_mask) {
    psx->pending_irqs |= irq_mask;
    // Set the hardware interrupt pending bit in COP0 Cause register
    psx->cpu.cop0[13] |= (irq_mask << 8) | PSX_COP0_CAUSE_IP;
}

void psx_acknowledge_irq(PsxState *psx, uint16_t irq_mask) {
    psx->pending_irqs &= ~irq_mask;
    if (psx->pending_irqs == 0) {
        // Clear all hardware interrupt pending bits except SW
        psx->cpu.cop0[13] &= ~PSX_COP0_CAUSE_IP;
    }
}

// Exception handling
static void psx_raise_exception(PsxState *psx, uint32_t exc_code, uint32_t addr) {
    // Save EPC (checking for delay slot)
    psx->cpu.cop0[14] = psx->cpu.pc;  // EPC
    if (psx->cpu.in_delay_slot) {
        psx->cpu.cop0[14] = psx->cpu.delay_slot;
        psx->cpu.cop0[13] |= PSX_COP0_CAUSE_BD;  // Set BD bit
    }
    
    // Set exception code in Cause register
    psx->cpu.cop0[13] = (psx->cpu.cop0[13] & ~PSX_COP0_CAUSE_EXC) | (exc_code << 2);
    
    // Set BadVAddr if memory exception
    if (exc_code == PSX_EXC_ADEL || exc_code == PSX_EXC_ADES) {
        psx->cpu.cop0[8] = addr;  // BadVAddr
    }
    
    // Set EXL bit in Status to force kernel mode
    psx->cpu.cop0[12] |= PSX_COP0_SR_EXL;
    
    // Jump to exception vector (0x80000080)
    psx->cpu.pc = 0x80000080;
    psx->cpu.next_pc = psx->cpu.pc + 4;
    psx->cpu.in_delay_slot = false;
}

// Check for unmapped memory access
static bool psx_check_memory_error(PsxState *psx, uint32_t addr, bool is_write) {
    uint32_t phys = addr & 0x007FFFFF;
    
    // Unused memory regions cause bus error
    if (phys >= 0x00800000 && phys < 0x1F800000) {
        // Expansion region - check if nothing mapped
        return true;
    }
    if (phys >= 0x1F800400 && phys < 0x1F801000) {
        // Gap between scratchpad and I/O
        return true;
    }
    if (phys >= 0x1F802000 && phys < 0x1FA00000) {
        // Gap in expansion region 2
        return true;
    }
    if (phys >= 0x1FA00000 && phys < 0x1FC00000) {
        // Expansion region 3 - only used for DTL cards
        return true;
    }
    
    return false;
}

bool psx_check_irq_pending(const PsxState *psx) {
    uint32_t status = psx->cpu.cop0[12];
    uint32_t cause = psx->cpu.cop0[13];
    
    if ((status & 0x00000001) == 0) {
        return false;
    }
    
    uint32_t irq_mask = status & 0x0000FF00;
    return (cause & 0x00000400) && (irq_mask != 0);
}

void psx_step_timers(PsxState *psx, uint32_t cycles) {
    for (int i = 0; i < 3; i++) {
        PsxTimer *timer = &psx->timers[i];
        
        if ((timer->mode & PSX_TIMER_MODE_IRQ) == 0) {
            continue;
        }
        
        uint32_t target = timer->target;
        
        if (psx->io_regs[0x04 + (i * 0x10)] != 0) {
            target = psx->io_regs[0x04 + (i * 0x10)];
        }
        
        timer->count += cycles;
        
        if (timer->count >= target) {
            uint16_t timer_irq = 0;
            switch (i) {
                case 0: timer_irq = PSX_IRQ_TIMER0; break;
                case 1: timer_irq = PSX_IRQ_TIMER1; break;
                case 2: timer_irq = PSX_IRQ_TIMER2; break;
            }
            psx_trigger_irq(psx, timer_irq);
            
            if (timer->mode & PSX_TIMER_MODE_REPEAT) {
                timer->count = timer->count % target;
            } else {
                timer->mode &= ~PSX_TIMER_MODE_IRQ;
            }
        }
    }
}

void psx_init_test_mode(PsxState *psx) {
    memset(&psx->test_result, 0, sizeof(psx->test_result));
    psx->test_mode = true;
}

void psx_record_error(PsxState *psx, uint16_t error_code, const char *description) {
    psx->test_result.error_code = error_code;
    psx->test_result.test_count++;
    psx->test_result.cycles_at_error = (uint32_t)psx->cycles;
    psx->test_result.pc_at_error = psx->halt_pc;
    psx->test_result.opcode_at_error = psx->last_opcode;
    strncpy(psx->test_result.error_description, description, sizeof(psx->test_result.error_description) - 1);
    psx->test_result.error_description[sizeof(psx->test_result.error_description) - 1] = '\0';
}

static bool psx_handle_bios_call(PsxState *psx, uint32_t current_pc) {
    uint32_t function_id = psx->cpu.gpr[9] & 0xFF;

    if (current_pc != 0x000000A0 && current_pc != 0x000000B0 && current_pc != 0x000000C0) {
        return false;
    }

    switch (current_pc) {
    case 0x000000A0:
        switch (function_id) {
        case 0x3C:
            psx->cpu.gpr[2] = 0x00000013;
            break;
        case 0xB8:
        case 0xBC:
            psx->cpu.gpr[2] = 0;
            break;
        case 0xC0:
        case 0xC4:
            psx->cpu.gpr[2] = 0;
            break;
        default:
            psx->cpu.gpr[2] = 0;
            break;
        }
        break;
    case 0x000000B0:
        switch (function_id) {
        case 0x01:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0B:
        case 0x0C:
            psx->cpu.gpr[2] = 0;
            break;
        default:
            psx->cpu.gpr[2] = 0;
            break;
        }
        break;
    case 0x000000C0:
        psx->cpu.gpr[2] = 0;
        break;
    default:
        return false;
    }

    psx->cpu.pc = psx->cpu.gpr[31];
    psx->cpu.next_pc = psx->cpu.pc + 4;
    psx->cycles++;
    return true;
}

static uint32_t read_le32(const uint8_t *src) {
    return (uint32_t)src[0]
        | ((uint32_t)src[1] << 8)
        | ((uint32_t)src[2] << 16)
        | ((uint32_t)src[3] << 24);
}

static void write_le32(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
    dst[2] = (uint8_t)((value >> 16) & 0xFF);
    dst[3] = (uint8_t)((value >> 24) & 0xFF);
}

static uint16_t read_le16(const uint8_t *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static void write_le16(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
}

static uint8_t psx_read8(const PsxState *psx, uint32_t addr) {
    uint32_t ram_addr = psx_translate_ram(psx, addr);
    uint32_t scratch_addr = psx_translate_scratchpad(addr);
    uint32_t io_addr = psx_translate_io(addr);
    uint32_t bios_addr = psx_translate_bios(addr);

    if (ram_addr != UINT32_MAX && ram_addr < psx->ram_size) {
        return psx->ram[ram_addr];
    }

    if (scratch_addr != UINT32_MAX && scratch_addr < PSX_SCRATCHPAD_SIZE) {
        return psx->scratchpad[scratch_addr];
    }

    if (io_addr != UINT32_MAX && io_addr < PSX_IO_SIZE) {
        uint32_t word = psx->io_regs[(io_addr & ~3U) >> 2];
        return (uint8_t)((word >> ((io_addr & 3U) * 8)) & 0xFF);
    }

    if (bios_addr != UINT32_MAX && bios_addr < PSX_BIOS_SIZE) {
        return psx->bios[bios_addr];
    }

    return 0;
}

static uint16_t psx_read16(const PsxState *psx, uint32_t addr) {
    uint32_t ram_addr = psx_translate_ram(psx, addr);
    uint32_t scratch_addr = psx_translate_scratchpad(addr);
    uint32_t io_addr = psx_translate_io(addr);
    uint32_t bios_addr = psx_translate_bios(addr);

    if (ram_addr != UINT32_MAX && ram_addr + 1 < psx->ram_size) {
        return read_le16(&psx->ram[ram_addr]);
    }

    if (scratch_addr != UINT32_MAX && scratch_addr + 1 < PSX_SCRATCHPAD_SIZE) {
        return read_le16(&psx->scratchpad[scratch_addr]);
    }

    if (io_addr != UINT32_MAX && io_addr + 1 < PSX_IO_SIZE) {
        uint32_t word = psx->io_regs[(io_addr & ~3U) >> 2];
        return (uint16_t)((word >> ((io_addr & 2U) * 8)) & 0xFFFF);
    }

    if (bios_addr != UINT32_MAX && bios_addr + 1 < PSX_BIOS_SIZE) {
        return read_le16(&psx->bios[bios_addr]);
    }

    return 0;
}

static void psx_write8(PsxState *psx, uint32_t addr, uint8_t value) {
    uint32_t ram_addr = psx_translate_ram(psx, addr);
    uint32_t scratch_addr = psx_translate_scratchpad(addr);
    uint32_t io_addr = psx_translate_io(addr);

    if (ram_addr != UINT32_MAX && ram_addr < psx->ram_size) {
        psx->ram[ram_addr] = value;
        return;
    }

    if (scratch_addr != UINT32_MAX && scratch_addr < PSX_SCRATCHPAD_SIZE) {
        psx->scratchpad[scratch_addr] = value;
        return;
    }

    if (io_addr != UINT32_MAX && io_addr < PSX_IO_SIZE) {
        uint32_t index = (io_addr & ~3U) >> 2;
        uint32_t shift = (io_addr & 3U) * 8;
        uint32_t word = psx->io_regs[index];
        word &= ~(0xFFU << shift);
        word |= ((uint32_t)value << shift);
        psx->io_regs[index] = word;
        psx_note_io(psx, addr, value, true);
    }
}

static void psx_write16(PsxState *psx, uint32_t addr, uint16_t value) {
    uint32_t ram_addr = psx_translate_ram(psx, addr);
    uint32_t scratch_addr = psx_translate_scratchpad(addr);
    uint32_t io_addr = psx_translate_io(addr);

    if (ram_addr != UINT32_MAX && ram_addr + 1 < psx->ram_size) {
        write_le16(&psx->ram[ram_addr], value);
        return;
    }

    if (scratch_addr != UINT32_MAX && scratch_addr + 1 < PSX_SCRATCHPAD_SIZE) {
        write_le16(&psx->scratchpad[scratch_addr], value);
        return;
    }

    if (io_addr != UINT32_MAX && io_addr + 1 < PSX_IO_SIZE) {
        uint32_t index = (io_addr & ~3U) >> 2;
        uint32_t shift = (io_addr & 2U) * 8;
        uint32_t word = psx->io_regs[index];
        word &= ~(0xFFFFU << shift);
        word |= ((uint32_t)value << shift);
        psx->io_regs[index] = word;
        psx_note_io(psx, addr, value, true);
    }
}

void psx_use_internal_ram(PsxState *psx) {
    psx->ram = psx->ram_internal;
    psx->ram_size = PSX_RAM_SIZE;
    strncpy(psx->ram_backend_name, "internal", sizeof(psx->ram_backend_name) - 1);
    psx->ram_backend_name[sizeof(psx->ram_backend_name) - 1] = '\0';
}

void psx_use_slot2_ram(PsxState *psx, uint8_t *slot2_buffer, size_t slot2_size) {
    if (psx == NULL || slot2_buffer == NULL || slot2_size < PSX_RAM_SIZE) {
        return;
    }
    psx->ram = slot2_buffer;
    psx->ram_size = PSX_RAM_SIZE;
    strncpy(psx->ram_backend_name, "SuperChis", sizeof(psx->ram_backend_name) - 1);
    psx->ram_backend_name[sizeof(psx->ram_backend_name) - 1] = '\0';
}

bool psx_set_ram_backing(PsxState *psx, uint8_t *buffer, size_t size, const char *backend_name) {
    if (psx == NULL || buffer == NULL || size < PSX_RAM_SIZE) {
        return false;
    }

    psx->ram = buffer;
    psx->ram_size = size;
    strncpy(
        psx->ram_backend_name,
        backend_name != NULL ? backend_name : "external",
        sizeof(psx->ram_backend_name) - 1
    );
    psx->ram_backend_name[sizeof(psx->ram_backend_name) - 1] = '\0';
    return true;
}

static void psx_push_trace(PsxState *psx, uint32_t pc, uint32_t opcode, const char *text) {
    snprintf(
        psx->trace[psx->trace_pos],
        sizeof(psx->trace[psx->trace_pos]),
        "%08lx %08lx %s",
        (unsigned long)pc,
        (unsigned long)opcode,
        text
    );

    psx->trace_pos = (psx->trace_pos + 1) % PSX_TRACE_LINES;
    if (psx->trace_count < PSX_TRACE_LINES) {
        psx->trace_count++;
    }
}

static void psx_format_instruction(char *out, size_t out_size, uint32_t pc, uint32_t opcode) {
    uint32_t op = (opcode >> 26) & 0x3F;
    uint32_t rs = (opcode >> 21) & 0x1F;
    uint32_t rt = (opcode >> 16) & 0x1F;
    uint32_t rd = (opcode >> 11) & 0x1F;
    uint32_t sa = (opcode >> 6) & 0x1F;
    uint32_t funct = opcode & 0x3F;
    uint32_t target = (pc & 0xF0000000) | ((opcode & 0x03FFFFFF) << 2);
    int16_t simm = (int16_t)(opcode & 0xFFFF);
    uint16_t uimm = opcode & 0xFFFF;

    switch (op) {
    case 0x00:
        switch (funct) {
        case 0x00:
            snprintf(out, out_size, "sll %s,%s,%lu", psx_reg_names[rd], psx_reg_names[rt], (unsigned long)sa);
            return;
        case 0x02:
            snprintf(out, out_size, "srl %s,%s,%lu", psx_reg_names[rd], psx_reg_names[rt], (unsigned long)sa);
            return;
        case 0x03:
            snprintf(out, out_size, "sra %s,%s,%lu", psx_reg_names[rd], psx_reg_names[rt], (unsigned long)sa);
            return;
        case 0x04:
            snprintf(out, out_size, "sllv %s,%s,%s", psx_reg_names[rd], psx_reg_names[rt], psx_reg_names[rs]);
            return;
        case 0x06:
            snprintf(out, out_size, "srlv %s,%s,%s", psx_reg_names[rd], psx_reg_names[rt], psx_reg_names[rs]);
            return;
        case 0x07:
            snprintf(out, out_size, "srav %s,%s,%s", psx_reg_names[rd], psx_reg_names[rt], psx_reg_names[rs]);
            return;
        case 0x08:
            snprintf(out, out_size, "jr %s", psx_reg_names[rs]);
            return;
        case 0x09:
            if (rd)
                snprintf(out, out_size, "jalr %s,%s", psx_reg_names[rd], psx_reg_names[rs]);
            else
                snprintf(out, out_size, "jalr %s", psx_reg_names[rs]);
            return;
        case 0x0A:
            snprintf(out, out_size, "movz %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x0B:
            snprintf(out, out_size, "movn %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x0C:
            snprintf(out, out_size, "syscall");
            return;
        case 0x0D:
            snprintf(out, out_size, "break");
            return;
        case 0x0F:
            snprintf(out, out_size, "mov %s,%s (hi)", psx_reg_names[rd], psx_reg_names[rs]);
            return;
        case 0x10:
            snprintf(out, out_size, "mfhi %s", psx_reg_names[rd]);
            return;
        case 0x11:
            snprintf(out, out_size, "mthi %s", psx_reg_names[rs]);
            return;
        case 0x12:
            snprintf(out, out_size, "mflo %s", psx_reg_names[rd]);
            return;
        case 0x13:
            snprintf(out, out_size, "mtlo %s", psx_reg_names[rs]);
            return;
        case 0x14:
            snprintf(out, out_size, "mfhi %s; mtlo %s", psx_reg_names[rd], psx_reg_names[rs]);
            return;
        case 0x18:
            snprintf(out, out_size, "mult %s,%s", psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x19:
            snprintf(out, out_size, "multu %s,%s", psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x1A:
            snprintf(out, out_size, "div %s,%s", psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x1B:
            snprintf(out, out_size, "divu %s,%s", psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x1C:
            if (rd == 0)
                snprintf(out, out_size, "madd %s,%s", psx_reg_names[rs], psx_reg_names[rt]);
            else
                snprintf(out, out_size, "mul %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x20:
            snprintf(out, out_size, "add %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x21:
            snprintf(out, out_size, "addu %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x22:
            snprintf(out, out_size, "sub %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x23:
            snprintf(out, out_size, "subu %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x24:
            snprintf(out, out_size, "and %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x25:
            snprintf(out, out_size, "or %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x26:
            snprintf(out, out_size, "xor %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x27:
            snprintf(out, out_size, "nor %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x2A:
            snprintf(out, out_size, "slt %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x2B:
            snprintf(out, out_size, "sltu %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x2C:
            snprintf(out, out_size, "sgt %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        case 0x2D:
            snprintf(out, out_size, "sgtu %s,%s,%s", psx_reg_names[rd], psx_reg_names[rs], psx_reg_names[rt]);
            return;
        default:
            break;
        }
        break;
    case 0x01:
        if (rt == 0x00) {
            snprintf(out, out_size, "bltz %s,%d", psx_reg_names[rs], (int)simm);
            return;
        }
        if (rt == 0x01) {
            snprintf(out, out_size, "bgez %s,%d", psx_reg_names[rs], (int)simm);
            return;
        }
        if (rt == 0x10) {
            snprintf(out, out_size, "bltzal %s,%d", psx_reg_names[rs], (int)simm);
            return;
        }
        if (rt == 0x11) {
            snprintf(out, out_size, "bgezal %s,%d", psx_reg_names[rs], (int)simm);
            return;
        }
        snprintf(out, out_size, "bcond rt=%lu off=%d", (unsigned long)rt, (int)simm);
        return;
    case 0x02:
        snprintf(out, out_size, "j %08lx", (unsigned long)target);
        return;
    case 0x03:
        snprintf(out, out_size, "jal %08lx", (unsigned long)target);
        return;
    case 0x04:
        snprintf(out, out_size, "beq %s,%s,%d", psx_reg_names[rs], psx_reg_names[rt], (int)simm);
        return;
    case 0x05:
        snprintf(out, out_size, "bne %s,%s,%d", psx_reg_names[rs], psx_reg_names[rt], (int)simm);
        return;
    case 0x06:
        snprintf(out, out_size, "blez %s,%d", psx_reg_names[rs], (int)simm);
        return;
    case 0x07:
        snprintf(out, out_size, "bgtz %s,%d", psx_reg_names[rs], (int)simm);
        return;
    case 0x08:
        snprintf(out, out_size, "addi %s,%s,%d", psx_reg_names[rt], psx_reg_names[rs], (int)simm);
        return;
    case 0x09:
        snprintf(out, out_size, "addiu %s,%s,%d", psx_reg_names[rt], psx_reg_names[rs], (int)simm);
        return;
    case 0x0A:
        snprintf(out, out_size, "slti %s,%s,%d", psx_reg_names[rt], psx_reg_names[rs], (int)simm);
        return;
    case 0x0B:
        snprintf(out, out_size, "sltiu %s,%s,%u", psx_reg_names[rt], psx_reg_names[rs], (unsigned int)uimm);
        return;
    case 0x0C:
        snprintf(out, out_size, "andi %s,%s,%04x", psx_reg_names[rt], psx_reg_names[rs], uimm);
        return;
    case 0x0D:
        snprintf(out, out_size, "ori %s,%s,%04x", psx_reg_names[rt], psx_reg_names[rs], uimm);
        return;
    case 0x0E:
        snprintf(out, out_size, "xori %s,%s,%04x", psx_reg_names[rt], psx_reg_names[rs], uimm);
        return;
    case 0x0F:
        snprintf(out, out_size, "lui %s,%04x", psx_reg_names[rt], uimm);
        return;
    case 0x10:
        if (rs == 0x00) {
            snprintf(out, out_size, "mfc0 %s,c%lu", psx_reg_names[rt], (unsigned long)rd);
            return;
        }
        if (rs == 0x04) {
            snprintf(out, out_size, "mtc0 %s,c%lu", psx_reg_names[rt], (unsigned long)rd);
            return;
        }
        if (rs == 0x10 && funct == 0x10) {
            snprintf(out, out_size, "rfe");
            return;
        }
        break;
    case 0x20:
        snprintf(out, out_size, "lb %s,%d(%s)", psx_reg_names[rt], (int)simm, psx_reg_names[rs]);
        return;
    case 0x21:
        snprintf(out, out_size, "lh %s,%d(%s)", psx_reg_names[rt], (int)simm, psx_reg_names[rs]);
        return;
    case 0x23:
        snprintf(out, out_size, "lw %s,%d(%s)", psx_reg_names[rt], (int)simm, psx_reg_names[rs]);
        return;
    case 0x24:
        snprintf(out, out_size, "lbu %s,%d(%s)", psx_reg_names[rt], (int)simm, psx_reg_names[rs]);
        return;
    case 0x25:
        snprintf(out, out_size, "lhu %s,%d(%s)", psx_reg_names[rt], (int)simm, psx_reg_names[rs]);
        return;
    case 0x28:
        snprintf(out, out_size, "sb %s,%d(%s)", psx_reg_names[rt], (int)simm, psx_reg_names[rs]);
        return;
    case 0x29:
        snprintf(out, out_size, "sh %s,%d(%s)", psx_reg_names[rt], (int)simm, psx_reg_names[rs]);
        return;
    case 0x2B:
        snprintf(out, out_size, "sw %s,%d(%s)", psx_reg_names[rt], (int)simm, psx_reg_names[rs]);
        return;
    default:
        break;
    }

    snprintf(out, out_size, "op=%02lx raw=%08lx", (unsigned long)op, (unsigned long)opcode);
}

void psx_init(PsxState *psx) {
    memset(psx, 0, sizeof(*psx));
    
    // Initialize i-Cache
    memset(psx->icache, 0, sizeof(psx->icache));
    psx->write_queue_head = 0;
    psx->write_queue_tail = 0;
    psx->write_queue_count = 0;
    
    psx_use_internal_ram(psx);
    psx_init_gpu(psx);
    psx_init_dma(psx);
    psx_init_cdrom(psx);
    psx_reset(psx);
}

static void psx_flush_write_queue(PsxState *psx) {
    // Flush all pending writes in the write queue
    // This is automatically called when reading from KSEG1 or hardware registers
    while (psx->write_queue_count > 0) {
        PsxWriteQueueEntry *entry = &psx->write_queue[psx->write_queue_tail];
        if (entry->is_write) {
            // Actually perform the pending write
            uint32_t offset = psx_translate_ram(psx, entry->address);
            if (offset != UINT32_MAX) {
                switch (entry->size) {
                    case 1:
                        psx->ram[offset] = (uint8_t)entry->value;
                        break;
                    case 2:
                        write_le16(&psx->ram[offset], (uint16_t)entry->value);
                        break;
                    case 4:
                        write_le32(&psx->ram[offset], entry->value);
                        break;
                }
            }
        }
        psx->write_queue_tail = (psx->write_queue_tail + 1) % PSX_WRITE_QUEUE_SIZE;
        psx->write_queue_count--;
    }
}

static void psx_add_write_queue(PsxState *psx, uint32_t addr, uint32_t value, uint8_t size) {
    // Add a write to the queue
    // Returns immediately if queue is full (shouldn't happen in practice)
    if (psx->write_queue_count >= PSX_WRITE_QUEUE_SIZE) {
        psx_flush_write_queue(psx);
    }
    
    PsxWriteQueueEntry *entry = &psx->write_queue[psx->write_queue_head];
    entry->address = addr;
    entry->value = value;
    entry->size = size;
    entry->is_write = true;
    
    psx->write_queue_head = (psx->write_queue_head + 1) % PSX_WRITE_QUEUE_SIZE;
    psx->write_queue_count++;
}

static bool psx_is_cached_access(uint32_t addr) {
    // Returns true if address is in cached region (KUSEG or KSEG0)
    // KSEG1 bypasses cache, KSEG2 is for kernel virtual (not used in PSX)
    return (addr < 0xA0000000);  // KUSEG or KSEG0
}

static uint32_t psx_icache_get_index(uint32_t phys_addr) {
    // i-Cache is direct-mapped: index = bits[11:4] of physical address
    return (phys_addr >> 4) & 0xFF;
}

static uint32_t psx_icache_get_tag(uint32_t phys_addr) {
    // Tag is physical address[31:12]
    return phys_addr >> 12;
}

static uint32_t psx_icache_lookup(PsxState *psx, uint32_t phys_addr) {
    if (!psx_is_cached_access(phys_addr)) {
        return 0;
    }
    
    uint32_t index = psx_icache_get_index(phys_addr);
    uint32_t tag = psx_icache_get_tag(phys_addr);
    PsxICacheLine *line = &psx->icache[index];
    
    if (line->tag == tag && line->valid) {
        uint32_t offset = phys_addr & 0xF;
        return read_le32(&line->data[offset]);
    }
    
    return 0;
}

static void psx_icache_fill(PsxState *psx, uint32_t phys_addr) {
    if (!psx_is_cached_access(phys_addr)) {
        return;
    }
    
    uint32_t index = psx_icache_get_index(phys_addr);
    uint32_t tag = psx_icache_get_tag(phys_addr);
    PsxICacheLine *line = &psx->icache[index];
    
    memcpy(line->data, &psx->ram[phys_addr & 0x7FFFF0], PSX_ICACHE_LINE_SIZE);
    line->tag = tag;
    line->valid = 1;
}

static uint32_t psx_fetch_instruction(PsxState *psx, uint32_t addr) {
    uint32_t phys_addr = psx_to_physical(addr);
    uint32_t opcode = psx_icache_lookup(psx, phys_addr);
    
    if (opcode == 0) {
        opcode = psx_read32(psx, addr);
        if (psx_is_cached_access(addr)) {
            psx_icache_fill(psx, phys_addr);
            opcode = psx_icache_lookup(psx, phys_addr);
        }
    }
    
    return opcode;
}

static uint32_t psx_translate_ram_mirror(const PsxState *psx, uint32_t addr) {
    uint32_t phys = addr & 0x007FFFFF;
    
    // Handle RAM mirroring: 2MB RAM can be mirrored to first 8MB
    if (phys < 0x00800000 && psx->ram_size != 0) {
        return phys % psx->ram_size;
    }
    
    // Check for BIOS mirroring (512KB BIOS mirrored to last 4MB)
    if (phys >= 0x1FC00000 && phys < 0x20000000) {
        return phys - 0x1FC00000;
    }
    
    return UINT32_MAX;
}

void psx_reset(PsxState *psx) {
    if (psx->ram == NULL || psx->ram_size < PSX_RAM_SIZE) {
        psx_use_internal_ram(psx);
    }

    memset(&psx->cpu, 0, sizeof(psx->cpu));
    memset(psx->ram, 0, psx->ram_size);
    memset(psx->scratchpad, 0, sizeof(psx->scratchpad));
    memset(psx->io_regs, 0, sizeof(psx->io_regs));
    psx->cycles = 0;
    psx->halted = false;
    psx->bios_loaded = false;
    psx->last_pc = 0;
    psx->last_opcode = 0;
    psx->halt_pc = 0;
    strncpy(psx->halt_reason, "running", sizeof(psx->halt_reason) - 1);
    psx->halt_reason[sizeof(psx->halt_reason) - 1] = '\0';
    strncpy(psx->last_disasm, "none", sizeof(psx->last_disasm) - 1);
    psx->last_disasm[sizeof(psx->last_disasm) - 1] = '\0';
    psx->trace_pos = 0;
    psx->trace_count = 0;
    memset(psx->trace, 0, sizeof(psx->trace));
    psx->last_io_addr = 0;
    psx->last_io_value = 0;
    psx->last_io_write = false;
    psx->pending_irqs = 0;
    memset(psx->timers, 0, sizeof(psx->timers));
    
    psx->cpu.cop0[12] = 0x00000000;
    psx->cpu.cop0[13] = 0x00000000;
    psx->cpu.cop0[15] = 0x00000002;

    psx->cpu.pc = 0x80000000;
    psx->cpu.next_pc = psx->cpu.pc + 4;
}

void psx_load_demo(PsxState *psx) {
    /*
     * This is a tiny hand-written instruction stream used only to prove out
     * fetch/decode/execute on real hardware.
     *
     *  lui  t0, 0x1234
     *  ori  t0, t0, 0x5678
     *  addiu t1, zero, 7
     *  sw   t0, 0x100(zero)
     *  sw   t1, 0x104(zero)
     *  j    0x80000014
     *  nop
     */
    static const uint32_t demo_program[] = {
        0x3C081234,
        0x35085678,
        0x24090007,
        0xAC080100,
        0xAC090104,
        0x08000005,
        0x00000000,
    };

    memset(psx->ram, 0, psx->ram_size);
    memcpy(psx->ram, demo_program, sizeof(demo_program));
    memset(psx->scratchpad, 0, sizeof(psx->scratchpad));
    memset(psx->io_regs, 0, sizeof(psx->io_regs));
    psx->cpu.pc = 0x80000000;
    psx->cpu.next_pc = psx->cpu.pc + 4;
    psx->halted = false;
    strncpy(psx->halt_reason, "demo loaded", sizeof(psx->halt_reason) - 1);
    psx->halt_reason[sizeof(psx->halt_reason) - 1] = '\0';
}

bool psx_load_bios(PsxState *psx, const uint8_t *data, size_t size) {
    if (data == NULL || size < 128 * 1024 || size > PSX_BIOS_SIZE * 2) {
        return false;
    }

    size_t copy_size = size;
    if (copy_size > PSX_BIOS_SIZE) copy_size = PSX_BIOS_SIZE;
    memcpy(psx->bios, data, copy_size);
    psx->bios_loaded = true;
    strncpy(psx->halt_reason, "bios loaded", sizeof(psx->halt_reason) - 1);
    psx->halt_reason[sizeof(psx->halt_reason) - 1] = '\0';
    return true;
}

void psx_boot_bios(PsxState *psx) {
    memset(&psx->cpu, 0, sizeof(psx->cpu));
    memset(psx->ram, 0, psx->ram_size);
    memset(psx->scratchpad, 0, sizeof(psx->scratchpad));
    memset(psx->io_regs, 0, sizeof(psx->io_regs));
    psx->cpu.pc = 0xBFC00000;
    psx->cpu.next_pc = psx->cpu.pc + 4;
    psx->cycles = 0;
    psx->halted = false;
    psx->last_pc = 0;
    psx->last_opcode = 0;
    psx->halt_pc = 0;
    strncpy(psx->halt_reason, "booting bios", sizeof(psx->halt_reason) - 1);
    psx->halt_reason[sizeof(psx->halt_reason) - 1] = '\0';
    strncpy(psx->last_disasm, "none", sizeof(psx->last_disasm) - 1);
    psx->last_disasm[sizeof(psx->last_disasm) - 1] = '\0';
    psx->trace_pos = 0;
    psx->trace_count = 0;
    memset(psx->trace, 0, sizeof(psx->trace));
    psx->last_io_addr = 0;
    psx->last_io_value = 0;
    psx->last_io_write = false;
}

void psx_load_raw_bin(PsxState *psx, const uint8_t *data, size_t size, uint32_t load_addr, uint32_t entry_pc) {
    if (data == NULL || size == 0) {
        psx_halt(psx, entry_pc, "raw bin load failed");
        return;
    }

    if (load_addr + size > PSX_RAM_SIZE) {
        psx_halt(psx, entry_pc, "raw bin too large");
        return;
    }

    memset(psx->ram, 0, psx->ram_size);
    memcpy(&psx->ram[load_addr], data, size);
    memset(&psx->cpu, 0, sizeof(psx->cpu));
    psx->cpu.pc = entry_pc;
    psx->cpu.next_pc = entry_pc + 4;
    psx->cycles = 0;
    psx->halted = false;
    psx->last_pc = 0;
    psx->last_opcode = 0;
    psx->halt_pc = 0;
    strncpy(psx->halt_reason, "raw bin loaded", sizeof(psx->halt_reason) - 1);
    psx->halt_reason[sizeof(psx->halt_reason) - 1] = '\0';
    strncpy(psx->last_disasm, "none", sizeof(psx->last_disasm) - 1);
    psx->last_disasm[sizeof(psx->last_disasm) - 1] = '\0';
    psx->trace_pos = 0;
    psx->trace_count = 0;
    memset(psx->trace, 0, sizeof(psx->trace));
    memset(psx->scratchpad, 0, sizeof(psx->scratchpad));
    memset(psx->io_regs, 0, sizeof(psx->io_regs));
    psx->last_io_addr = 0;
    psx->last_io_value = 0;
    psx->last_io_write = false;
}

static void psx_flush_write_queue_on_access(PsxState *psx, uint32_t addr) {
    if (addr >= 0xA0000000 && psx->write_queue_count > 0) {
        psx_flush_write_queue(psx);
    }
}

uint32_t psx_read32(PsxState *psx, uint32_t addr) {
    psx_flush_write_queue_on_access(psx, addr);
    
    uint32_t ram_addr = psx_translate_ram(psx, addr);
    uint32_t scratch_addr = psx_translate_scratchpad(addr);
    uint32_t io_addr = psx_translate_io(addr);
    uint32_t bios_addr = psx_translate_bios(addr);
    
    if (ram_addr != UINT32_MAX && ram_addr + 3 < psx->ram_size) {
        return read_le32(&psx->ram[ram_addr]);
    }

    if (scratch_addr != UINT32_MAX && scratch_addr + 3 < PSX_SCRATCHPAD_SIZE) {
        return read_le32(&psx->scratchpad[scratch_addr]);
    }

    if (io_addr != UINT32_MAX && io_addr + 3 < PSX_IO_SIZE) {
        if (psx->dma && addr >= PSX_DMA_BASE_ADDR && addr < PSX_DMA_BASE_ADDR + 0x80) {
            return dma_read32(psx->dma, addr);
        }
        return psx->io_regs[io_addr >> 2];
    }
    
    if (addr >= PSX_GPU_BASE_ADDR && addr < PSX_GPU_BASE_ADDR + 0x10) {
        if (psx->gpu) {
            return gpu_read32(psx->gpu, addr);
        }
        return 0;
    }
    
    if (addr >= PSX_CDROM_BASE_ADDR && addr < PSX_CDROM_BASE_ADDR + 0x80) {
        if (psx->cdrom) {
            return cdrom_read8(psx->cdrom, addr);
        }
        return 0;
    }

    if (bios_addr != UINT32_MAX && bios_addr + 3 < PSX_BIOS_SIZE) {
        return read_le32(&psx->bios[bios_addr]);
    }

    return 0;
}

void psx_write32(PsxState *psx, uint32_t addr, uint32_t value) {
    psx_flush_write_queue_on_access(psx, addr);
    
    uint32_t ram_addr = psx_translate_ram(psx, addr);
    uint32_t scratch_addr = psx_translate_scratchpad(addr);
    uint32_t io_addr = psx_translate_io(addr);

    if (ram_addr != UINT32_MAX && ram_addr + 3 < psx->ram_size) {
        write_le32(&psx->ram[ram_addr], value);
        return;
    }

    if (scratch_addr != UINT32_MAX && scratch_addr + 3 < PSX_SCRATCHPAD_SIZE) {
        write_le32(&psx->scratchpad[scratch_addr], value);
        return;
    }

    if (io_addr != UINT32_MAX && io_addr + 3 < PSX_IO_SIZE) {
        if (psx->dma && addr >= PSX_DMA_BASE_ADDR && addr < PSX_DMA_BASE_ADDR + 0x80) {
            dma_write32(psx->dma, addr, value);
            return;
        }
        psx->io_regs[io_addr >> 2] = value;
        psx_note_io(psx, addr, value, true);
    }
    
    if (addr >= PSX_GPU_BASE_ADDR && addr < PSX_GPU_BASE_ADDR + 0x10) {
        if (psx->gpu) {
            gpu_write32(psx->gpu, addr, value);
        }
        return;
    }
    
    if (addr >= PSX_CDROM_BASE_ADDR && addr < PSX_CDROM_BASE_ADDR + 0x80) {
        if (psx->cdrom) {
            cdrom_write8(psx->cdrom, addr, value);
        }
        return;
    }
}

static void psx_execute_special(PsxState *psx, uint32_t opcode, uint32_t *branch_target) {
    uint32_t funct = opcode & 0x3F;
    uint32_t rs = (opcode >> 21) & 0x1F;
    uint32_t rt = (opcode >> 16) & 0x1F;
    uint32_t rd = (opcode >> 11) & 0x1F;
    uint32_t sa = (opcode >> 6) & 0x1F;

    switch (funct) {
    case 0x00:
        psx->cpu.gpr[rd] = psx->cpu.gpr[rt] << sa;
        break;
    case 0x02:
        psx->cpu.gpr[rd] = psx->cpu.gpr[rt] >> sa;
        break;
    case 0x03:
        psx->cpu.gpr[rd] = (uint32_t)(((int32_t)psx->cpu.gpr[rt]) >> sa);
        break;
    case 0x04:
        psx->cpu.gpr[rd] = psx->cpu.gpr[rt] << (psx->cpu.gpr[rs] & 0x1F);
        break;
    case 0x06:
        psx->cpu.gpr[rd] = psx->cpu.gpr[rt] >> (psx->cpu.gpr[rs] & 0x1F);
        break;
    case 0x07:
        psx->cpu.gpr[rd] = (uint32_t)(((int32_t)psx->cpu.gpr[rt]) >> (psx->cpu.gpr[rs] & 0x1F));
        break;
    case 0x08:
        *branch_target = psx->cpu.gpr[rs];
        break;
    case 0x09:
        psx->cpu.gpr[rd ? rd : 31] = psx->cpu.pc + 8;
        *branch_target = psx->cpu.gpr[rs];
        break;
    case 0x0A:
        psx->cpu.lo = psx->cpu.gpr[rs];
        break;
    case 0x0B:
        psx->cpu.hi = psx->cpu.gpr[rs];
        break;
    case 0x0C:
        // Syscall - increment PC and return (BIOS will handle the call)
        psx->cpu.next_pc = psx->cpu.pc + 4;
        psx->cpu.pc = psx->cpu.next_pc;
        // Don't halt - let BIOS handle syscall
        break;
    case 0x0D:
        // Break - increment PC and return (debug handler will process)
        psx->cpu.next_pc = psx->cpu.pc + 4;
        psx->cpu.pc = psx->cpu.next_pc;
        // Don't halt - let debug handler process break
        break;
    case 0x0F:
        psx->cpu.gpr[rd] = psx->cpu.hi;
        psx->cpu.hi = psx->cpu.gpr[rs];
        break;
    case 0x10:
        psx->cpu.gpr[rd] = psx->cpu.hi;
        break;
    case 0x11:
        psx->cpu.hi = psx->cpu.gpr[rs];
        break;
    case 0x12:
        psx->cpu.gpr[rd] = psx->cpu.lo;
        break;
    case 0x13:
        psx->cpu.lo = psx->cpu.gpr[rs];
        break;
    case 0x14:
        psx->cpu.gpr[rd] = psx->cpu.lo;
        psx->cpu.lo = psx->cpu.hi;
        psx->cpu.hi = psx->cpu.gpr[rs];
        break;
    case 0x18:
    {
        int64_t result = (int64_t)(int32_t)psx->cpu.gpr[rs] * (int64_t)(int32_t)psx->cpu.gpr[rt];
        psx->cpu.lo = (uint32_t)result;
        psx->cpu.hi = (uint32_t)(result >> 32);
        break;
    }
    case 0x19:
    {
        uint64_t result = (uint64_t)psx->cpu.gpr[rs] * (uint64_t)psx->cpu.gpr[rt];
        psx->cpu.lo = (uint32_t)result;
        psx->cpu.hi = (uint32_t)(result >> 32);
        break;
    }
    case 0x1A:
        if (psx->cpu.gpr[rt] != 0) {
            psx->cpu.lo = (uint32_t)((int32_t)psx->cpu.gpr[rs] / (int32_t)psx->cpu.gpr[rt]);
            psx->cpu.hi = (uint32_t)((int32_t)psx->cpu.gpr[rs] % (int32_t)psx->cpu.gpr[rt]);
        }
        break;
    case 0x1B:
        if (psx->cpu.gpr[rt] != 0) {
            psx->cpu.lo = psx->cpu.gpr[rs] / psx->cpu.gpr[rt];
            psx->cpu.hi = psx->cpu.gpr[rs] % psx->cpu.gpr[rt];
        }
        break;
    case 0x1C:
    {
        if (rd == 0) {
            int64_t result = (int64_t)(int32_t)psx->cpu.gpr[rs] * (int64_t)(int32_t)psx->cpu.gpr[rt];
            psx->cpu.lo = (uint32_t)result;
            psx->cpu.hi = (uint32_t)(result >> 32);
        } else {
            int64_t result = (int64_t)(int32_t)psx->cpu.gpr[rs] * (int64_t)(int32_t)psx->cpu.gpr[rt];
            psx->cpu.gpr[rd] = (uint32_t)result;
        }
        break;
    }
    case 0x1D:
        psx_halt(psx, psx->cpu.pc, "unsupported SPECIAL 0x1D");
        break;
    case 0x1E:
        psx_halt(psx, psx->cpu.pc, "unsupported SPECIAL 0x1E");
        break;
    case 0x1F:
        psx_halt(psx, psx->cpu.pc, "unsupported SPECIAL 0x1F");
        break;
    case 0x20:
        psx->cpu.gpr[rd] = psx->cpu.gpr[rs] + psx->cpu.gpr[rt];
        break;
    case 0x21:
        psx->cpu.gpr[rd] = psx->cpu.gpr[rs] + psx->cpu.gpr[rt];
        break;
    case 0x22:
        psx->cpu.gpr[rd] = psx->cpu.gpr[rs] - psx->cpu.gpr[rt];
        break;
    case 0x23:
        psx->cpu.gpr[rd] = psx->cpu.gpr[rs] - psx->cpu.gpr[rt];
        break;
    case 0x24:
        psx->cpu.gpr[rd] = psx->cpu.gpr[rs] & psx->cpu.gpr[rt];
        break;
    case 0x25:
        psx->cpu.gpr[rd] = psx->cpu.gpr[rs] | psx->cpu.gpr[rt];
        break;
    case 0x26:
        psx->cpu.gpr[rd] = psx->cpu.gpr[rs] ^ psx->cpu.gpr[rt];
        break;
    case 0x27:
        psx->cpu.gpr[rd] = ~(psx->cpu.gpr[rs] | psx->cpu.gpr[rt]);
        break;
    case 0x2A:
        psx->cpu.gpr[rd] = ((int32_t)psx->cpu.gpr[rs] < (int32_t)psx->cpu.gpr[rt]) ? 1U : 0U;
        break;
    case 0x2B:
        psx->cpu.gpr[rd] = (psx->cpu.gpr[rs] < psx->cpu.gpr[rt]) ? 1U : 0U;
        break;
    case 0x2C:
        psx->cpu.gpr[rd] = ((int32_t)psx->cpu.gpr[rs] <= (int32_t)psx->cpu.gpr[rt]) ? 1U : 0U;
        break;
    case 0x2D:
        psx->cpu.gpr[rd] = (psx->cpu.gpr[rs] <= psx->cpu.gpr[rt]) ? 1U : 0U;
        break;
    case 0x2E:
        if ((int32_t)psx->cpu.gpr[rs] > (int32_t)psx->cpu.gpr[rt]) {
            psx->cpu.gpr[rd] = psx->cpu.gpr[rs] - psx->cpu.gpr[rt];
        } else {
            psx->cpu.gpr[rd] = psx->cpu.gpr[rt] - psx->cpu.gpr[rs];
        }
        break;
    case 0x2F:
        if (psx->cpu.gpr[rs] > psx->cpu.gpr[rt]) {
            psx->cpu.gpr[rd] = psx->cpu.gpr[rs] - psx->cpu.gpr[rt];
        } else {
            psx->cpu.gpr[rd] = psx->cpu.gpr[rt] - psx->cpu.gpr[rs];
        }
        break;
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37:
    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F:
        psx_halt(psx, psx->cpu.pc, "unsupported SPECIAL opcode");
        break;
    default:
        psx_halt(psx, psx->cpu.pc, "unsupported SPECIAL opcode");
        break;
    }
}

static void psx_execute_cop0(PsxState *psx, uint32_t opcode) {
    uint32_t rs = (opcode >> 21) & 0x1F;
    uint32_t rt = (opcode >> 16) & 0x1F;
    uint32_t rd = (opcode >> 11) & 0x1F;
    uint32_t funct = opcode & 0x3F;

    switch (rs) {
    case 0x00:
        psx->cpu.gpr[rt] = psx->cpu.cop0[rd];
        break;
    case 0x04:
        psx->cpu.cop0[rd] = psx->cpu.gpr[rt];
        break;
    case 0x10:
        if (funct == 0x10) {
            uint32_t sr = psx->cpu.cop0[12];
            psx->cpu.cop0[12] = (sr & ~0x0FU) | ((sr >> 2) & 0x0FU);
            break;
        }
        psx_halt(psx, psx->cpu.pc, "unsupported COP0 function");
        break;
    default:
        psx_halt(psx, psx->cpu.pc, "unsupported COP0 opcode");
        break;
    }
}

void psx_step(PsxState *psx) {
    uint32_t opcode;
    uint32_t op;
    uint32_t rs;
    uint32_t rt;
    uint32_t rd;
    uint32_t imm;
    uint32_t current_pc = psx->cpu.pc;
    uint32_t branch_target = psx->cpu.next_pc + 4;

    if (psx->halted) {
        return;
    }

    if (psx_handle_bios_call(psx, current_pc)) {
        return;
    }

    // Check for unaligned memory access
    if (current_pc & 3) {
        psx_raise_exception(psx, PSX_EXC_ADEL, current_pc);
        return;
    }
    
    // Check for scratchpad execution (should cause bus error)
    uint32_t scratch_addr = psx_translate_scratchpad(current_pc);
    if (scratch_addr != UINT32_MAX) {
        psx_raise_exception(psx, PSX_EXC_IBE, current_pc);  // Instruction bus error
        return;
    }
    
    // Check for memory errors in the address
    if (psx_check_memory_error(psx, current_pc, false)) {
        psx_raise_exception(psx, PSX_EXC_IBE, current_pc);
        return;
    }
    
    opcode = psx_fetch_instruction(psx, current_pc);
    psx->last_pc = current_pc;
    psx->last_opcode = opcode;
    psx_format_instruction(psx->last_disasm, sizeof(psx->last_disasm), current_pc, opcode);
    psx_push_trace(psx, current_pc, opcode, psx->last_disasm);

    op = (opcode >> 26) & 0x3F;
    rs = (opcode >> 21) & 0x1F;
    rt = (opcode >> 16) & 0x1F;
    rd = (opcode >> 11) & 0x1F;
    imm = opcode & 0xFFFF;

    switch (op) {
    case 0x00:
        psx_execute_special(psx, opcode, &branch_target);
        break;
    case 0x01:
        switch (rt) {
        case 0x00:
            if ((int32_t)psx->cpu.gpr[rs] < 0) {
                branch_target = psx->cpu.next_pc + ((int16_t)imm << 2);
            }
            break;
        case 0x01:
            if ((int32_t)psx->cpu.gpr[rs] >= 0) {
                branch_target = psx->cpu.next_pc + ((int16_t)imm << 2);
            }
            break;
        case 0x10:
            psx->cpu.gpr[31] = current_pc + 8;
            if ((int32_t)psx->cpu.gpr[rs] < 0) {
                branch_target = psx->cpu.next_pc + ((int16_t)imm << 2);
            }
            break;
        case 0x11:
            psx->cpu.gpr[31] = current_pc + 8;
            if ((int32_t)psx->cpu.gpr[rs] >= 0) {
                branch_target = psx->cpu.next_pc + ((int16_t)imm << 2);
            }
            break;
        default:
            psx_halt(psx, current_pc, "unsupported bcond opcode");
            break;
        }
        break;
    case 0x02:
        branch_target = (current_pc & 0xF0000000) | ((opcode & 0x03FFFFFF) << 2);
        break;
    case 0x03:
        psx->cpu.gpr[31] = current_pc + 8;
        branch_target = (current_pc & 0xF0000000) | ((opcode & 0x03FFFFFF) << 2);
        break;
    case 0x04:
        if (psx->cpu.gpr[rs] == psx->cpu.gpr[rt]) {
            branch_target = psx->cpu.next_pc + ((int16_t)imm << 2);
        }
        break;
    case 0x05:
        if (psx->cpu.gpr[rs] != psx->cpu.gpr[rt]) {
            branch_target = psx->cpu.next_pc + ((int16_t)imm << 2);
        }
        break;
    case 0x06:
        if ((int32_t)psx->cpu.gpr[rs] <= 0) {
            branch_target = psx->cpu.next_pc + ((int16_t)imm << 2);
        }
        break;
    case 0x07:
        if ((int32_t)psx->cpu.gpr[rs] > 0) {
            branch_target = psx->cpu.next_pc + ((int16_t)imm << 2);
        }
        break;
    case 0x08:
        psx->cpu.gpr[rt] = psx->cpu.gpr[rs] + (int16_t)imm;
        break;
    case 0x09:
        psx->cpu.gpr[rt] = psx->cpu.gpr[rs] + (int16_t)imm;
        break;
    case 0x0A:
        psx->cpu.gpr[rt] = ((int32_t)psx->cpu.gpr[rs] < (int16_t)imm) ? 1U : 0U;
        break;
    case 0x0B:
        psx->cpu.gpr[rt] = (psx->cpu.gpr[rs] < (uint32_t)(uint16_t)imm) ? 1U : 0U;
        break;
    case 0x0C:
        psx->cpu.gpr[rt] = psx->cpu.gpr[rs] & imm;
        break;
    case 0x0D:
        psx->cpu.gpr[rt] = psx->cpu.gpr[rs] | imm;
        break;
    case 0x0E:
        psx->cpu.gpr[rt] = psx->cpu.gpr[rs] ^ imm;
        break;
    case 0x0F:
        psx->cpu.gpr[rt] = imm << 16;
        break;
    case 0x10:
        psx_execute_cop0(psx, opcode);
        break;
    case 0x24:
        psx->cpu.gpr[rt] = psx_read8(psx, psx->cpu.gpr[rs] + (int16_t)imm);
        break;
    case 0x20:
        psx->cpu.gpr[rt] = (uint32_t)(int32_t)(int8_t)psx_read8(psx, psx->cpu.gpr[rs] + (int16_t)imm);
        break;
    case 0x21:
        psx->cpu.gpr[rt] = (uint32_t)(int32_t)(int16_t)psx_read16(psx, psx->cpu.gpr[rs] + (int16_t)imm);
        break;
    case 0x23:
        psx->cpu.gpr[rt] = psx_read32(psx, psx->cpu.gpr[rs] + (int16_t)imm);
        break;
    case 0x25:
        psx->cpu.gpr[rt] = psx_read16(psx, psx->cpu.gpr[rs] + (int16_t)imm);
        break;
    case 0x28:
        psx_write8(psx, psx->cpu.gpr[rs] + (int16_t)imm, (uint8_t)(psx->cpu.gpr[rt] & 0xFF));
        break;
    case 0x29:
        psx_write16(psx, psx->cpu.gpr[rs] + (int16_t)imm, (uint16_t)(psx->cpu.gpr[rt] & 0xFFFF));
        break;
    case 0x2B:
        psx_write32(psx, psx->cpu.gpr[rs] + (int16_t)imm, psx->cpu.gpr[rt]);
        break;
    case 0x22:
    {
        uint32_t addr = psx->cpu.gpr[rs] + (int16_t)imm;
        uint32_t aligned_addr = addr & ~3U;
        uint32_t old_val = psx_read32(psx, aligned_addr);
        uint32_t shift = (addr & 3U) * 8;
        uint32_t mask = 0x00FFFFFF >> (24 - shift);
        psx->cpu.gpr[rt] = (old_val & mask) | ((psx->cpu.gpr[rt] & 0xFF) << shift);
        break;
    }
    case 0x26:
    {
        uint32_t addr = psx->cpu.gpr[rs] + (int16_t)imm;
        uint32_t aligned_addr = addr & ~3U;
        uint32_t old_val = psx_read32(psx, aligned_addr);
        uint32_t shift = (addr & 3U) * 8;
        uint32_t mask = 0xFFFFFF00 << shift;
        psx->cpu.gpr[rt] = (old_val & mask) | ((psx->cpu.gpr[rt] & 0xFF) << (24 - shift));
        break;
    }
    case 0x2A:
    {
        uint32_t addr = psx->cpu.gpr[rs] + (int16_t)imm;
        uint32_t aligned_addr = addr & ~3U;
        uint32_t old_val = psx_read32(psx, aligned_addr);
        uint32_t shift = (addr & 3U) * 8;
        uint32_t mask = 0x00FFFFFF >> (24 - shift);
        uint32_t new_val = (old_val & ~mask) | ((psx->cpu.gpr[rt] & 0xFF) << shift);
        psx_write32(psx, aligned_addr, new_val);
        break;
    }
    case 0x2E:
    {
        uint32_t addr = psx->cpu.gpr[rs] + (int16_t)imm;
        uint32_t aligned_addr = addr & ~3U;
        uint32_t old_val = psx_read32(psx, aligned_addr);
        uint32_t shift = (addr & 3U) * 8;
        uint32_t mask = 0xFFFFFF00 << shift;
        uint32_t new_val = (old_val & ~mask) | ((psx->cpu.gpr[rt] & 0xFF) << (24 - shift));
        psx_write32(psx, aligned_addr, new_val);
        break;
    }
    case 0x31:
        psx->cpu.gpr[rt] = (uint32_t)(int32_t)(int16_t)psx_read16(psx, psx->cpu.gpr[rs] + (int16_t)imm);
        break;
    case 0x35:
        psx->cpu.gpr[rt] = psx_read16(psx, psx->cpu.gpr[rs] + (int16_t)imm);
        break;
    case 0x39:
        psx_write16(psx, psx->cpu.gpr[rs] + (int16_t)imm, (uint16_t)(psx->cpu.gpr[rt] & 0xFFFF));
        break;
    case 0x3F:
        psx_halt(psx, current_pc, "cache opcode (unimplemented)");
        break;
    default:
        psx_halt(psx, current_pc, "unsupported primary opcode");
        break;
    }

    if (psx->halted) {
        return;
    }

    (void)rd;
    psx->cpu.gpr[0] = 0;
    psx->cpu.pc = psx->cpu.next_pc;
    psx->cpu.next_pc = branch_target;
    psx->cycles++;
    
    if (psx_check_irq_pending(psx)) {
        uint32_t status = psx->cpu.cop0[12];
        psx->cpu.pc = psx->cpu.next_pc;
        psx->cpu.next_pc = 0x80000080;
        psx->cpu.cop0[12] = (status & ~0x0000003C) | ((status & 0x0000000F) << 2);
        psx->cycles++;
    }
}

uint32_t psx_run(PsxState *psx, uint32_t max_steps) {
    uint32_t executed = 0;

    while (executed < max_steps && !psx->halted) {
        psx_step(psx);
        executed++;
    }

    return executed;
}

static PsxGpuState g_gpu;
static PsxDmaState g_dma;
static PsxCdromState g_cdrom;

void psx_init_gpu(PsxState *psx) {
    memset(&g_gpu, 0, sizeof(g_gpu));
    psx->gpu = &g_gpu;
}

void psx_init_dma(PsxState *psx) {
    memset(&g_dma, 0, sizeof(g_dma));
    psx->dma = &g_dma;
}

void psx_init_cdrom(PsxState *psx) {
    memset(&g_cdrom, 0, sizeof(g_cdrom));
    psx->cdrom = &g_cdrom;
}

void psx_update_peripherals(PsxState *psx, uint32_t cycles) {
    if (psx->gpu) {
        gpu_update(psx->gpu, cycles);
    }
    if (psx->dma) {
        dma_update(psx->dma, cycles);
    }
    if (psx->cdrom) {
        cdrom_update(psx->cdrom, cycles);
    }
    
    psx_step_timers(psx, cycles);
    
    psx->vblank_counter += cycles;
    if (psx->vblank_counter >= 33333 * 3) {
        psx->vblank_counter = 0;
        psx_trigger_irq(psx, PSX_IRQ_VBLANK);
    }
}

void psx_render_frame(PsxState *psx) {
    (void)psx;
}
