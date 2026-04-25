#include "psx_slot2.h"
#include "psx_debug.h"

#include <nds.h>
#include <string.h>
#include <stdio.h>

#define SLOT2_EXMEMCNT_4_2 (EXMEMCNT_ROM_TIME1_10_CYCLES | EXMEMCNT_ROM_TIME2_6_CYCLES | EXMEMCNT_SRAM_TIME_18_CYCLES)
#define SLOT2_EXMEMCNT_3_1 (EXMEMCNT_ROM_TIME1_8_CYCLES | EXMEMCNT_ROM_TIME2_4_CYCLES | EXMEMCNT_SRAM_TIME_18_CYCLES)
#define SLOT2_EXMEMCNT_2_1 (EXMEMCNT_ROM_TIME1_6_CYCLES | EXMEMCNT_ROM_TIME2_4_CYCLES | EXMEMCNT_SRAM_TIME_18_CYCLES)

#define EZ_CMD_SET_PSRAM_PAGE 0x9860000
#define EZ_CMD_SET_ROM_PAGE 0x9880000
#define EZ_CMD_SET_NOR_WRITE 0x9C40000

static Slot2Device g_slot2;
static bool g_slot2_initialized = false;
static bool g_slot2_detecting = false;

#define SLOT2_DEVICE_NONE 0xFFFF

static uint16_t *g_slot2_ram_base = NULL;
static uint32_t g_slot2_ram_size = 0;
static uint16_t g_slot2_device_id = SLOT2_DEVICE_NONE;
static uint16_t g_slot2_ram_banks = 0;

static void slot2_set_bus_ownership(void) {
    sysSetBusOwners(BUS_OWNER_ARM9, BUS_OWNER_ARM9);
    for (volatile int i = 0; i < 1000; i++);
}

static void slot2_set_cart_owner(void) {
    sysSetCartOwner(true);
    for (volatile int i = 0; i < 1000; i++);
}

static void ez_command(uint32_t address, uint16_t value) {
    *((volatile uint16_t*)0x9FE0000) = 0xD200;
    *((volatile uint16_t*)0x8000000) = 0x1500;
    *((volatile uint16_t*)0x8020000) = 0xD200;
    *((volatile uint16_t*)0x8040000) = 0x1500;
    *((volatile uint16_t*)address) = value;
    *((volatile uint16_t*)0x9FC0000) = 0x1500;
}

static bool __extram_detect(uint32_t max_banks, uint32_t max_address) {
    g_slot2_ram_size = 0;
    g_slot2_ram_banks = 0;
    
    uint32_t previous_size = 2048;
    uint32_t proposed_size = 4096;
    bool searching = true;
    
    g_slot2_ram_base = (uint16_t*)0x08000000;
    
    while ((((uintptr_t)g_slot2_ram_base) + proposed_size) <= 0xA000000) {
        volatile uint16_t *ptr1 = (volatile uint16_t*)(((uintptr_t)g_slot2_ram_base) + previous_size);
        volatile uint16_t *ptr2 = (volatile uint16_t*)(((uintptr_t)g_slot2_ram_base) + proposed_size - 2);
        
        uint16_t ptr2v = *ptr2;
        ptr2v ^= 0xFFFF;
        *ptr2 = ptr2v;
        if (*ptr2 != ptr2v)
            searching = false;
        
        ptr2v ^= 0xFFFF;
        *ptr2 = ptr2v;
        
        if (!searching) break;
        
        uint16_t ptr1v = *ptr1;
        ptr1v ^= 0xFFFF;
        *ptr1 = ptr1v;
        if (*ptr1 != ptr1v)
            searching = false;
        
        if (*g_slot2_ram_base != 0x0000) {
            *ptr1 = 0x0000;
            if (*g_slot2_ram_base == 0x0000)
                searching = false;
        } else if (*g_slot2_ram_base != 0xFFFF) {
            *ptr1 = 0xFFFF;
            if (*g_slot2_ram_base == 0xFFFF)
                searching = false;
        }
        
        ptr1v ^= 0xFFFF;
        *ptr1 = ptr1v;
        
        if (!searching) break;
        
        g_slot2_ram_size = proposed_size;
        previous_size = proposed_size;
        proposed_size += 2048;
    }
    
    if (!g_slot2_ram_size)
        return false;
    
    if (g_slot2_ram_size > (max_address - 0x08000000))
        g_slot2_ram_size = max_address - 0x08000000;
    
    g_slot2_ram_banks = 1;
    
    if (max_banks > 1) {
        ez_command(EZ_CMD_SET_PSRAM_PAGE, 0);
        
        while (g_slot2_ram_banks < max_banks) {
            uint16_t old_value_bank0 = *g_slot2_ram_base;
            *g_slot2_ram_base = 0x0000;
            ez_command(EZ_CMD_SET_PSRAM_PAGE, g_slot2_ram_banks << 12);
            uint16_t old_value_bank1 = *g_slot2_ram_base;
            *g_slot2_ram_base = 0xFFFF;
            ez_command(EZ_CMD_SET_PSRAM_PAGE, 0);
            bool result = *g_slot2_ram_base == 0x0000;
            *g_slot2_ram_base = old_value_bank0;
            ez_command(EZ_CMD_SET_PSRAM_PAGE, g_slot2_ram_banks << 12);
            *g_slot2_ram_base = old_value_bank1;
            ez_command(EZ_CMD_SET_PSRAM_PAGE, 0);
            
            if (result)
                g_slot2_ram_banks++;
            else
                break;
        }
    }
    
    return true;
}

static bool extram_detect(void) {
    return __extram_detect(1, 0xA000000);
}

static bool pak_rumble_detect(void) {
    for (int i = 0; i < 0x80; i++) {
        if (GBA_BUS[i] & BIT(1))
            return false;
    }
    return true;
}

#define SC_REG_ENABLE (*(volatile uint16_t*)0x9FFFFFE)
#define SC_ENABLE_MAGIC 0xA55A
#define SC_ENABLE_RAM (1 << 0)
#define SC_ENABLE_CARD (1 << 1)
#define SC_ENABLE_WRITE (1 << 2)
#define SC_ENABLE_RUMBLE (1 << 3)

static void supercard_unlock(uint32_t type) {
    SC_REG_ENABLE = SC_ENABLE_MAGIC;
    SC_REG_ENABLE = SC_ENABLE_MAGIC;
    uint32_t mode;
    if (!type)
        mode = 0;
    else if (type & 0x20)
        mode = SC_ENABLE_RUMBLE;
    else
        mode = SC_ENABLE_RAM | SC_ENABLE_WRITE;
    SC_REG_ENABLE = mode;
    SC_REG_ENABLE = mode;
}

static bool supercard_detect(void) {
    supercard_unlock(0x01);
    if (extram_detect()) return true;
    supercard_unlock(0x20);
    if (pak_rumble_detect()) return true;
    return false;
}

static void m3_unlock(uint32_t type) {
    *((volatile uint16_t*)0x08E00002);
    *((volatile uint16_t*)0x0800000E);
    *((volatile uint16_t*)0x08801FFC);
    *((volatile uint16_t*)0x0800104A);
    *((volatile uint16_t*)0x08800612);
    *((volatile uint16_t*)0x08000000);
    *((volatile uint16_t*)0x08801B66);
    *((volatile uint16_t*)0x08000000 + ((type == 0 ? 0x400003 : 0x400006) << 1));
    *((volatile uint16_t*)0x0800080E);
    *((volatile uint16_t*)0x08000000);
    *((volatile uint16_t*)0x080001E4);
    *((volatile uint16_t*)0x080001E4);
    *((volatile uint16_t*)0x08000188);
    *((volatile uint16_t*)0x08000188);
}

static bool m3_detect(void) {
    m3_unlock(0x01);
    if (extram_detect()) return true;
    return false;
}

static void g6_unlock(uint32_t type) {
    *((volatile uint16_t*)0x09000000);
    *((volatile uint16_t*)0x09FFFFE0);
    *((volatile uint16_t*)0x09FFFFEC);
    *((volatile uint16_t*)0x09FFFFEC);
    *((volatile uint16_t*)0x09FFFFEC);
    *((volatile uint16_t*)0x09FFFFFC);
    *((volatile uint16_t*)0x09FFFFFC);
    *((volatile uint16_t*)0x09FFFFFC);
    *((volatile uint16_t*)0x09FFFF4A);
    *((volatile uint16_t*)0x09FFFF4A);
    *((volatile uint16_t*)0x09FFFF4A);
    *((volatile uint16_t*)0x09200000 + ((type == 0 ? 0x3 : 0x6) << 1));
    *((volatile uint16_t*)0x09FFFFF0);
    *((volatile uint16_t*)0x09FFFFE8);
}

static bool g6_detect(void) {
    g6_unlock(0x01);
    if (extram_detect()) return true;
    return false;
}

static void ez_unlock(uint32_t type) {
    (void)type;
    ez_command(0x9880000, 0x8000);
    ez_command(EZ_CMD_SET_NOR_WRITE, 0x1500);
}

static bool ez_detect(void) {
    ez_unlock(0x01);
    volatile uint16_t *test_addr = (volatile uint16_t*)0x08400000;
    uint16_t old_val = *test_addr;
    *test_addr = 0x5A5A;
    *test_addr = 0x5A5A;
    uint16_t read1 = *test_addr;
    *test_addr = 0x0000;
    *test_addr = 0x0000;
    *test_addr = old_val;
    *test_addr = old_val;
    
    if (read1 == 0x5A5A) {
        return __extram_detect(1, 0x08800000);
    }
    return false;
}

static bool none_detect(void) {
    return true;
}

typedef struct {
    uint32_t peripheral_mask;
    bool (*detect)(void);
    void (*unlock)(uint32_t);
    const char *name;
} slot2_definition_t;

static slot2_definition_t slot2definitions[] = {
    { 0x01, supercard_detect, supercard_unlock, "SuperCard" },
    { 0x01, m3_detect, m3_unlock, "M3" },
    { 0x01, g6_detect, g6_unlock, "G6" },
    { 0x01, ez_detect, ez_unlock, "EZ Flash" },
    { 0x00, none_detect, NULL, "None" }
};

void slot2_init(void) {
    memset(&g_slot2, 0, sizeof(g_slot2));
    g_slot2.type = SLOT2_NONE;
    g_slot2.buffer = NULL;
    g_slot2.size = 0;
    g_slot2.max_size = 0;
    g_slot2.initialized = true;
    g_slot2.detected = false;
    g_slot2.writable = false;
    g_slot2.sdram_base = 0x08000000;
    g_slot2.fram_base = 0x0A000000;
    g_slot2.nor_base = 0x08000000;
    g_slot2.current_bank = 0;
    g_slot2.bank_count = 0;
    
    g_slot2_ram_base = NULL;
    g_slot2_ram_size = 0;
    g_slot2_device_id = SLOT2_DEVICE_NONE;
    g_slot2_ram_banks = 0;
    g_slot2_detecting = false;
    
    g_slot2_initialized = true;
}

void slot2_deinit(void) {
    g_slot2_ram_base = NULL;
    g_slot2_ram_size = 0;
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
    if (enable) {
        g_slot2.detected = false;
        g_slot2.type = SLOT2_NONE;
    }
}

bool slot2_get_manual_status(void) {
    return false;
}

bool slot2_detect(void) {
    if (!g_slot2_initialized) {
        slot2_init();
    }
    
    if (g_slot2.detected && !g_slot2_detecting) {
        return g_slot2.type != SLOT2_NONE;
    }
    
    if (g_slot2_detecting) {
        return false;
    }
    
    g_slot2_detecting = true;
    
    debug_info("Slot-2: Disabled for safety - use manual detection");
    iprintf("slot2: Disabled for safety\n");
    g_slot2.detected = true;
    g_slot2.type = SLOT2_NONE;
    g_slot2.size = 0;
    g_slot2.buffer = NULL;
    g_slot2_detecting = false;
    return false;
    
#if 0
    debug_info("Slot-2: Starting auto-detection...");
    iprintf("slot2: Starting auto-detection...\n");
    
    slot2_set_bus_ownership();
    
    bool detected = false;
    
    for (int i = 0; i < sizeof(slot2definitions) / sizeof(slot2definitions[0]); i++) {
        slot2_definition_t *def = &slot2definitions[i];
        
        if (def->peripheral_mask == 0x00) {
            break;
        }
        
        debug_info("Slot-2: Trying %s...", def->name);
        iprintf("slot2: Trying %s...\n", def->name);
        
        if (def->detect()) {
            g_slot2_device_id = i;
            g_slot2.type = SLOT2_SUPERCARD;
            g_slot2.buffer = (uint8_t*)g_slot2_ram_base;
            g_slot2.size = g_slot2_ram_size;
            g_slot2.max_size = g_slot2_ram_size;
            g_slot2.bank_count = g_slot2_ram_banks;
            g_slot2.writable = true;
            g_slot2.detected = true;
            strncpy(g_slot2.name, def->name, sizeof(g_slot2.name) - 1);
            g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
            
            debug_info("Slot-2: Detected %s at %p, %lu KB", 
                def->name, g_slot2_ram_base, (unsigned long)(g_slot2_ram_size / 1024));
            iprintf("slot2: Detected %s\n", def->name);
            iprintf("slot2: RAM at %p, %lu KB\n", g_slot2_ram_base, (unsigned long)(g_slot2_ram_size / 1024));
            
            detected = true;
            g_slot2_detecting = false;
            return true;
        }
        
        debug_warning("Slot-2: %s detection failed", def->name);
    }
    
    g_slot2.detected = true;
    g_slot2.type = SLOT2_NONE;
    g_slot2.size = 0;
    g_slot2.buffer = NULL;
    
    strncpy(g_slot2.name, "None", sizeof(g_slot2.name) - 1);
    g_slot2.name[sizeof(g_slot2.name) - 1] = '\0';
    
    debug_error("Slot-2: No RAM detected - tried SuperCard, M3, G6, EZ Flash");
    iprintf("slot2: No Slot-2 RAM found\n");
    
    g_slot2_detecting = false;
    return false;
#endif
}

void slot2_show_detection_ui(bool *running) {
    PrintConsole *current_console = consoleGetDefault();
    
    consoleClear();
    
    iprintf("\n=== Slot-2 RAM Detection ===\n\n");
    iprintf("Press A to auto-detect\n");
    iprintf("Press B to cancel\n\n");
    
    if (g_slot2.detected && g_slot2.buffer != NULL) {
        iprintf("Current: %s\n", g_slot2.name);
        iprintf("Size: %lu KB\n", (unsigned long)(g_slot2.size / 1024));
    } else {
        iprintf("Current: None\n");
    }
    
    iprintf("\nSlot-2 RAM provides\nexpanded memory for PSX\nemulation.\n");
    
    scanKeys();
    swiWaitForVBlank();
    
    uint32_t keys = keysDown();
    
    if (keys & KEY_A) {
        consoleClear();
        iprintf("slot2: Detecting...\n");
        
        slot2_detect();
        
        consoleClear();
        
        if (g_slot2.detected && g_slot2.buffer != NULL) {
            iprintf("\nDetected!\n\n");
            iprintf("%s\n", g_slot2.name);
            iprintf("%lu KB at %p\n", (unsigned long)(g_slot2.size / 1024), g_slot2.buffer);
        } else {
            iprintf("\nNot found\n\n");
            iprintf("No Slot-2 RAM detected.\n");
            iprintf("Check cartridge is inserted.\n");
        }
        
        swiWaitForVBlank();
        swiWaitForVBlank();
        swiWaitForVBlank();
        swiWaitForVBlank();
    }
    
    *running = false;
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