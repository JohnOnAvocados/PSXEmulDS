#include "psx_mdec.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

static const uint8_t mdec_zigzag[64] = {
    0,  1,  8, 16,  9,  2,  3, 10,
   17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34,
   27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36,
   29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46,
   53, 60, 61, 54, 47, 55, 62, 63
};

static const uint8_t mdec_default_quantum[64] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

static const int16_t mdec_default_iquant[64] = {
    16,  11,  10,  16,  24,  40,  51,  61,
    12,  12,  14,  19,  26,  58,  60,  55,
    14,  13,  16,  24,  40,  57,  69,  56,
    14,  17,  22,  29,  51,  87,  80,  62,
    18,  22,  37,  56,  68,  109,  103, 77,
    24,  35,  55,  64,  81,  104,  113, 92,
    49,  64,  78,  87,  103, 121,  120, 101,
    72,  92,  95,  98,  112, 100,  103,  99
};

static void mdec_idct_col(int16_t *block, int col) {
    int16_t tmp[8];
    
    for (int i = 0; i < 8; i++) {
        if (block[i * 8] == 0 && block[1 * 8] == 0 && block[2 * 8] == 0 &&
            block[3 * 8] == 0 && block[4 * 8] == 0 && block[5 * 8] == 0 &&
            block[6 * 8] == 0 && block[7 * 8] == 0) {
            tmp[i] = block[0] / 8;
        } else {
            int32_t sum = 0;
            for (int j = 0; j < 8; j++) {
                if (block[j * 8] != 0) {
                    double cos_val = cos((2 * i + 1) * j * M_PI / 16.0);
                    sum += (int32_t)(block[j * 8] * cos_val * (j == 0 ? 0.353553 : 0.490393));
                }
            }
            tmp[i] = (int16_t)((sum + 0x8000) >> 16);
        }
    }
    
    for (int i = 0; i < 8; i++) {
        block[i * 8] = tmp[i];
    }
}

static void mdec_idct_row(int16_t *block, int row) {
    int16_t tmp[8];
    int offset = row * 8;
    
    for (int i = 0; i < 8; i++) {
        if (block[offset] == 0 && block[offset + 1] == 0 && block[offset + 2] == 0 &&
            block[offset + 3] == 0 && block[offset + 4] == 0 && block[offset + 5] == 0 &&
            block[offset + 6] == 0 && block[offset + 7] == 0) {
            tmp[i] = block[offset] / 8;
        } else {
            int32_t sum = 0;
            for (int j = 0; j < 8; j++) {
                if (block[offset + j] != 0) {
                    double cos_val = cos((2 * i + 1) * j * M_PI / 16.0);
                    sum += (int32_t)(block[offset + j] * cos_val * (j == 0 ? 0.353553 : 0.490393));
                }
            }
            tmp[i] = (int16_t)((sum + 0x8000) >> 16);
        }
    }
    
    for (int i = 0; i < 8; i++) {
        block[offset + i] = tmp[i];
    }
}

void mdec_init(PsxMdecState *mdec) {
    memset(mdec, 0, sizeof(*mdec));
    
    mdec->frame_width = 320;
    mdec->frame_height = 240;
    mdec->quality = 0;
    mdec->half_frame = false;
    
    memcpy(mdec->quantum, mdec_default_quantum, 64);
    memcpy(mdec->iquant, mdec_default_iquant, 64);
    
    mdec->status = PSX_MDEC_STATUS_READY;
    mdec->hw_scale = 256;
}

void mdec_reset(PsxMdecState *mdec) {
    mdec->control = 0;
    mdec->command = 0;
    mdec->status = PSX_MDEC_STATUS_READY | PSX_MDEC_STATUS_RESET;
    mdec->output_ptr = 0;
    mdec->output_size = 0;
    mdec->decoding = false;
    mdec->hw_scale = 256;
    
    memcpy(mdec->quantum, mdec_default_quantum, 64);
    memcpy(mdec->iquant, mdec_default_iquant, 64);
}

uint32_t mdec_read32(PsxMdecState *mdec, uint32_t addr) {
    uint32_t offset = addr - PSX_MDEC_BASE_ADDR;
    
    switch (offset) {
    case 0:
        return mdec->control;
    case 4:
        return mdec->status;
    case 8:
        return mdec->command;
    case 0x18:
        return mdec->hw_width;
    case 0x1C:
        return mdec->hw_height;
    case 0x20:
        return mdec->hw_scale;
    case 0x24:
        return mdec->hw_picture_code;
    default:
        return 0;
    }
}

void mdec_write32(PsxMdecState *mdec, uint32_t addr, uint32_t value) {
    uint32_t offset = addr - PSX_MDEC_BASE_ADDR;
    
    switch (offset) {
    case 0:
        mdec->control = value;
        if (value & PSX_MDEC_CONTROL_RESET) {
            mdec_reset(mdec);
        }
        if (value & PSX_MDEC_CONTROL_DECODE) {
            mdec->decoding = true;
            mdec->status &= ~PSX_MDEC_STATUS_EOF;
        }
        break;
    case 4:
        mdec->status = value & 0x7FFFFFFF;
        break;
    case 8:
        mdec->command = value;
        
        switch (value & 0xFF) {
        case PSX_MDEC_CMD_SETSCALE:
            mdec->hw_scale = value >> 8;
            break;
        default:
            break;
        }
        break;
    case 0x14:
        if (mdec->quantum[mdec->output_ptr % 64] != 0) {
            mdec->quantum[mdec->output_ptr % 64] = (uint8_t)((value >> 8) & 0xFF);
            mdec->iquant[mdec->output_ptr % 64] = (int16_t)(256 / mdec->quantum[mdec->output_ptr % 64]);
        }
        mdec->output_ptr++;
        break;
    default:
        break;
    }
}

void mdec_decode_frame(PsxMdecState *mdec, const uint8_t *data, uint32_t size) {
    if (!mdec->decoding) {
        return;
    }
    
    memset(mdec->dct, 0, sizeof(mdec->dct));
    
    uint32_t ptr = 0;
    
    for (int i = 0; i < 64 && ptr < size; i++) {
        int idx = mdec_zigzag[i];
        
        if (ptr < size && data[ptr] != 0) {
            int16_t val = (int16_t)(data[ptr] & 0xFF);
            if (val > 127) val -= 256;
            mdec->dct[idx] = (int16_t)(val * (int16_t)mdec->quantum[idx]);
        }
        ptr++;
    }
    
    for (int i = 0; i < 8; i++) {
        mdec_idct_col(mdec->dct, i);
    }
    for (int i = 0; i < 8; i++) {
        mdec_idct_row(mdec->dct, i);
    }
    
    uint32_t output_w = mdec->half_frame ? mdec->frame_width / 2 : mdec->frame_width;
    uint32_t out_idx = mdec->output_size;
    
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int16_t val = mdec->dct[y * 8 + x];
            int y_out = mdec->half_frame ? y * 2 : y;
            int x_out = mdec->half_frame ? x * 2 : x;
            
            if (out_idx + 1 < sizeof(mdec->output_buffer)) {
                mdec->output_buffer[out_idx++] = (uint8_t)((val >> 8) & 0xFF);
                mdec->output_buffer[out_idx++] = (uint8_t)(val & 0xFF);
            }
        }
    }
    
    mdec->output_size = out_idx;
    mdec->status |= PSX_MDEC_STATUS_EOF;
    mdec->decoding = false;
}

uint8_t *mdec_get_output(PsxMdecState *mdec, uint32_t *size) {
    if (size) {
        *size = mdec->output_size;
    }
    return mdec->output_buffer;
}