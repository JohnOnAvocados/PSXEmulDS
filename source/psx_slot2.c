#include "psx_slot2.h"

#include <nds.h>
#include <string.h>

#define SUPERCHIS_DETECT_ADDR    ((volatile uint16_t*)0x0A000000)
#define SUPERCHIS_SDRAM_ADDR    ((volatile uint32_t*)0x0C000000)

static Slot2Device g_slot2;

void slot2_init(void) {
    memset(&g_slot2, 0, sizeof(g_slot2));
    g_slot2.type = SLOT2_NONE;
    g_slot2.buffer = NULL;
    g_slot2.size = 0;
    g_slot2.initialized = false;
    g_slot2.sdram_base = 0x0C000000;
    g_slot2.fram_base = 0x0A800000;
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
    
    uint16_t test_val = 0x1234;
    *SUPERCHIS_DETECT_ADDR = test_val;
    uint16_t readback = *SUPERCHIS_DETECT_ADDR;
    
    if (readback == test_val) {
        volatile uint32_t *sdram = SUPERCHIS_SDRAM_ADDR;
        sdram[0] = 0xDEADBEEF;
        sdram[1] = 0xCAFEBABE;
        
        if (sdram[0] == 0xDEADBEEF && sdram[1] == 0xCAFEBABE) {
            g_slot2.type = SLOT2_SUPERCHIS;
            g_slot2.size = SLOT2_SUPERCHIS_SDRAM;
            g_slot2.writable = true;
            strncpy(g_slot2.name, "SuperChis Prime", sizeof(g_slot2.name) - 1);
            g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
            
            g_slot2.buffer = (uint8_t*)0x0C000000;
            return true;
        }
        
        g_slot2.type = SLOT2_SUPERCARD;
        g_slot2.size = 16 * 1024 * 1024;
        g_slot2.writable = true;
        strncpy(g_slot2.name, "SuperCard Slot-2", sizeof(g_slot2.name) - 1);
        g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
        
        g_slot2.buffer = (uint8_t*)malloc(g_slot2.size);
        if (g_slot2.buffer != NULL) {
            memset(g_slot2.buffer, 0, g_slot2.size);
            return true;
        }
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
