#include "psx_memctrl.h"
#include <string.h>

void memctrl_init(PsxMemCtrlState *mem) {
    memset(mem, 0, sizeof(*mem));
    mem->expansion1_base = 0x1F000000;
    mem->expansion2_base = 0x1F802000;
    mem->expansion1_delay = 0x0013243F;
    mem->expansion3_delay = 0x00003022;
    mem->bios_delay = 0x0013243F;
    mem->spu_delay = 0x200931E1;
    mem->cdrom_delay = 0x00020843;
    mem->expansion2_delay = 0x00070777;
    mem->com_delay = 0x00031125;
    mem->ram_size = 0x00000B88;
}

void memctrl_reset(PsxMemCtrlState *mem) {
    memctrl_init(mem);
}

uint32_t memctrl_read32(PsxMemCtrlState *mem, uint32_t addr) {
    uint32_t offset = addr - PSX_MEMCTRL_BASE_ADDR;
    
    switch (offset) {
    case 0x00: return mem->expansion1_base;
    case 0x04: return mem->expansion2_base;
    case 0x08: return mem->expansion1_delay;
    case 0x0C: return mem->expansion3_delay;
    case 0x10: return mem->bios_delay;
    case 0x14: return mem->spu_delay;
    case 0x18: return mem->cdrom_delay;
    case 0x1C: return mem->expansion2_delay;
    case 0x20: return mem->com_delay;
    case 0x60: return mem->ram_size;
    default: return 0;
    }
}

void memctrl_write32(PsxMemCtrlState *mem, uint32_t addr, uint32_t value) {
    uint32_t offset = addr - PSX_MEMCTRL_BASE_ADDR;
    
    switch (offset) {
    case 0x00: mem->expansion1_base = value; break;
    case 0x04: mem->expansion2_base = value; break;
    case 0x08: mem->expansion1_delay = value; break;
    case 0x0C: mem->expansion3_delay = value; break;
    case 0x10: mem->bios_delay = value; break;
    case 0x14: mem->spu_delay = value; break;
    case 0x18: mem->cdrom_delay = value; break;
    case 0x1C: mem->expansion2_delay = value; break;
    case 0x20: mem->com_delay = value; break;
    case 0x60: mem->ram_size = value; break;
    default: break;
    }
}