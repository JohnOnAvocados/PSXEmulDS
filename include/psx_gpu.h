#ifndef PSX_GPU_H
#define PSX_GPU_H

#include <stdbool.h>
#include <stdint.h>

#define PSX_GPU_STATUS_READY_FOR_CMD   (1 << 0)
#define PSX_GPU_STATUS_READY_FOR_VRAM (1 << 1)
#define PSX_GPU_STATUS_DMADREQ        (1 << 2)
#define PSX_GPU_STATUS_CMD_WRITE      (1 << 3)
#define PSX_GPU_STATUS_CLEAR_IE       (1 << 4)
#define PSX_GPU_STATUS_FIELD          (1 << 5)
#define PSX_GPU_STATUS_BUSY           (1 << 6)
#define PSX_GPU_STATUS_READY_FOR_CD   (1 << 7)

#define PSX_GPU_VRAM_WIDTH  256
#define PSX_GPU_VRAM_HEIGHT 256

#define PSX_GPU_GP0_NOP              0x00
#define PSX_GPU_GP0_CLEAR_VRAM        0x01
#define PSX_GPU_GP0_FILL_VRAM         0x02
#define PSX_GPU_GP0_COPY_VRAM_CPU     0x40
#define PSX_GPU_GP0_COPY_CPU_VRAM     0xA0
#define PSX_GPU_GP0_COPY_VRAM_VRAM    0x80

#define PSX_GPU_GP0_TRIANGLE_F3       0x20
#define PSX_GPU_GP0_TRIANGLE_F4       0x24
#define PSX_GPU_GP0_TRIANGLE_FT3      0x22
#define PSX_GPU_GP0_TRIANGLE_FT4      0x26
#define PSX_GPU_GP0_LINE_F3           0x28
#define PSX_GPU_GP0_LINE_B3           0x2C
#define PSX_GPU_GP0_LINE_F4           0x30
#define PSX_GPU_GP0_LINE_B4           0x34
#define PSX_GPU_GP0_RECT_F            0x38
#define PSX_GPU_GP0_RECT_S            0x3C
#define PSX_GPU_GP0_RECT_T            0x40
#define PSX_GPU_GP0_RECT_D            0x44

#define PSX_GPU_GP1_RESET_GPU         0x00
#define PSX_GPU_GP1_FIFO_RESET        0x01
#define PSX_GPU_GP1_DISPLAY_ENABLE    0x03
#define PSX_GPU_GP1_DMADIRECTION       0x04
#define PSX_GPU_GP1_DISPLAY_START     0x05
#define PSX_GPU_GP1_HORIZONTAL_RANGE  0x06
#define PSX_GPU_GP1_VERTICAL_RANGE    0x07
#define PSX_GPU_GP1_DISPLAY_MODE      0x08
#define PSX_GPU_GP1_EXTERNAL_RESOLUTION 0x09

#define PSX_GPU_DITHER_RGB888 0
#define PSX_GPU_DITHER_RGB444 1
#define PSX_GPU_DITHER_NONE  2

typedef struct PsxGpuState {
    uint16_t vram[PSX_GPU_VRAM_WIDTH * PSX_GPU_VRAM_HEIGHT];
    uint16_t display_x;
    uint16_t display_y;
    uint16_t display_w;
    uint16_t display_h;
    uint16_t displaymode_x;
    uint16_t displaymode_y;
    uint8_t display_enable;
    uint8_t display_mode;
    uint8_t dither_mode;
    uint16_t draw_x_start;
    uint16_t draw_y_start;
    uint16_t draw_x_end;
    uint16_t draw_y_end;
    uint16_t draw_offset_x;
    uint16_t draw_offset_y;
    uint16_t gpu_stat;
    uint8_t gpu_read;
    uint8_t gp0_command;
    uint8_t gp0_params[32];
    uint8_t gp0_param_count;
    uint8_t gp0_param_index;
    uint32_t gp0_data;
    uint32_t gp1_data;
    bool vram_dirty;
} PsxGpuState;

void gpu_init(PsxGpuState *gpu);
void gpu_reset(PsxGpuState *gpu);
uint32_t gpu_read32(const PsxGpuState *gpu, uint32_t addr);
void gpu_write32(PsxGpuState *gpu, uint32_t addr, uint32_t value);
void gpu_update(PsxGpuState *gpu, uint32_t cycles);
void gpu_exec_gp0(PsxGpuState *gpu, uint8_t cmd);
void gpu_exec_gp1(PsxGpuState *gpu, uint8_t cmd);
void gpu_render(PsxGpuState *gpu, uint16_t *framebuffer, int width, int height);

#endif
