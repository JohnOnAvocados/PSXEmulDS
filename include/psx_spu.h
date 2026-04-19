#ifndef PSX_SPU_H
#define PSX_SPU_H

#include <stdbool.h>
#include <stdint.h>

#define PSX_SPU_ENABLED 1
#define PSX_SPU_SIMPLE_AUDIO 1

#define PSX_SPU_BASE_ADDR 0x1F801C00
#define PSX_SPU_SIZE 0x400

#define PSX_SPU_VOICE_COUNT 24
#define PSX_SPU_SAMPLE_RATE 44100
#define PSX_SPU_NUM_CHANNELS 2

#define PSX_SPU_MAIN_VOLUME_LEFT   0x1F801C00
#define PSX_SPU_MAIN_VOLUME_RIGHT  0x1F801C02
#define PSX_SPU_REVERB_VOLUME_L    0x1F801C04
#define PSX_SPU_REVERB_VOLUME_R    0x1F801C06
#define PSX_SPU_KEY_ON             0x1F801C88
#define PSX_SPU_KEY_OFF            0x1F801C8C
#define PSX_SPU_FM_MODE            0x1F801C90
#define PSX_SPU_NOISE_MODE         0x1F801C92
#define PSX_SPU_REVERB_BASE_ADDR   0x1F801C94
#define PSX_SPU_IRQ_ADDR           0x1F801CA4
#define PSX_SPU_TRANSFER_ADDR      0x1F801CA8
#define PSX_SPU_TRANSFER_CONTROL   0x1F801CAC
#define PSX_SPU_TRANSFER_START     0x1F801CB0

#define PSX_SPU_VOICE_BASE(n)      (0x1F801C00 + (n) * 0x10)
#define PSX_SPU_VOICE_VOL_L(n)     (PSX_SPU_VOICE_BASE(n) + 0x00)
#define PSX_SPU_VOICE_VOL_R(n)     (PSX_SPU_VOICE_BASE(n) + 0x02)
#define PSX_SPU_VOICE_FREQ(n)      (PSX_SPU_VOICE_BASE(n) + 0x04)
#define PSX_SPU_VOICE_START(n)     (PSX_SPU_VOICE_BASE(n) + 0x06)
#define PSX_SPU_VOICE_ADSR_H(n)    (PSX_SPU_VOICE_BASE(n) + 0x08)
#define PSX_SPU_VOICE_ADSR_L(n)    (PSX_SPU_VOICE_BASE(n) + 0x0A)
#define PSX_SPU_VOICE_ENV(n)       (PSX_SPU_VOICE_BASE(n) + 0x0C)
#define PSX_SPU_VOICE_MODE(n)      (PSX_SPU_VOICE_BASE(n) + 0x0E)

typedef struct {
    uint8_t enabled;
    uint16_t main_vol_left;
    uint16_t main_vol_right;
    uint16_t reverb_vol_left;
    uint16_t reverb_vol_right;
    uint16_t voice_vol_l[24];
    uint16_t voice_vol_r[24];
    uint16_t voice_freq[24];
    uint16_t voice_start[24];
    uint16_t voice_adsr_h[24];
    uint16_t voice_adsr_l[24];
    uint16_t voice_env[24];
    uint16_t voice_mode[24];
    uint8_t ram[256 * 1024];
    uint32_t transfer_addr;
    uint8_t key_on;
    uint8_t key_off;
    uint8_t reverb_base;
    uint32_t irq_addr;
    uint32_t transfer_mode;
} PsxSpuState;

void spu_init(PsxSpuState *spu);
void spu_reset(PsxSpuState *spu);
uint32_t spu_read32(const PsxSpuState *spu, uint32_t addr);
uint16_t spu_read16(const PsxSpuState *spu, uint32_t addr);
uint8_t spu_read8(const PsxSpuState *spu, uint32_t addr);
void spu_write32(PsxSpuState *spu, uint32_t addr, uint32_t value);
void spu_write16(PsxSpuState *spu, uint32_t addr, uint16_t value);
void spu_write8(PsxSpuState *spu, uint32_t addr, uint8_t value);
void spu_update(PsxSpuState *spu, int16_t *buffer, int num_samples);

#if PSX_SPU_SIMPLE_AUDIO
void spu_audio_init(void);
void spu_audio_update(PsxSpuState *spu);
void spu_audio_shutdown(void);
bool spu_audio_ready(void);
#endif

#endif
