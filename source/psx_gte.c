#include "psx_gte.h"
#include <string.h>
#include <math.h>

void gte_init(PsxGteState *gte) {
    memset(gte, 0, sizeof(*gte));
    gte->ctrl.h = 256;
    gte->ctrl.zsf3 = 0x1000 / 3;
    gte->ctrl.zsf4 = 0x1000 / 4;
}

void gte_reset(PsxGteState *gte) {
    memset(&gte->data, 0, sizeof(gte->data));
    memset(&gte->ctrl, 0, sizeof(gte->ctrl));
    memset(gte->mac, 0, sizeof(gte->mac));
    memset(gte->ir, 0, sizeof(gte->ir));
    memset(gte->screen_xy, 0, sizeof(gte->screen_xy));
    memset(gte->screen_z, 0, sizeof(gte->screen_z));
    memset(gte->color_fifo, 0, sizeof(gte->color_fifo));
    gte->ctrl.h = 256;
    gte->ctrl.zsf3 = 0x1000 / 3;
    gte->ctrl.zsf4 = 0x1000 / 4;
    gte->flag = 0;
    gte->sx2 = 0;
    gte->sy2 = 0;
}

static int32_t gte_saturate(int64_t val, int bits, int signed_val) {
    int64_t max = (1LL << (bits - 1)) - 1;
    int64_t min = signed_val ? -(1LL << (bits - 1)) : 0;
    if (val > max) return (int32_t)max;
    if (val < min) return (int32_t)min;
    return (int32_t)val;
}

static void gte_check_overflow(PsxGteState *gte, int64_t mac, int bit) {
    int oversize = (mac > 0x7FFFFFFFFFFFLL) || (mac < -0x80000000000LL);
    if (oversize) {
        gte->flag |= (1 << (bit + 16));
    }
}

static int32_t gte_sar(int64_t val, int shift) {
    if (shift == 0) return (int32_t)val;
    if (val < 0) return (int32_t)((val >> shift) | (0xFFFFFFFFFFFFFFFFLL << (64 - shift)));
    return (int32_t)(val >> shift);
}

static void gte_command_rtps(PsxGteState *gte, int sf) {
    int32_t *rt = (int32_t*)&gte->ctrl.rt11;
    int32_t vx0 = (int16_t)(gte->data.vxy0 & 0xFFFF);
    int32_t vy0 = (int16_t)((gte->data.vxy0 >> 16) & 0xFFFF);
    int32_t vz0 = (int16_t)(gte->data.vz0 & 0xFFFF);
    
    int32_t h = gte->ctrl.h;
    int32_t ofx = gte->ctrl.ofx;
    int32_t ofy = gte->ctrl.ofy;
    int32_t dqa = gte->ctrl.dqa;
    int32_t dqb = gte->ctrl.dqb;
    int32_t tr_x = gte->ctrl.tr_x;
    int32_t tr_y = gte->ctrl.tr_y;
    int32_t tr_z = gte->ctrl.tr_z;
    
    int shift = sf ? 12 : 0;
    
    gte->mac[1] = (((int64_t)tr_x << 12) + rt[0]*vx0 + rt[1]*vy0 + rt[2]*vz0) >> (12 + shift);
    gte->mac[2] = (((int64_t)tr_y << 12) + rt[3]*vx0 + rt[4]*vy0 + rt[5]*vz0) >> (12 + shift);
    gte->mac[3] = (((int64_t)tr_z << 12) + rt[6]*vx0 + rt[7]*vy0 + rt[8]*vz0) >> (12 + shift);
    
    gte->ir[1] = gte_saturate(gte->mac[1], 16, 1);
    gte->ir[2] = gte_saturate(gte->mac[2], 16, 1);
    gte->ir[3] = gte_saturate(gte->mac[3], 16, 1);
    
    gte->data.sz3 = (uint16_t)gte_saturate(gte->mac[3], 16, 0);
    
    int64_t sz3 = gte->data.sz3;
    if (sz3 == 0) sz3 = 1;
    
    int64_t factor = (((h * 0x20000) / sz3) + 1) / 2;
    if (factor > 0x1FFFF) {
        factor = 0x1FFFF;
        gte->flag |= (1 << 17);
    }
    
    gte->mac[0] = factor * gte->ir[1] + ofx;
    gte->sx2 = (int16_t)gte_saturate(gte->mac[0] >> 16, 12, 1);
    
    gte->mac[0] = factor * gte->ir[2] + ofy;
    gte->sy2 = (int16_t)gte_saturate(gte->mac[0] >> 16, 12, 1);
    
    gte->mac[0] = factor * dqa + dqb;
    gte->data.ir0 = (int16_t)gte_saturate(gte->mac[0] >> 12, 12, 0);
    
    for (int i = 0; i < 3; i++) {
        gte->screen_xy[i*2] = gte->screen_xy[(i+1)*2];
        gte->screen_xy[i*2+1] = gte->screen_xy[(i+1)*2+1];
    }
    gte->screen_xy[4] = gte->sx2;
    gte->screen_xy[5] = gte->sy2;
    
    for (int i = 0; i < 3; i++) {
        gte->screen_z[i] = gte->screen_z[i+1];
    }
    gte->screen_z[3] = gte->data.sz3;
    
    gte->data.rgbc = gte->data.rgbc;
}

static void gte_command_rtpt(PsxGteState *gte, int sf) {
    gte_command_rtps(gte, sf);
    
    int32_t *rt = (int32_t*)&gte->ctrl.rt11;
    int32_t h = gte->ctrl.h;
    int32_t ofx = gte->ctrl.ofx;
    int32_t ofy = gte->ctrl.ofy;
    int32_t dqa = gte->ctrl.dqa;
    int32_t dqb = gte->ctrl.dqb;
    int32_t tr_x = gte->ctrl.tr_x;
    int32_t tr_y = gte->ctrl.tr_y;
    int32_t tr_z = gte->ctrl.tr_z;
    
    for (int v = 1; v <= 2; v++) {
        int32_t vxn = (v == 1) ? (int16_t)(gte->data.vxy1 & 0xFFFF) : (int16_t)(gte->data.vxy2 & 0xFFFF);
        int32_t vyn = (v == 1) ? (int16_t)((gte->data.vxy1 >> 16) & 0xFFFF) : (int16_t)((gte->data.vxy2 >> 16) & 0xFFFF);
        int32_t vzn = (v == 1) ? (int16_t)(gte->data.vz1 & 0xFFFF) : (int16_t)(gte->data.vz2 & 0xFFFF);
        
        int shift = sf ? 12 : 0;
        
        gte->mac[1] = (((int64_t)tr_x << 12) + rt[0]*vxn + rt[1]*vyn + rt[2]*vzn) >> (12 + shift);
        gte->mac[2] = (((int64_t)tr_y << 12) + rt[3]*vxn + rt[4]*vyn + rt[5]*vzn) >> (12 + shift);
        gte->mac[3] = (((int64_t)tr_z << 12) + rt[6]*vxn + rt[7]*vyn + rt[8]*vzn) >> (12 + shift);
        
        gte->ir[1] = gte_saturate(gte->mac[1], 16, 1);
        gte->ir[2] = gte_saturate(gte->mac[2], 16, 1);
        gte->ir[3] = gte_saturate(gte->mac[3], 16, 1);
        
        int64_t sz3 = gte->data.sz3;
        if (sz3 == 0) sz3 = 1;
        int64_t factor = (((h * 0x20000) / sz3) + 1) / 2;
        if (factor > 0x1FFFF) factor = 0x1FFFF;
        
        gte->mac[0] = factor * gte->ir[1] + ofx;
        gte->sx2 = (int16_t)gte_saturate(gte->mac[0] >> 16, 12, 1);

        gte->mac[0] = factor * gte->ir[2] + ofy;
        gte->sy2 = (int16_t)gte_saturate(gte->mac[0] >> 16, 12, 1);
    }
}

static void gte_command_nclip(PsxGteState *gte) {
    int32_t sx0 = (int16_t)(gte->data.sxy0 & 0xFFFF);
    int32_t sy0 = (int16_t)((gte->data.sxy0 >> 16) & 0xFFFF);
    int32_t sx1 = (int16_t)(gte->data.sxy1 & 0xFFFF);
    int32_t sy1 = (int16_t)((gte->data.sxy1 >> 16) & 0xFFFF);
    int32_t sx2 = (int16_t)(gte->data.sxy2 & 0xFFFF);
    int32_t sy2 = (int16_t)((gte->data.sxy2 >> 16) & 0xFFFF);
    
    gte->mac[0] = (int64_t)sx0*sy1 + sx1*sy2 + sx2*sy0 - sx0*sy2 - sx1*sy0 - sx2*sy1;
}

static void gte_command_avsz3(PsxGteState *gte) {
    int32_t zsf3 = gte->ctrl.zsf3;
    gte->mac[0] = (int64_t)zsf3 * (gte->data.sz1 + gte->data.sz2 + gte->data.sz3);
    gte->data.otz = (uint16_t)gte_saturate(gte->mac[0] >> 12, 16, 0);
}

static void gte_command_avsz4(PsxGteState *gte) {
    int32_t zsf4 = gte->ctrl.zsf4;
    gte->mac[0] = (int64_t)zsf4 * (gte->data.sz0 + gte->data.sz1 + gte->data.sz2 + gte->data.sz3);
    gte->data.otz = (uint16_t)gte_saturate(gte->mac[0] >> 12, 16, 0);
}

static void gte_command_mvmva(PsxGteState *gte, int sf, int mx, int v, int cv, int lm) {
    int32_t *matrix;
    int32_t *vec;
    int32_t *trans;
    
    switch (mx) {
    case 0: matrix = (int32_t*)&gte->ctrl.rt11; break;
    case 1: matrix = (int32_t*)&gte->ctrl.l11; break;
    case 2: matrix = (int32_t*)&gte->ctrl.lr1; break;
    default: matrix = (int32_t*)&gte->ctrl.rt11; break;
    }
    
    switch (v) {
    case 0: vec = (int32_t*)&gte->data.vxy0; break;
    case 1: vec = (int32_t*)&gte->data.vxy1; break;
    case 2: vec = (int32_t*)&gte->data.vxy2; break;
    case 3: vec = gte->ir; break;
    default: vec = (int32_t*)&gte->data.vxy0; break;
    }
    
    switch (cv) {
    case 0: trans = &gte->ctrl.tr_x; break;
    case 1: trans = &gte->ctrl.rbk; break;
    case 2: trans = &gte->ctrl.rfc; break;
    default: trans = &gte->ctrl.tr_x; break;
    }
    
    int shift = sf ? 12 : 0;
    
    gte->mac[1] = (((int64_t)trans[0] << 12) + matrix[0]*vec[0] + matrix[1]*vec[1] + matrix[2]*vec[2]) >> (12 + shift);
    gte->mac[2] = (((int64_t)trans[1] << 12) + matrix[3]*vec[0] + matrix[4]*vec[1] + matrix[5]*vec[2]) >> (12 + shift);
    gte->mac[3] = (((int64_t)trans[2] << 12) + matrix[6]*vec[0] + matrix[7]*vec[1] + matrix[8]*vec[2]) >> (12 + shift);
    
    gte->ir[1] = gte_saturate(gte->mac[1], 16, lm);
    gte->ir[2] = gte_saturate(gte->mac[2], 16, lm);
    gte->ir[3] = gte_saturate(gte->mac[3], 16, lm);
}

static void gte_command_sqr(PsxGteState *gte, int sf) {
    int shift = sf ? 12 : 0;
    
    gte->mac[1] = (((int64_t)gte->ir[1] * gte->ir[1]) >> shift);
    gte->mac[2] = (((int64_t)gte->ir[2] * gte->ir[2]) >> shift);
    gte->mac[3] = (((int64_t)gte->ir[3] * gte->ir[3]) >> shift);
    
    gte->ir[1] = gte_saturate(gte->mac[1], 16, 1);
    gte->ir[2] = gte_saturate(gte->mac[2], 16, 1);
    gte->ir[3] = gte_saturate(gte->mac[3], 16, 1);
}

static void gte_command_op(PsxGteState *gte, int sf) {
    int shift = sf ? 12 : 0;
    
    int32_t ir1 = gte->ir[1];
    int32_t ir2 = gte->ir[2];
    int32_t ir3 = gte->ir[3];
    
    gte->mac[1] = (((int64_t)ir3 * gte->ctrl.rt22) - (int64_t)ir2 * gte->ctrl.rt23) >> shift;
    gte->mac[2] = (((int64_t)ir1 * gte->ctrl.rt23) - (int64_t)ir3 * gte->ctrl.rt21) >> shift;
    gte->mac[3] = (((int64_t)ir2 * gte->ctrl.rt21) - (int64_t)ir1 * gte->ctrl.rt22) >> shift;
    
    gte->ir[1] = gte_saturate(gte->mac[1], 16, 1);
    gte->ir[2] = gte_saturate(gte->mac[2], 16, 1);
    gte->ir[3] = gte_saturate(gte->mac[3], 16, 1);
}

static void gte_command_ncs(PsxGteState *gte, int sf) {
    int32_t *llm = (int32_t*)&gte->ctrl.l11;
    int32_t vx0 = (int16_t)(gte->data.vxy0 & 0xFFFF);
    int32_t vy0 = (int16_t)((gte->data.vxy0 >> 16) & 0xFFFF);
    int32_t vz0 = (int16_t)(gte->data.vz0 & 0xFFFF);
    
    int shift = sf ? 12 : 0;
    
    gte->mac[1] = ((int64_t)llm[0]*vx0 + llm[1]*vy0 + llm[2]*vz0) >> (12 + shift);
    gte->mac[2] = ((int64_t)llm[3]*vx0 + llm[4]*vy0 + llm[5]*vz0) >> (12 + shift);
    gte->mac[3] = ((int64_t)llm[6]*vx0 + llm[7]*vy0 + llm[8]*vz0) >> (12 + shift);
    
    gte->ir[1] = gte_saturate(gte->mac[1], 16, 1);
    gte->ir[2] = gte_saturate(gte->mac[2], 16, 1);
    gte->ir[3] = gte_saturate(gte->mac[3], 16, 1);
    
    uint8_t r = (gte->data.rgbc >> 0) & 0xFF;
    uint8_t g = (gte->data.rgbc >> 8) & 0xFF;
    uint8_t b = (gte->data.rgbc >> 16) & 0xFF;
    
    gte->color_fifo[0] = (uint8_t)gte_saturate((gte->mac[1] * r) >> 12, 8, 0);
    gte->color_fifo[1] = (uint8_t)gte_saturate((gte->mac[2] * g) >> 12, 8, 0);
    gte->color_fifo[2] = (uint8_t)gte_saturate((gte->mac[3] * b) >> 12, 8, 0);
    gte->color_fifo[3] = gte->data.rgbc & 0xFF;
    
    for (int i = 0; i < 3; i++) {
        gte->color_fifo[i] = gte->color_fifo[i+4];
        gte->color_fifo[i+4] = gte->color_fifo[i+8];
    }
    for (int i = 0; i < 3; i++) {
        gte->color_fifo[i+4] = gte->color_fifo[i+8];
    }
}

static void gte_command_dpcs(PsxGteState *gte, int sf) {
    int32_t fc_r = gte->ctrl.rfc;
    int32_t fc_g = gte->ctrl.gfc;
    int32_t fc_b = gte->ctrl.bfc;
    int32_t ir0 = gte->data.ir0;
    
    int shift = sf ? 12 : 0;
    
    uint8_t r = (gte->data.rgbc >> 0) & 0xFF;
    uint8_t g = (gte->data.rgbc >> 8) & 0xFF;
    uint8_t b = (gte->data.rgbc >> 16) & 0xFF;
    
    gte->mac[1] = (((int64_t)r * gte->ir[1]) + ((fc_r - gte->ir[1]) * ir0)) >> (12 + shift);
    gte->mac[2] = (((int64_t)g * gte->ir[2]) + ((fc_g - gte->ir[2]) * ir0)) >> (12 + shift);
    gte->mac[3] = (((int64_t)b * gte->ir[3]) + ((fc_b - gte->ir[3]) * ir0)) >> (12 + shift);
    
    gte->ir[1] = gte_saturate(gte->mac[1], 16, 1);
    gte->ir[2] = gte_saturate(gte->mac[2], 16, 1);
    gte->ir[3] = gte_saturate(gte->mac[3], 16, 1);
    
    gte->color_fifo[0] = (uint8_t)gte_saturate(gte->mac[1] >> 8, 8, 0);
    gte->color_fifo[1] = (uint8_t)gte_saturate(gte->mac[2] >> 8, 8, 0);
    gte->color_fifo[2] = (uint8_t)gte_saturate(gte->mac[3] >> 8, 8, 0);
    gte->color_fifo[3] = gte->data.rgbc & 0xFF;
}

static void gte_command_intpl(PsxGteState *gte, int sf) {
    int32_t fc_r = gte->ctrl.rfc;
    int32_t fc_g = gte->ctrl.gfc;
    int32_t fc_b = gte->ctrl.bfc;
    int32_t ir0 = gte->data.ir0;
    
    int shift = sf ? 12 : 0;
    
    gte->mac[1] = (((int64_t)gte->ir[1] * ir0) + ((fc_r << 12) - gte->ir[1] * ir0)) >> (12 + shift);
    gte->mac[2] = (((int64_t)gte->ir[2] * ir0) + ((fc_g << 12) - gte->ir[2] * ir0)) >> (12 + shift);
    gte->mac[3] = (((int64_t)gte->ir[3] * ir0) + ((fc_b << 12) - gte->ir[3] * ir0)) >> (12 + shift);
    
    gte->ir[1] = gte_saturate(gte->mac[1], 16, 1);
    gte->ir[2] = gte_saturate(gte->mac[2], 16, 1);
    gte->ir[3] = gte_saturate(gte->mac[3], 16, 1);
    
    gte->color_fifo[0] = (uint8_t)gte_saturate(gte->mac[1] >> 8, 8, 0);
    gte->color_fifo[1] = (uint8_t)gte_saturate(gte->mac[2] >> 8, 8, 0);
    gte->color_fifo[2] = (uint8_t)gte_saturate(gte->mac[3] >> 8, 8, 0);
    gte->color_fifo[3] = gte->data.rgbc & 0xFF;
}

void gte_exec(PsxGteState *gte, uint32_t opcode) {
    int real_cmd = opcode & 0x3F;
    int fake_cmd = (opcode >> 20) & 0x1F;
    int sf = (opcode >> 19) & 1;
    int mx = (opcode >> 17) & 3;
    int v = (opcode >> 15) & 3;
    int cv = (opcode >> 13) & 3;
    int lm = (opcode >> 10) & 1;
    
    gte->flag = 0;
    gte->cmd_pending = 1;
    
    switch (real_cmd) {
    case 0x01:
        gte_command_rtps(gte, sf);
        gte->cycles = 15;
        break;
    case 0x06:
        gte_command_nclip(gte);
        gte->cycles = 8;
        break;
    case 0x0C:
        gte_command_op(gte, sf);
        gte->cycles = 6;
        break;
    case 0x10:
        gte_command_dpcs(gte, sf);
        gte->cycles = 8;
        break;
    case 0x11:
        gte_command_intpl(gte, sf);
        gte->cycles = 8;
        break;
    case 0x12:
        gte_command_mvmva(gte, sf, mx, v, cv, lm);
        gte->cycles = 8;
        break;
    case 0x13:
        break;
    case 0x1E:
        gte_command_ncs(gte, sf);
        gte->cycles = 14;
        break;
    case 0x20:
        gte_command_rtpt(gte, sf);
        gte->cycles = 23;
        break;
    case 0x28:
        gte_command_sqr(gte, sf);
        gte->cycles = 5;
        break;
    case 0x2D:
        gte_command_avsz3(gte);
        gte->cycles = 5;
        break;
    case 0x2E:
        gte_command_avsz4(gte);
        gte->cycles = 6;
        break;
    case 0x3D:
        break;
    case 0x3E:
        break;
    default:
        gte->cycles = 1;
        break;
    }
}

uint32_t gte_read32(PsxGteState *gte, uint32_t reg) {
    switch (reg) {
    case 0: return (uint32_t)gte->data.vxy0;
    case 1: return (uint32_t)gte->data.vz0;
    case 2: return (uint32_t)gte->data.vxy1;
    case 3: return (uint32_t)gte->data.vz1;
    case 4: return (uint32_t)gte->data.vxy2;
    case 5: return (uint32_t)gte->data.vz2;
    case 6: return gte->data.rgbc;
    case 7: return gte->data.otz;
    case 8: return (uint32_t)gte->data.ir0;
    case 9: return (uint32_t)gte->data.ir1;
    case 10: return (uint32_t)gte->data.ir2;
    case 11: return (uint32_t)gte->data.ir3;
    case 12: return (uint32_t)gte->data.sxy0;
    case 13: return (uint32_t)gte->data.sxy1;
    case 14: return (uint32_t)gte->data.sxy2;
    case 15: return (uint32_t)gte->data.sxyp;
    case 16: return gte->data.sz0;
    case 17: return gte->data.sz1;
    case 18: return gte->data.sz2;
    case 19: return gte->data.sz3;
    case 20: return ((gte->data.rgb2 << 16) | (gte->data.rgb1 << 8) | gte->data.rgb0);
    case 24: return (uint32_t)gte->mac[0];
    case 25: return (uint32_t)gte->mac[1];
    case 26: return (uint32_t)gte->mac[2];
    case 27: return (uint32_t)gte->mac[3];
    case 28: return gte->data.irgb;
    case 29: return gte->data.orgb;
    case 30: return (uint32_t)gte->data.lzcs;
    case 31: return (uint32_t)gte->data.lzcr;
    case 63: return (uint32_t)gte->flag;
    default:
        if (reg >= 32 && reg <= 36) return ((uint32_t*)&gte->ctrl.rt11)[reg - 32];
        if (reg >= 37 && reg <= 39) return ((uint32_t*)&gte->ctrl.tr_x)[reg - 37];
        if (reg >= 40 && reg <= 44) return ((uint32_t*)&gte->ctrl.l11)[reg - 40];
        if (reg >= 45 && reg <= 47) return ((uint32_t*)&gte->ctrl.rbk)[reg - 45];
        if (reg >= 48 && reg <= 52) return ((uint32_t*)&gte->ctrl.lr1)[reg - 48];
        if (reg >= 53 && reg <= 55) return ((uint32_t*)&gte->ctrl.rfc)[reg - 53];
        if (reg >= 56 && reg <= 57) return ((uint32_t*)&gte->ctrl.ofx)[reg - 56];
        if (reg == 58) return (uint32_t)gte->ctrl.h;
        if (reg == 59) return (uint32_t)gte->ctrl.dqa;
        if (reg == 60) return (uint32_t)gte->ctrl.dqb;
        if (reg >= 61 && reg <= 62) return ((uint32_t*)&gte->ctrl.zsf3)[reg - 61];
        return 0;
    }
}

void gte_write32(PsxGteState *gte, uint32_t reg, uint32_t value) {
    switch (reg) {
    case 0: gte->data.vxy0 = value; break;
    case 1: gte->data.vz0 = value; break;
    case 2: gte->data.vxy1 = value; break;
    case 3: gte->data.vz1 = value; break;
    case 4: gte->data.vxy2 = value; break;
    case 5: gte->data.vz2 = value; break;
    case 6: gte->data.rgbc = value; break;
    case 8: gte->data.ir0 = value; break;
    case 9: gte->data.ir1 = value; break;
    case 10: gte->data.ir2 = value; break;
    case 11: gte->data.ir3 = value; break;
    default:
        if (reg >= 32 && reg <= 36) ((uint32_t*)&gte->ctrl.rt11)[reg - 32] = value;
        else if (reg >= 37 && reg <= 39) ((uint32_t*)&gte->ctrl.tr_x)[reg - 37] = value;
        else if (reg >= 40 && reg <= 44) ((uint32_t*)&gte->ctrl.l11)[reg - 40] = value;
        else if (reg >= 45 && reg <= 47) ((uint32_t*)&gte->ctrl.rbk)[reg - 45] = value;
        else if (reg >= 48 && reg <= 52) ((uint32_t*)&gte->ctrl.lr1)[reg - 48] = value;
        else if (reg >= 53 && reg <= 55) ((uint32_t*)&gte->ctrl.rfc)[reg - 53] = value;
        else if (reg >= 56 && reg <= 57) ((uint32_t*)&gte->ctrl.ofx)[reg - 56] = value;
        else if (reg == 58) gte->ctrl.h = value;
        else if (reg == 59) gte->ctrl.dqa = value;
        else if (reg == 60) gte->ctrl.dqb = value;
        else if (reg >= 61 && reg <= 62) ((uint32_t*)&gte->ctrl.zsf3)[reg - 61] = value;
        else if (reg == 30) gte->data.lzcs = value;
        break;
    }
}

uint32_t gte_mfc2(PsxGteState *gte, uint32_t reg) {
    return gte_read32(gte, reg);
}

void gte_cfc2(PsxGteState *gte, uint32_t reg, uint32_t *value) {
    *value = gte_read32(gte, reg);
}

void gte_mtc2(PsxGteState *gte, uint32_t reg, uint32_t value) {
    gte_write32(gte, reg, value);
}

void gte_ctc2(PsxGteState *gte, uint32_t reg, uint32_t value) {
    gte_write32(gte, reg, value);
}