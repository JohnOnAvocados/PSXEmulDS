#ifndef PSX_SLOT2_H
#define PSX_SLOT2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SLOT2_NONE            0
#define SLOT2_SUPERCARD       1
#define SLOT2_GBAEXP          2
#define SLOT2_M3SD            3
#define SLOT2_DSTT            4
#define SLOT2_SUPERCHIS       5
#define SLOT2_ACE3DS         6

#define SLOT2_MAX_SIZE        (32 * 1024 * 1024)
#define SLOT2_SUPERCHIS_SDRAM (32 * 1024 * 1024)
#define SLOT2_SUPERCHIS_FRAM  (128 * 1024)
#define SLOT2_SUPERCHIS_NOR    (128 * 1024 * 1024)

#define SLOT2_BANK_SIZE      (2 * 1024 * 1024)
#define SLOT2_BANK_COUNT     (SLOT2_MAX_SIZE / SLOT2_BANK_SIZE)

typedef struct {
    uint8_t type;
    uint8_t *buffer;
    size_t size;
    size_t max_size;
    bool initialized;
    bool detected;
    bool writable;
    char name[32];
    uint32_t sdram_base;
    uint32_t fram_base;
    uint32_t nor_base;
    uint8_t current_bank;
    uint8_t bank_count;
} Slot2Device;

void slot2_init(void);
void slot2_deinit(void);
Slot2Device* slot2_get_device(void);
bool slot2_detect(void);
size_t slot2_get_ram_size(void);
bool slot2_set_bank(uint8_t bank);
uint8_t slot2_get_bank(void);
bool slot2_has_ram(void);
bool slot2_read(Slot2Device *dev, uint32_t addr, uint8_t *data, size_t len);
bool slot2_write(Slot2Device *dev, uint32_t addr, const uint8_t *data, size_t len);
uint8_t slot2_read8(Slot2Device *dev, uint32_t addr);
uint16_t slot2_read16(Slot2Device *dev, uint32_t addr);
uint32_t slot2_read32(Slot2Device *dev, uint32_t addr);
void slot2_write8(Slot2Device *dev, uint32_t addr, uint8_t value);
void slot2_write16(Slot2Device *dev, uint32_t addr, uint16_t value);
void slot2_write32(Slot2Device *dev, uint32_t addr, uint32_t value);

#endif
