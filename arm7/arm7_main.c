#include "arm7.h"

#include <nds.h>

static PSX_SPU_State g_spu;
static uint16_t g_buttons;

void arm7_main(void) {
    powerOn(POWER_SOUND);
    soundEnable();
    
    arm7_spu_init(&g_spu);
    
    while (1) {
        arm7_spu_tick(&g_spu);
        
        arm7_input_poll(&g_buttons);
        
        swiWaitForVBlank();
    }
}

void arm7_spu_init(PSX_SPU_State *spu) {
    int i;
    
    spu->main_volume_left = 0x3F;
    spu->main_volume_right = 0x3F;
    spu->reverb_depth = 0;
    spu->ctrl = 0;
    spu->irq = 0;
    spu->current_addr = 0;
    
    for (i = 0; i < PSX_SPU_CHANNELS; i++) {
        spu->channels[i].active = 0;
        spu->channels[i].volume_left = 0;
        spu->channels[i].volume_right = 0;
        spu->channels[i].pitch = 0;
        spu->channels[i].address = 0;
        spu->channels[i].repeat_address = 0;
        spu->channels[i].mode = 0;
    }
    
    for (i = 0; i < 0x800; i++) {
        spu->reverb_work[i] = 0;
    }
}

void arm7_spu_tick(PSX_SPU_State *spu) {
    int ch;
    
    for (ch = 0; ch < PSX_SPU_CHANNELS; ch++) {
        PSX_SPU_Channel *c = &spu->channels[ch];
        
        if (c->active) {
        }
    }
}

void arm7_input_poll(uint16_t *buttons) {
    uint32_t keys = keysHeld();
    
    *buttons = 0;
    
    if (keys & KEY_UP) *buttons |= 0x0010;
    if (keys & KEY_RIGHT) *buttons |= 0x0020;
    if (keys & KEY_DOWN) *buttons |= 0x0040;
    if (keys & KEY_LEFT) *buttons |= 0x0080;
    if (keys & KEY_SELECT) *buttons |= 0x0001;
    if (keys & KEY_START) *buttons |= 0x0008;
    if (keys & KEY_A) *buttons |= 0x1000;
    if (keys & KEY_B) *buttons |= 0x2000;
    if (keys & KEY_X) *buttons |= 0x4000;
    if (keys & KEY_Y) *buttons |= 0x8000;
    if (keys & KEY_L) *buttons |= 0x0004;
    if (keys & KEY_R) *buttons |= 0x0002;
}

void arm7_ipc_send_spu(const PSX_SPU_State *spu) {
}

void arm7_ipc_recv_spu(PSX_SPU_State *spu) {
}