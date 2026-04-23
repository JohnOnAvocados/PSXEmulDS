#ifndef PSXEMULDS_ARM7_H
#define PSXEMULDS_ARM7_H

#include <stdbool.h>
#include <stdint.h>

#define PSX_SPU_CHANNELS 24

typedef struct {
    uint8_t active;
    uint8_t volume_left;
    uint8_t volume_right;
    uint16_t pitch;
    uint8_t adsr[4];
    uint32_t address;
    uint32_t repeat_address;
    uint8_t mode;
} PSX_SPU_Channel;

typedef struct {
    PSX_SPU_Channel channels[PSX_SPU_CHANNELS];
    uint8_t main_volume_left;
    uint8_t main_volume_right;
    uint16_t reverb_depth;
    uint8_t ctrl;
    uint8_t irq;
    uint32_t current_addr;
    uint8_t reverb_work[0x800];
    uint16_t *waveform_ram;
} PSX_SPU_State;

void arm7_main(void);
void arm7_spu_init(PSX_SPU_State *spu);
void arm7_spu_tick(PSX_SPU_State *spu);
void arm7_input_poll(uint16_t *buttons);
void arm7_ipc_send_spu(const PSX_SPU_State *spu);
void arm7_ipc_recv_spu(PSX_SPU_State *spu);

#endif