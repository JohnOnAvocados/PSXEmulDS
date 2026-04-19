#include "psx_slot2.h"

#include <nds.h>
#include <string.h>
#include <stdio.h>

#define SUPERCHIS_SDRAM_ADDR    0x0C000000
#define SUPERCHIS_FRAM_ADDR    0x0A000000
#define SUPERCHIS_NOR_ADDR    0x08000000
#define SUPERCHIS_MODE_REG     ((volatile uint16_t*)0x09FFFFFE)

#define SUPERCHIS_DETECT_MAGIC 0xA5A5
#define SUPERCHIS_MODE_RAM    0x0005
#define SUPERCHIS_MODE_MEDIA  0x0003

#define ACE3DS_RAM_ADDR      0x0C000000
#define ACE3DS_DETECT_MAGIC  0x5A5A

static Slot2Device g_slot2;
static bool g_slot2_initialized = false;

void slot2_init(void) {
    memset(&g_slot2, 0, sizeof(g_slot2));
    g_slot2.type = SLOT2_NONE;
    g_slot2.buffer = NULL;
    g_slot2.size = 0;
    g_slot2.max_size = 0;
    g_slot2.initialized = true;
    g_slot2.detected = false;
    g_slot2.writable = false;
    g_slot2.sdram_base = SUPERCHIS_SDRAM_ADDR;
    g_slot2.fram_base = SUPERCHIS_FRAM_ADDR;
    g_slot2.nor_base = SUPERCHIS_NOR_ADDR;
    g_slot2.current_bank = 0;
    g_slot2.bank_count = 0;
    g_slot2_initialized = true;
}

void slot2_deinit(void) {
    if (g_slot2.buffer != NULL && g_slot2.type == SLOT2_SUPERCHIS) {
        free(g_slot2.buffer);
        g_slot2.buffer = NULL;
    }
    g_slot2.initialized = false;
    g_slot2.detected = false;
}

Slot2Device* slot2_get_device(void) {
    return &g_slot2;
}

bool slot2_has_ram(void) {
    return g_slot2.detected && g_slot2.size > 0 && g_slot2.buffer != NULL;
}

size_t slot2_get_ram_size(void) {
    if (!g_slot2.detected) {
        slot2_detect();
    }
    return g_slot2.size;
}

uint8_t slot2_get_bank(void) {
    return g_slot2.current_bank;
}

bool slot2_set_bank(uint8_t bank) {
    if (!slot2_has_ram()) {
        return false;
    }
    if (bank >= g_slot2.bank_count) {
        return false;
    }
    g_slot2.current_bank = bank;
    return true;
}

static bool detect_superchis_prime(void) {
    volatile uint16_t *test_addr = (volatile uint16_t*)SUPERCHIS_SDRAM_ADDR;
    
    volatile uint16_t *mode_reg = (volatile uint16_t*)0x09FFFFFE;
    
    uint16_t old_val = test_addr[0];
    uint16_t test_val = 0x5A5A;
    
    test_addr[0] = test_val;
    test_addr[0] = test_val;
    uint16_t read1 = test_addr[0];
    test_addr[0] = 0;
    test_addr[0] = 0;
    uint16_t read2 = test_addr[0];
    test_addr[0] = old_val;
    test_addr[0] = old_val;
    
    if (read1 == test_val && read2 == 0) {
        g_slot2.type = SLOT2_SUPERCHIS;
        g_slot2.size = SLOT2_SUPERCHIS_SDRAM;
        g_slot2.max_size = SLOT2_SUPERCHIS_SDRAM;
        g_slot2.bank_count = SLOT2_MAX_SIZE / SLOT2_BANK_SIZE;
        g_slot2.writable = true;
        strncpy(g_slot2.name, "SuperChis Prime", sizeof(g_slot2.name) - 1);
        g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
        
        g_slot2.buffer = malloc(g_slot2.size);
        if (g_slot2.buffer != NULL) {
            memset(g_slot2.buffer, 0, g_slot2.size);
        } else {
            g_slot2.size = 0;
            return false;
        }
        
        return true;
    }
    
    return false;
}

static bool detect_ace3ds_ram(void) {
    volatile uint16_t *test_addr = (volatile uint16_t*)ACE3DS_RAM_ADDR;
    
    uint16_t old_val = test_addr[0];
    uint16_t test_val = 0xA5A5;
    
    test_addr[0] = test_val;
    test_addr[0] = test_val;
    uint16_t read1 = test_addr[0];
    test_addr[0] = 0xFFFF;
    test_addr[0] = 0xFFFF;
    uint16_t read2 = test_addr[0];
    test_addr[0] = old_val;
    test_addr[0] = old_val;
    
    if (read1 == test_val && read2 == 0xFFFF) {
        g_slot2.type = SLOT2_ACE3DS;
        g_slot2.size = 8 * 1024 * 1024;
        g_slot2.max_size = 8 * 1024 * 1024;
        g_slot2.bank_count = 4;
        g_slot2.writable = true;
        strncpy(g_slot2.name, "Ace3DS RAM", sizeof(g_slot2.name) - 1);
        g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
        
        g_slot2.buffer = malloc(g_slot2.size);
        if (g_slot2.buffer != NULL) {
            memset(g_slot2.buffer, 0, g_slot2.size);
        } else {
            g_slot2.size = 0;
            return false;
        }
        
        return true;
    }
    
    return false;
}

bool slot2_detect(void) {
    if (!g_slot2_initialized) {
        slot2_init();
    }
    
    if (g_slot2.detected) {
        return g_slot2.type != SLOT2_NONE;
    }
    
    g_slot2.detected = true;
    g_slot2.type = SLOT2_NONE;
    g_slot2.size = 0;
    
    if (detect_superchis_prime()) {
        iprintf("slot2: SuperChis Prime detected\n");
        iprintf("slot2: SDRAM %lu KB\n", (unsigned long)(g_slot2.size / 1024));
        return true;
    }
    
    if (detect_ace3ds_ram()) {
        iprintf("slot2: Ace3DS RAM detected\n");
        iprintf("slot2: SDRAM %lu KB\n", (unsigned long)(g_slot2.size / 1024));
        return true;
    }
    
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