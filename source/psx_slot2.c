#include "psx_slot2.h"

#include <nds.h>
#include <string.h>
#include <stdio.h>

#define SUPERCHIS_SDRAM_ADDR    0x0C000000
#define SUPERCHIS_FRAM_ADDR     0x0A000000
#define SUPERCHIS_NOR_ADDR      0x08000000
#define SUPERCHIS_MODE_REG      ((volatile uint16_t*)0x09FFFFFE)

#define SUPERCHIS_DETECT_MAGIC  0xA5A5
#define SUPERCHIS_MODE_RAM      0x0005
#define SUPERCHIS_MODE_MEDIA    0x0003

#define ACE3DS_RAM_ADDR        0x0C000000
#define ACE3DS_DETECT_MAGIC    0x5A5A

static Slot2Device g_slot2;
static bool g_slot2_initialized = false;
static bool g_slot2_manual_enabled = false;

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
    g_slot2_manual_enabled = false;
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

void slot2_enable_manual(bool enable) {
    g_slot2_manual_enabled = enable;
    if (enable) {
        g_slot2.detected = false;
        g_slot2.type = SLOT2_NONE;
    }
}

bool slot2_get_manual_status(void) {
    return g_slot2_manual_enabled;
}

static void slot2_set_bus_ownership(void) {
    sysSetBusOwners(BUS_OWNER_ARM9, BUS_OWNER_ARM9);
    for (volatile int i = 0; i < 1000; i++);
}

static bool test_ram_at_address(uint32_t addr, uint32_t test_size) {
    volatile uint16_t *test_ptr = (volatile uint16_t*)addr;
    
    uint16_t old_val = test_ptr[0];
    
    test_ptr[0] = 0x5A5A;
    test_ptr[0] = 0x5A5A;
    uint16_t read1 = test_ptr[0];
    
    test_ptr[0] = 0x0000;
    test_ptr[0] = 0x0000;
    uint16_t read2 = test_ptr[0];
    
    test_ptr[0] = old_val;
    test_ptr[0] = old_val;
    
    return (read1 == 0x5A5A);
}

static bool detect_superchis_prime(void) {
    iprintf("slot2: Trying SuperChis Prime...\n");
    
    slot2_set_bus_ownership();
    
    volatile uint16_t *mode_reg = (volatile uint16_t*)0x09FFFFFE;
    volatile uint16_t *test_addr = (volatile uint16_t*)SUPERCHIS_SDRAM_ADDR;
    
    mode_reg[0] = 0xA55A;
    mode_reg[0] = 0xA55A;
    mode_reg[0] = SUPERCHIS_MODE_RAM;
    mode_reg[0] = SUPERCHIS_MODE_RAM;
    
    for (volatile int i = 0; i < 100; i++);
    
    bool detected = test_ram_at_address(SUPERCHIS_SDRAM_ADDR, 32 * 1024 * 1024);
    
    if (detected) {
        iprintf("slot2: SuperChis SDRAM detected!\n");
        g_slot2.type = SLOT2_SUPERCHIS;
        g_slot2.size = 32 * 1024 * 1024;
        g_slot2.max_size = 32 * 1024 * 1024;
        g_slot2.bank_count = 16;
        g_slot2.writable = true;
        strncpy(g_slot2.name, "SuperChis Prime", sizeof(g_slot2.name) - 1);
        g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
        
        g_slot2.buffer = malloc(g_slot2.size);
        if (g_slot2.buffer != NULL) {
            memset(g_slot2.buffer, 0, g_slot2.size);
            iprintf("slot2: Allocated %lu KB\n", (unsigned long)(g_slot2.size / 1024));
            return true;
        } else {
            iprintf("slot2: Memory allocation failed!\n");
            g_slot2.size = 0;
            g_slot2.type = SLOT2_NONE;
        }
    } else {
        mode_reg[0] = 0xA55A;
        mode_reg[0] = 0xA55A;
        mode_reg[0] = SUPERCHIS_MODE_MEDIA;
        mode_reg[0] = SUPERCHIS_MODE_MEDIA;
    }
    
    return false;
}

static bool detect_ace3ds_ram(void) {
    iprintf("slot2: Trying Ace3DS...\n");
    
    slot2_set_bus_ownership();
    
    volatile uint16_t *test_addr = (volatile uint16_t*)ACE3DS_RAM_ADDR;
    
    bool detected = test_ram_at_address(ACE3DS_RAM_ADDR, 8 * 1024 * 1024);
    
    if (detected) {
        iprintf("slot2: Ace3DS RAM detected!\n");
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
            iprintf("slot2: Allocated %lu KB\n", (unsigned long)(g_slot2.size / 1024));
            return true;
        } else {
            g_slot2.size = 0;
            g_slot2.type = SLOT2_NONE;
        }
    }
    
    return false;
}

static bool detect_generic_slot2(void) {
    iprintf("slot2: Trying generic detection...\n");
    
    slot2_set_bus_ownership();
    
    volatile uint16_t *test_addr = (volatile uint16_t*)0x08000000;
    uint16_t val = test_addr[0];
    iprintf("slot2: Base read: 0x%04X\n", val);
    
    if (val != 0xFFFF && val != 0x0000) {
        bool generic = test_ram_at_address(0x08000000, 8 * 1024 * 1024);
        if (generic) {
            iprintf("slot2: Generic Slot-2 RAM found!\n");
            g_slot2.type = SLOT2_GBAEXP;
            g_slot2.size = 8 * 1024 * 1024;
            g_slot2.max_size = 8 * 1024 * 1024;
            g_slot2.bank_count = 4;
            g_slot2.writable = true;
            strncpy(g_slot2.name, "Slot-2 RAM", sizeof(g_slot2.name) - 1);
            g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
            
            g_slot2.buffer = malloc(g_slot2.size);
            if (g_slot2.buffer != NULL) {
                memset(g_slot2.buffer, 0, g_slot2.size);
                return true;
            }
        }
    }
    
    return false;
}

bool slot2_detect(void) {
    if (!g_slot2_initialized) {
        slot2_init();
    }
    
    if (g_slot2.detected && !g_slot2_manual_enabled) {
        return g_slot2.type != SLOT2_NONE;
    }
    
    if (g_slot2_manual_enabled) {
        iprintf("slot2: Manual mode enabled\n");
        iprintf("slot2: Trying manual SDRAM...\n");
        
        slot2_set_bus_ownership();
        
        bool detected = test_ram_at_address(0x0C000000, 32 * 1024 * 1024);
        
        if (detected) {
            g_slot2.type = SLOT2_SUPERCHIS;
            g_slot2.size = 32 * 1024 * 1024;
            g_slot2.max_size = 32 * 1024 * 1024;
            g_slot2.bank_count = 16;
            g_slot2.writable = true;
            g_slot2.detected = true;
            strncpy(g_slot2.name, "Manual Slot-2", sizeof(g_slot2.name) - 1);
            g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
            
            g_slot2.buffer = malloc(g_slot2.size);
            if (g_slot2.buffer != NULL) {
                memset(g_slot2.buffer, 0, g_slot2.size);
                iprintf("slot2: Manual RAM enabled: %lu KB\n", (unsigned long)(g_slot2.size / 1024));
                return true;
            }
        }
        
        iprintf("slot2: Manual mode failed!\n");
        g_slot2.detected = true;
        g_slot2.type = SLOT2_NONE;
        return false;
    }
    
    g_slot2.detected = true;
    g_slot2.type = SLOT2_NONE;
    g_slot2.size = 0;
    
    if (detect_superchis_prime()) {
        iprintf("slot2: === SUCCESS ===\n");
        return true;
    }
    
    if (detect_ace3ds_ram()) {
        iprintf("slot2: === SUCCESS ===\n");
        return true;
    }
    
    if (detect_generic_slot2()) {
        iprintf("slot2: === SUCCESS ===\n");
        return true;
    }
    
    strncpy(g_slot2.name, "None", sizeof(g_slot2.name) - 1);
    g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
    iprintf("slot2: No Slot-2 RAM found\n");
    iprintf("slot2: Use START+SELECT to try manual mode\n");
    
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
