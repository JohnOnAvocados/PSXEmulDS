#include "psx_spu.h"

#include <string.h>
#include <stdio.h>

#if PSX_SPU_ENABLED

static const int16_t spu_adpcm_table[16] = {
    0, 1, 2, 4, 8, 16, 32, 64,
    -128, -256, -512, -1024, -2048, -4096, -8192, -16384
};

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

static void spu_process_envelope(PsxSpuState *spu, int voice) {
    uint16_t adsr_h = spu->voice_adsr_h[voice];
    uint16_t adsr_l = spu->voice_adsr_l[voice];
    uint16_t env = spu->voice_env[voice];
    
    uint8_t attack = (adsr_h >> 8) & 0x1F;
    uint8_t decay = adsr_h & 0x1F;
    uint8_t sustain = (adsr_l >> 8) & 0x7F;
    uint8_t release = adsr_l & 0x1F;
    
    uint8_t key_on = (spu->key_on >> voice) & 1;
    uint8_t key_off = (spu->key_off >> voice) & 1;
    
    uint8_t state = (env >> 14) & 3;
    int16_t level = env & 0x7FFF;
    
    if (key_on && state == 0) {
        state = 1;
        level = 0;
    } else if (key_off) {
        state = 4;
    }
    
    switch (state) {
    case 1:
        if (attack > 0) {
            level += (0x8000 / (attack * 10));
            if (level >= 0x7FFF) {
                level = 0x7FFF;
                state = 2;
            }
        }
        break;
    case 2:
        level = 0x7FFF - ((0x7FFF - level) * decay / 32);
        break;
    case 4:
        if (release > 0) {
            level -= (level * release / 32);
            if (level < 0) level = 0;
        }
        break;
    }
    
    spu->voice_env[voice] = (state << 14) | (level & 0x7FFF);
}

void spu_update(PsxSpuState *spu, int16_t *buffer, int num_samples) {
#if PSX_SPU_ENABLED
    for (int v = 0; v < 24; v++) {
        if (spu->voice_mode[v] & 0x3FF) {
            spu_process_envelope(spu, v);
        }
    }

    for (int i = 0; i < num_samples; i++) {
        int32_t sample_l = 0;
        int32_t sample_r = 0;

        for (int v = 0; v < 24; v++) {
            uint16_t env = spu->voice_env[v];
            uint8_t state = (env >> 14) & 3;
            if (state == 0) continue;

            int16_t level = (int16_t)(env & 0x7FFF);
            if (level == 0) continue;

            uint16_t vol_l = spu->voice_vol_l[v];
            uint16_t vol_r = spu->voice_vol_r[v];

            int32_t voice_sample = (level >> 8);
            sample_l += (voice_sample * (int16_t)vol_l) >> 14;
            sample_r += (voice_sample * (int16_t)vol_r) >> 14;
        }

        sample_l = (sample_l * (int16_t)spu->main_vol_left) >> 14;
        sample_r = (sample_r * (int16_t)spu->main_vol_right) >> 14;

        sample_l = sample_l > 32767 ? 32767 : (sample_l < -32768 ? -32768 : sample_l);
        sample_r = sample_r > 32767 ? 32767 : (sample_r < -32768 ? -32768 : sample_r);

        buffer[i * 2 + 0] = (int16_t)sample_l;
        buffer[i * 2 + 1] = (int16_t)sample_r;
    }
#else
    for (int i = 0; i < num_samples; i++) {
        buffer[i * 2 + 0] = 0;
        buffer[i * 2 + 1] = 0;
    }
#endif
}

#if PSX_SPU_SIMPLE_AUDIO

#include <nds.h>

#define SPU_AUDIO_BUFFER_SIZE 1024

static int16_t g_audio_buffer[SPU_AUDIO_BUFFER_SIZE * 2];
static int g_audio_channel = -1;
static bool g_audio_initialized = false;
static bool g_audio_ready = false;

void spu_audio_init(void) {
    if (g_audio_initialized) {
        return;
    }

    soundEnable();
    g_audio_initialized = true;
    g_audio_ready = true;

    memset(g_audio_buffer, 0, sizeof(g_audio_buffer));

    g_audio_channel = soundPlaySample(
        g_audio_buffer,
        SoundFormat_16Bit,
        sizeof(g_audio_buffer),
        PSX_SPU_SAMPLE_RATE,
        64,
        64,
        true,
        0
    );

    if (g_audio_channel >= 0) {
        iprintf("SPU audio: channel %d\n", g_audio_channel);
    } else {
        iprintf("SPU audio: failed to start\n");
        g_audio_ready = false;
    }
}

void spu_audio_update(PsxSpuState *spu) {
    if (!g_audio_ready || g_audio_channel < 0) {
        return;
    }

    static int16_t mixed_buffer[SPU_AUDIO_BUFFER_SIZE * 2];
    spu_update(spu, mixed_buffer, SPU_AUDIO_BUFFER_SIZE);
}

bool spu_audio_ready(void) {
    return g_audio_ready;
}

void spu_audio_shutdown(void) {
    if (g_audio_channel >= 0) {
        soundKill(g_audio_channel);
        g_audio_channel = -1;
    }
    soundDisable();
    g_audio_initialized = false;
    g_audio_ready = false;
}

#endif
#endif
