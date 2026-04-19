#ifndef PSX_MDEC_H
#define PSX_MDEC_H

#include <stdbool.h>
#include <stdint.h>

#define PSX_MDEC_BASE_ADDR 0x1F801800
#define PSX_MDEC_SIZE 0x80

#define PSX_MDEC_STATUS_BUSY     (1 << 0)
#define PSX_MDEC_STATUS_EOF       (1 << 1)
#define PSX_MDEC_STATUS_ERROR    (1 << 2)
#define PSX_MDEC_STATUS_DATA     (1 << 3)
#define PSX_MDEC_STATUS_RESET   (1 << 4)
#define PSX_MDEC_STATUS_READY  (1 << 5)

#define PSX_MDEC_CONTROL_RESET      0x01
#define PSX_MDEC_CONTROL_DECODE    0x02
#define PSX_MDEC_CONTROL_SETSCALE    0x04
#define PSX_MDEC_CONTROL_SETIQTABLE 0x08
#define PSX_MDEC_CONTROL_SETIQSCALE 0x10

#define PSX_MDEC_CMD_DECODE     0x00
#define PSX_MDEC_CMD_SETBLOCK 0x01
#define PSX_MDEC_CMD_SETSCALE 0x02

typedef struct PsxMdecState {
    uint32_t control;
    uint32_t status;
    uint32_t command;
    uint32_t hw_width;
    uint32_t hw_height;
    uint32_t hw_scale;
    uint32_t hw_picture_code;
    uint8_t quantum[64];
    int16_t iquant[64];
    int16_t iquant_scale;
    int16_t dct[64];
    uint8_t output_buffer[1024 * 1024 * 2];
    uint32_t output_ptr;
    uint32_t output_size;
    uint16_t frame_width;
    uint16_t frame_height;
    uint8_t quality;
    bool decoding;
    bool half_frame;
    bool satd;
} PsxMdecState;

void mdec_init(PsxMdecState *mdec);
void mdec_reset(PsxMdecState *mdec);
uint32_t mdec_read32(PsxMdecState *mdec, uint32_t addr);
void mdec_write32(PsxMdecState *mdec, uint32_t addr, uint32_t value);
void mdec_decode_frame(PsxMdecState *mdec, const uint8_t *data, uint32_t size);
uint8_t *mdec_get_output(PsxMdecState *mdec, uint32_t *size);

#endif