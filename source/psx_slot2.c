#include "psx_slot2.h"

#include <nds.h>
#include <string.h>
#include <stdio.h>

#define SUPERCHIS_DETECT_ADDR    ((volatile uint16_t*)0x08000000)
#define SUPERCHIS_SDRAM_ADDR    ((volatile uint16_t*)0x08000000)
#define SUPERCHIS_MODE_REG    ((volatile uint16_t*)0x09FFFFFE)

#define SUPERCHIS_MODE_RAM     0x0005
#define SUPERCHIS_MODE_MEDIA  0x0003

static Slot2Device g_slot2;

void slot2_init(void) {
    memset(&g_slot2, 0, sizeof(g_slot2));
    g_slot2.type = SLOT2_NONE;
    g_slot2.buffer = NULL;
    g_slot2.size = 0;
    g_slot2.initialized = false;
    g_slot2.sdram_base = 0x08000000;
    g_slot2.fram_base = 0x0A000000;
    g_slot2.nor_base = 0x08000000;
}

void slot2_deinit(void) {
    if (g_slot2.buffer != NULL) {
        free(g_slot2.buffer);
        g_slot2.buffer = NULL;
    }
    g_slot2.initialized = false;
}

Slot2Device* slot2_get_device(void) {
    return &g_slot2;
}

bool slot2_detect(void) {
    if (g_slot2.initialized) {
        return g_slot2.type != SLOT2_NONE;
    }
    
    g_slot2.initialized = true;
    
    sysSetBusOwners(BUS_OWNER_ARM9, BUS_OWNER_ARM9);

    iprintf("slot2: set bus owners\n");

    volatile uint16_t *mode_reg = SUPERCHIS_MODE_REG;
    volatile uint16_t *sdram = SUPERCHIS_SDRAM_ADDR;

    mode_reg[0] = 0xA55A;
    mode_reg[0] = 0xA55A;
    mode_reg[0] = SUPERCHIS_MODE_RAM;
    mode_reg[0] = SUPERCHIS_MODE_RAM;

    iprintf("slot2: unlock sequence sent\n");

    sdram[0] = 0xDEAD;
    sdram[1] = 0xBEEF;
    sdram[2] = 0xCAFE;
    sdram[3] = 0xBABE;

    iprintf("slot2: wrote test %04x %04x %04x %04x\n",
           (unsigned int)sdram[0], (unsigned int)sdram[1],
           (unsigned int)sdram[2], (unsigned int)sdram[3]);

    if (sdram[0] == 0xDEAD && sdram[1] == 0xBEEF &&
        sdram[2] == 0xCAFE && sdram[3] == 0xBABE) {
        g_slot2.type = SLOT2_SUPERCHIS;
        g_slot2.size = SLOT2_SUPERCHIS_SDRAM;
        g_slot2.writable = true;
        strncpy(g_slot2.name, "SuperChis Prime", sizeof(g_slot2.name) - 1);
        g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
        
        g_slot2.buffer = (uint8_t*)0x08000000;
        return true;
    }
    
    g_slot2.type = SLOT2_NONE;
    g_slot2.size = 0;
    strncpy(g_slot2.name, "None", sizeof(g_slot2.name) - 1);
    g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
    return false;
}

bool slot2_read(Slot2Device *dev, uint32_t addr, uint8_t *data, size_t len) {
    if (dev == NULL || dev->buffer == NULL) {
        return false;
    }
    if (addr + len > dev->size) {
        return false;
    }
    memcpy(data, &dev->buffer[addr], len);
    return true;
}

bool slot2_write(Slot2Device *dev, uint32_t addr, const uint8_t *data, size_t len) {
    if (dev == NULL || dev->buffer == NULL || !dev->writable) {
        return false;
    }
    if (addr + len > dev->size) {
        return false;
    }
    memcpy(&dev->buffer[addr], data, len);
    return true;
}

uint8_t slot2_read8(Slot2Device *dev, uint32_t addr) {
    if (dev == NULL || dev->buffer == NULL || addr >= dev->size) {
        return 0xFF;
    }
    return dev->buffer[addr];
}

uint16_t slot2_read16(Slot2Device *dev, uint32_t addr) {
    if (dev == NULL || dev->buffer == NULL || addr + 1 >= dev->size) {
        return 0xFFFF;
    }
    return dev->buffer[addr] | ((uint16_t)dev->buffer[addr + 1] << 8);
}

uint32_t slot2_read32(Slot2Device *dev, uint32_t addr) {
    if (dev == NULL || dev->buffer == NULL || addr + 3 >= dev->size) {
        return 0xFFFFFFFF;
    }
    return dev->buffer[addr] 
        | ((uint32_t)dev->buffer[addr + 1] << 8)
        | ((uint32_t)dev->buffer[addr + 2] << 16)
        | ((uint32_t)dev->buffer[addr + 3] << 24);
}

void slot2_write8(Slot2Device *dev, uint32_t addr, uint8_t value) {
    if (dev == NULL || dev->buffer == NULL || !dev->writable || addr >= dev->size) {
        return;
    }
    dev->buffer[addr] = value;
}

void slot2_write16(Slot2Device *dev, uint32_t addr, uint16_t value) {
    if (dev == NULL || dev->buffer == NULL || !dev->writable || addr + 1 >= dev->size) {
        return;
    }
    dev->buffer[addr] = value & 0xFF;
    dev->buffer[addr + 1] = (value >> 8) & 0xFF;
}

void slot2_write32(Slot2Device *dev, uint32_t addr, uint32_t value) {
    if (dev == NULL || dev->buffer == NULL || !dev->writable || addr + 3 >= dev->size) {
        return;
    }
    dev->buffer[addr] = value & 0xFF;
    dev->buffer[addr + 1] = (value >> 8) & 0xFF;
    dev->buffer[addr + 2] = (value >> 16) & 0xFF;
    dev->buffer[addr + 3] = (value >> 24) & 0xFF;
}
