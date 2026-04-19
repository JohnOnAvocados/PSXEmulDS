#ifndef PSX_PAD_H
#define PSX_PAD_H

#include <stdbool.h>
#include <stdint.h>

#define PSX_PAD_BASE_ADDR 0x1F801400
#define PSX_PAD_SIZE 0x20

#define PSX_PAD_1_STATUS  0x1F801400
#define PSX_PAD_1_DATA  0x1F801401

#define PSX_PAD_2_STATUS 0x1F801404
#define PSX_PAD_2_DATA 0x1F801405

#define PSX_PAD_COMMAND_GETMODE1    0x01
#define PSX_PAD_COMMAND_GETSTATUS   0x02
#define PSX_PAD_COMMAND_READ      0x42
#define PSX_PAD_COMMAND_CONFIG   0x43
#define PSX_PAD_COMMAND_SETMODE 0x44

#define PSX_PAD_MODE_DIGITAL    0x00
#define PSX_PAD_MODE_ANALOG   0x01
#define PSX_PAD_MODE_NEGCON  0x02
#define PSX_PAD_MODE_ANALOG_PRESS 0x12

#define PSX_PAD_ID_DIGITAL       0x41
#define PSX_PAD_ID_ANALOG       0x73
#define PSX_PAD_ID_NEGCON       0x23
#define PSX_PAD_ID_ANALOG_PRESS 0x53

#define PSX_PAD_BUTTON_SELECT   (1 << 0)
#define PSX_PAD_BUTTON_L3      (1 << 1)
#define PSX_PAD_BUTTON_R3       (1 << 2)
#define PSX_PAD_BUTTON_START   (1 << 3)
#define PSX_PAD_DPAD_UP         (1 << 4)
#define PSX_PAD_DPAD_RIGHT     (1 << 5)
#define PSX_PAD_DPAD_DOWN      (1 << 6)
#define PSX_PAD_DPAD_LEFT      (1 << 7)
#define PSX_PAD_BUTTON_L2       (1 << 8)
#define PSX_PAD_BUTTON_R2      (1 << 9)
#define PSX_PAD_BUTTON_L1       (1 << 10)
#define PSX_PAD_BUTTON_R1       (1 << 11)
#define PSX_PAD_BUTTON_TRIANGLE (1 << 12)
#define PSX_PAD_BUTTON_CIRCLE   (1 << 13)
#define PSX_PAD_BUTTON_CROSS    (1 << 14)
#define PSX_PAD_BUTTON_SQUARE  (1 << 15)

#define PSX_PAD_HORIZONTAL_MAX 255
#define PSX_PAD_VERTICAL_MAX 255

typedef struct {
    uint8_t command;
    uint8_t mode;
    uint8_t id;
    uint8_t status;
    uint8_t buttons_low;
    uint8_t buttons_high;
    int8_t right_x;
    int8_t right_y;
    int8_t left_x;
    int8_t left_y;
    uint8_t state;
    uint8_t poll_byte;
    uint16_t poll_delay;
    bool inserted;
    bool analog_mode;
} PsxPadState;

typedef struct {
    uint8_t data[128];
    uint32_t size;
    uint32_t dirty;
    uint8_t *memory;
    bool inserted;
    uint32_t cluster_size;
} PsxMemCardState;

void pad_init(PsxPadState *pad);
void pad_reset(PsxPadState *pad);
uint8_t pad_read8(PsxPadState *pad, uint32_t addr);
void pad_write8(PsxPadState *pad, uint32_t addr, uint8_t value);
void pad_update(PsxPadState *pad);
void pad_set_buttons(PsxPadState *pad, uint16_t buttons);
void pad_set_analog(PsxPadState *pad, int8_t lx, int8_t ly, int8_t rx, int8_t ry);

void memcard_init(PsxMemCardState *card);
void memcard_reset(PsxMemCardState *card);
uint8_t memcard_read8(PsxMemCardState *card, uint32_t addr);
void memcard_write8(PsxMemCardState *card, uint32_t addr, uint8_t value);
void memcard_insert(PsxMemCardState *card, const char *filename);

#endif