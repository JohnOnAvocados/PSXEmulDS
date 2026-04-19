#ifndef PSX_MEMCTRL_H
#define PSX_MEMCTRL_H

#include <stdbool.h>
#include <stdint.h>

#define PSX_MEMCTRL_BASE_ADDR 0x1F801000
#define PSX_MEMCTRL_SIZE 0x30

#define PSX_MEMCTRL_EXPANSION1_BASE      0x1F801000
#define PSX_MEMCTRL_EXPANSION2_BASE      0x1F801004
#define PSX_MEMCTRL_EXPANSION1_DELAY     0x1F801008
#define PSX_MEMCTRL_EXPANSION3_DELAY     0x1F80100C
#define PSX_MEMCTRL_BIOS_DELAY            0x1F801010
#define PSX_MEMCTRL_SPU_DELAY             0x1F801014
#define PSX_MEMCTRL_CDROM_DELAY         0x1F801018
#define PSX_MEMCTRL_EXPANSION2_DELAY    0x1F80101C
#define PSX_MEMCTRL_COM_DELAY           0x1F801020
#define PSX_MEMCTRL_RAM_SIZE             0x1F801060
#define PSX_MEMCTRL_CACHE_CONTROL        0x1F8010E0

typedef struct {
    uint32_t expansion1_base;
    uint32_t expansion2_base;
    uint32_t expansion1_delay;
    uint32_t expansion3_delay;
    uint32_t bios_delay;
    uint32_t spu_delay;
    uint32_t cdrom_delay;
    uint32_t expansion2_delay;
    uint32_t com_delay;
    uint32_t ram_size;
} PsxMemCtrlState;

void memctrl_init(PsxMemCtrlState *mem);
void memctrl_reset(PsxMemCtrlState *mem);
uint32_t memctrl_read32(PsxMemCtrlState *mem, uint32_t addr);
void memctrl_write32(PsxMemCtrlState *mem, uint32_t addr, uint32_t value);

#endif