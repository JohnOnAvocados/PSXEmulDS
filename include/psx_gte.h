#ifndef PSX_GTE_H
#define PSX_GTE_H

#include <stdbool.h>
#include <stdint.h>

#define PSX_GTE_BASE_ADDR 0x1F801820

typedef struct {
    int32_t vxy0, vz0;
    int32_t vxy1, vz1;
    int32_t vxy2, vz2;
    uint32_t rgbc;
    uint16_t otz;
    int32_t ir0;
    int32_t ir1, ir2, ir3;
    uint16_t sxy0, sxy1, sxy2, sxyp;
    uint16_t sz0, sz1, sz2, sz3;
    uint8_t rgb0, rgb1, rgb2;
    uint8_t res1;
    int32_t mac0;
    int32_t mac1, mac2, mac3;
    uint32_t irgb;
    uint32_t orgb;
    int32_t lzcs, lzcr;
    uint8_t res2[36];
    int32_t flag;
} PsxGteDataRegs;

typedef struct {
    int32_t rt11, rt12, rt13;
    int32_t rt21, rt22, rt23;
    int32_t rt31, rt32, rt33;
    int32_t tr_x, tr_y, tr_z;
    int32_t l11, l12, l13;
    int32_t l21, l22, l23;
    int32_t l31, l32, l33;
    int32_t rbk, gbk, bbk;
    int32_t lr1, lr2, lr3;
    int32_t lg1, lg2, lg3;
    int32_t lb1, lb2, lb3;
    int32_t rfc, gfc, bfc;
    int32_t ofx, ofy;
    int32_t h;
    int32_t dqa;
    int32_t dqb;
    int32_t zsf3, zsf4;
    uint8_t res3[16];
} PsxGteCtrlRegs;

typedef struct {
    PsxGteDataRegs data;
    PsxGteCtrlRegs ctrl;
    int32_t mac[4];
    int32_t ir[4];
    int16_t screen_xy[12];
    uint16_t screen_z[4];
    uint8_t color_fifo[12];
    uint8_t cmd_pending;
    uint32_t cycles;
    uint32_t flag;
    int16_t sx2, sy2;
} PsxGteState;

void gte_init(PsxGteState *gte);
void gte_reset(PsxGteState *gte);
void gte_exec(PsxGteState *gte, uint32_t opcode);
uint32_t gte_read32(PsxGteState *gte, uint32_t reg);
void gte_write32(PsxGteState *gte, uint32_t reg, uint32_t value);
uint32_t gte_mfc2(PsxGteState *gte, uint32_t reg);
void gte_cfc2(PsxGteState *gte, uint32_t reg, uint32_t *value);
void gte_mtc2(PsxGteState *gte, uint32_t reg, uint32_t value);
void gte_ctc2(PsxGteState *gte, uint32_t reg, uint32_t value);

#endif