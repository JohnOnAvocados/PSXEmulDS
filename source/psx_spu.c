#include "psx_spu.h"

#include <string.h>

static uint32_t spu_to_offset(uint32_t addr) {
    if (addr >= PSX_SPU_BASE_ADDR && addr < PSX_SPU_BASE_ADDR + PSX_SPU_SIZE) {
        return addr - PSX_SPU_BASE_ADDR;
    }
    return UINT32_MAX;
}

void spu_init(PsxSpuState *spu) {
    memset(spu, 0, sizeof(*spu));
    spu->enabled = 1;
    spu->main_vol_left = 0x3FFF;
    spu->main_vol_right = 0x3FFF;
}

void spu_reset(PsxSpuState *spu) {
    spu->main_vol_left = 0x3FFF;
    spu->main_vol_right = 0x3FFF;
    spu->reverb_vol_left = 0;
    spu->reverb_vol_right = 0;
    spu->key_on = 0;
    spu->key_off = 0;
    spu->transfer_addr = 0;
    spu->transfer_mode = 0;
    memset(spu->voice_vol_l, 0, sizeof(spu->voice_vol_l));
    memset(spu->voice_vol_r, 0, sizeof(spu->voice_vol_r));
    memset(spu->voice_freq, 0, sizeof(spu->voice_freq));
    memset(spu->voice_start, 0, sizeof(spu->voice_start));
    memset(spu->voice_adsr_h, 0, sizeof(spu->voice_adsr_h));
    memset(spu->voice_adsr_l, 0, sizeof(spu->voice_adsr_l));
    memset(spu->voice_env, 0, sizeof(spu->voice_env));
    memset(spu->voice_mode, 0, sizeof(spu->voice_mode));
}

uint32_t spu_read32(const PsxSpuState *spu, uint32_t addr) {
    uint32_t offset = spu_to_offset(addr);
    if (offset == UINT32_MAX) {
        return 0;
    }
    if (offset >= 0x80 && offset < 0x80 + 24 * 0x10) {
        uint32_t voice_index = (offset - 0x80) / 0x10;
        uint32_t field_offset = (offset - 0x80) % 0x10;
        switch (field_offset) {
        case 0x04:
            return spu->voice_freq[voice_index];
        case 0x06:
            return spu->voice_start[voice_index];
        case 0x08:
            return spu->voice_adsr_h[voice_index];
        case 0x0A:
            return spu->voice_adsr_l[voice_index];
        case 0x0C:
            return spu->voice_env[voice_index];
        case 0x0E:
            return spu->voice_mode[voice_index];
        default:
            return 0;
        }
    }
    return 0;
}

uint16_t spu_read16(const PsxSpuState *spu, uint32_t addr) {
    uint32_t offset = spu_to_offset(addr);
    if (offset == UINT32_MAX) {
        return 0;
    }
    switch (offset) {
    case 0x00:
        return spu->main_vol_left;
    case 0x02:
        return spu->main_vol_right;
    case 0x04:
        return spu->reverb_vol_left;
    case 0x06:
        return spu->reverb_vol_right;
    case 0x88:
        return spu->key_on;
    case 0x8C:
        return spu->key_off;
    default:
        if (offset >= 0x80 && offset < 0x80 + 24 * 0x10) {
            uint32_t voice_index = (offset - 0x80) / 0x10;
            uint32_t field_offset = (offset - 0x80) % 0x10;
            switch (field_offset) {
            case 0x00:
                return spu->voice_vol_l[voice_index];
            case 0x02:
                return spu->voice_vol_r[voice_index];
            case 0x04:
                return spu->voice_freq[voice_index];
            case 0x06:
                return spu->voice_start[voice_index];
            case 0x08:
                return spu->voice_adsr_h[voice_index];
            case 0x0A:
                return spu->voice_adsr_l[voice_index];
            case 0x0C:
                return spu->voice_env[voice_index];
            case 0x0E:
                return spu->voice_mode[voice_index];
            default:
                return 0;
            }
        }
        return 0;
    }
}

uint8_t spu_read8(const PsxSpuState *spu, uint32_t addr) {
    uint32_t offset = spu_to_offset(addr);
    if (offset == UINT32_MAX) {
        return 0;
    }
    if (offset < 0x100) {
        return ((uint8_t*)spu)[offset];
    }
    return 0;
}

void spu_write32(PsxSpuState *spu, uint32_t addr, uint32_t value) {
    uint32_t offset = spu_to_offset(addr);
    if (offset == UINT32_MAX) {
        return;
    }
    if (offset >= 0x80 && offset < 0x80 + 24 * 0x10) {
        uint32_t voice_index = (offset - 0x80) / 0x10;
        uint32_t field_offset = (offset - 0x80) % 0x10;
        switch (field_offset) {
        case 0x04:
            spu->voice_freq[voice_index] = value & 0xFFFF;
            break;
        case 0x06:
            spu->voice_start[voice_index] = value & 0xFFFF;
            break;
        case 0x08:
            spu->voice_adsr_h[voice_index] = value & 0xFFFF;
            break;
        case 0x0A:
            spu->voice_adsr_l[voice_index] = value & 0xFFFF;
            break;
        case 0x0C:
            spu->voice_env[voice_index] = value & 0xFFFF;
            break;
        case 0x0E:
            spu->voice_mode[voice_index] = value & 0xFFFF;
            break;
        }
    }
}

void spu_write16(PsxSpuState *spu, uint32_t addr, uint16_t value) {
    uint32_t offset = spu_to_offset(addr);
    if (offset == UINT32_MAX) {
        return;
    }
    switch (offset) {
    case 0x00:
        spu->main_vol_left = value;
        break;
    case 0x02:
        spu->main_vol_right = value;
        break;
    case 0x04:
        spu->reverb_vol_left = value;
        break;
    case 0x06:
        spu->reverb_vol_right = value;
        break;
    case 0x88:
        spu->key_on = value & 0xFF;
        break;
    case 0x8C:
        spu->key_off = value & 0xFF;
        break;
    case 0x94:
        spu->reverb_base = value & 0xFF;
        break;
    default:
        if (offset >= 0x80 && offset < 0x80 + 24 * 0x10) {
            uint32_t voice_index = (offset - 0x80) / 0x10;
            uint32_t field_offset = (offset - 0x80) % 0x10;
            switch (field_offset) {
            case 0x00:
                spu->voice_vol_l[voice_index] = value;
                break;
            case 0x02:
                spu->voice_vol_r[voice_index] = value;
                break;
            case 0x04:
                spu->voice_freq[voice_index] = value;
                break;
            case 0x06:
                spu->voice_start[voice_index] = value;
                break;
            case 0x08:
                spu->voice_adsr_h[voice_index] = value;
                break;
            case 0x0A:
                spu->voice_adsr_l[voice_index] = value;
                break;
            case 0x0C:
                spu->voice_env[voice_index] = value;
                break;
            case 0x0E:
                spu->voice_mode[voice_index] = value;
                break;
            }
        }
        break;
    }
}

void spu_write8(PsxSpuState *spu, uint32_t addr, uint8_t value) {
    uint32_t offset = spu_to_offset(addr);
    if (offset == UINT32_MAX) {
        return;
    }
    if (offset < 0x100) {
        ((uint8_t*)spu)[offset] = value;
    }
}

void spu_update(PsxSpuState *spu, int16_t *buffer, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        buffer[i * 2 + 0] = 0;
        buffer[i * 2 + 1] = 0;
    }
}
