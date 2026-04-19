#include "psx_pad.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define PAD_STATE_IDLE 0
#define PAD_STATE_READING 1
#define PAD_STATE_CONFIG 2
#define PAD_STATE_READY 3

static uint16_t g_buttons = 0;
static int8_t g_analog_left_x = 128;
static int8_t g_analog_left_y = 128;
static int8_t g_analog_right_x = 128;
static int8_t g_analog_right_y = 128;

void pad_init(PsxPadState *pad) {
    memset(pad, 0, sizeof(*pad));
    pad->inserted = true;
    pad->id = PSX_PAD_ID_DIGITAL;
    pad->status = 0x5A;
    pad->poll_delay = 1;
}

void pad_reset(PsxPadState *pad) {
    pad->command = 0;
    pad->mode = PSX_PAD_MODE_DIGITAL;
    pad->state = PAD_STATE_IDLE;
    pad->poll_byte = 0;
    pad->poll_delay = 1;
}

uint8_t pad_read8(PsxPadState *pad, uint32_t addr) {
    uint32_t offset = addr - PSX_PAD_BASE_ADDR;
    if (offset >= PSX_PAD_SIZE) {
        return 0xFF;
    }
    
    switch (offset) {
    case 0:
        if (pad->inserted) {
            uint8_t stat = 0x14;
            if (pad->state == PAD_STATE_READY) stat |= 0x40;
            return stat;
        }
        return 0xFF;
    case 1:
        if (!pad->inserted) return 0xFF;
        switch (pad->poll_byte++) {
        case 0:
            return pad->id;
        case 1:
            return (pad->buttons_low & 0xFF);
        case 2:
            return ((pad->buttons_low >> 8) & 0xFF);
        case 3:
            return (pad->buttons_high & 0xFF);
        default:
            if (pad->analog_mode) {
                if (pad->poll_byte == 4) return pad->left_x;
                if (pad->poll_byte == 5) return pad->left_y;
                if (pad->poll_byte == 6) return pad->right_x;
                if (pad->poll_byte == 7) return pad->right_y;
            }
            return 0;
        }
    default:
        return 0xFF;
    }
}

void pad_write8(PsxPadState *pad, uint32_t addr, uint8_t value) {
    uint32_t offset = addr - PSX_PAD_BASE_ADDR;
    if (offset >= PSX_PAD_SIZE) {
        return;
    }
    
    if (offset == 0) {
        pad->command = value;
        
        switch (value) {
        case PSX_PAD_COMMAND_GETMODE1:
            pad->state = PAD_STATE_READY;
            pad->poll_byte = 0;
            break;
            
        case PSX_PAD_COMMAND_GETSTATUS:
            pad->state = PAD_STATE_READY;
            pad->poll_byte = 0;
            break;
            
        case PSX_PAD_COMMAND_READ:
            if (pad->state != PAD_STATE_CONFIG) {
                pad->buttons_low = g_buttons & 0xFF;
                pad->buttons_high = (g_buttons >> 8) & 0xFF;
                pad->left_x = g_analog_left_x;
                pad->left_y = g_analog_left_y;
                pad->right_x = g_analog_right_x;
                pad->right_y = g_analog_right_y;
                pad->state = PAD_STATE_READY;
                pad->poll_byte = 0;
            }
            break;
            
        case PSX_PAD_COMMAND_CONFIG:
            pad->state = PAD_STATE_CONFIG;
            pad->poll_byte = 0;
            break;
            
        case PSX_PAD_COMMAND_SETMODE:
            pad->analog_mode = (value == PSX_PAD_MODE_ANALOG);
            if (pad->analog_mode) {
                pad->id = PSX_PAD_ID_ANALOG;
            } else {
                pad->id = PSX_PAD_ID_DIGITAL;
            }
            break;
        }
    }
}

void pad_update(PsxPadState *pad) {
    if (pad->poll_byte >= 8) {
        pad->poll_byte = 0;
    }
    if (pad->poll_delay > 0) {
        pad->poll_delay--;
    }
}

void pad_set_buttons(PsxPadState *pad, uint16_t buttons) {
    g_buttons = buttons;
    pad->buttons_low = buttons & 0xFF;
    pad->buttons_high = (buttons >> 8) & 0xFF;
}

void pad_set_analog(PsxPadState *pad, int8_t lx, int8_t ly, int8_t rx, int8_t ry) {
    g_analog_left_x = lx;
    g_analog_left_y = ly;
    g_analog_right_x = rx;
    g_analog_right_y = ry;
    pad->left_x = lx;
    pad->left_y = ly;
    pad->right_x = rx;
    pad->right_y = ry;
}

void memcard_init(PsxMemCardState *card) {
    memset(card, 0, sizeof(*card));
    card->cluster_size = 512;
    card->inserted = false;
}

void memcard_reset(PsxMemCardState *card) {
    card->size = 0;
    card->dirty = 0;
}

uint8_t memcard_read8(PsxMemCardState *card, uint32_t addr) {
    if (!card->inserted || !card->memory) {
        return 0xFF;
    }
    
    if (addr >= card->size) {
        return 0xFF;
    }
    
    return card->memory[addr];
}

void memcard_write8(PsxMemCardState *card, uint32_t addr, uint8_t value) {
    if (!card->inserted || !card->memory) {
        return;
    }
    
    if (addr >= card->size) {
        return;
    }
    
    if (card->memory[addr] != value) {
        card->memory[addr] = value;
        card->dirty = 1;
    }
}

void memcard_insert(PsxMemCardState *card, const char *filename) {
    if (card->memory) {
        free(card->memory);
    }
    
    card->memory = NULL;
    card->size = 0;
    card->inserted = false;
    
    if (!filename) {
        return;
    }
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fp = fopen(filename, "wb");
        if (fp) {
            card->memory = calloc(128 * 1024, 1);
            if (card->memory) {
                card->size = 128 * 1024;
                card->inserted = true;
                fwrite(card->memory, 1, card->size, fp);
            }
            fclose(fp);
        }
        return;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size > 0) {
        card->memory = malloc(size);
        if (card->memory) {
            fread(card->memory, 1, size, fp);
            card->size = size;
            card->inserted = true;
        }
    }
    
    fclose(fp);
}