#include "psx_gpu.h"

#include <stdio.h>
#include <string.h>

#define PSX_GPU_REG_GP0     0x00
#define PSX_GPU_REG_GP1      0x01
#define PSX_GPU_STATUS       0x02
#define PSX_GPU_RESET        0x03

static inline uint16_t gpu_rgb555(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
}

static inline void gpu_rgb555_decode(uint16_t color, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (color & 0x7C00) >> 10;
    *g = (color & 0x03E0) >> 5;
    *b = (color & 0x001F);
    *r = (*r << 3) | (*r >> 2);
    *g = (*g << 3) | (*g >> 2);
    *b = (*b << 3) | (*b >> 2);
}

static inline void gpu_set_pixel(PsxGpuState *gpu, int x, int y, uint16_t color) {
    if (x >= 0 && x < PSX_GPU_VRAM_WIDTH && y >= 0 && y < PSX_GPU_VRAM_HEIGHT) {
        gpu->vram[y * PSX_GPU_VRAM_WIDTH + x] = color;
        gpu->vram_dirty = true;
    }
}

static inline uint16_t gpu_get_pixel(const PsxGpuState *gpu, int x, int y) {
    if (x >= 0 && x < PSX_GPU_VRAM_WIDTH && y >= 0 && y < PSX_GPU_VRAM_HEIGHT) {
        return gpu->vram[y * PSX_GPU_VRAM_WIDTH + x];
    }
    return 0;
}

static void gpu_draw_line(PsxGpuState *gpu, int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = dx > dy ? dx : dy;
    
    if (steps < 0) {
        steps = -steps;
    }
    
    if (steps == 0) {
        gpu_set_pixel(gpu, x0, y0, color);
        return;
    }
    
    float inc_x = (float)dx / (float)steps;
    float inc_y = (float)dy / (float)steps;
    
    float x = x0;
    float y = y0;
    
    for (int i = 0; i <= steps; i++) {
        gpu_set_pixel(gpu, (int)x, (int)y, color);
        x += inc_x;
        y += inc_y;
    }
}

static void gpu_draw_triangle(PsxGpuState *gpu, int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    gpu_draw_line(gpu, x0, y0, x1, y1, color);
    gpu_draw_line(gpu, x1, y1, x2, y2, color);
    gpu_draw_line(gpu, x2, y2, x0, y0, color);
}

static void gpu_fill_rect(PsxGpuState *gpu, int x, int y, int w, int h, uint16_t color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            gpu_set_pixel(gpu, x + i, y + j, color);
        }
    }
}

void gpu_init(PsxGpuState *gpu) {
    memset(gpu, 0, sizeof(*gpu));
    gpu->display_x = 0;
    gpu->display_y = 0;
    gpu->display_w = 320;
    gpu->display_h = 240;
    gpu->displaymode_x = 0x200;
    gpu->displaymode_y = 0x010;
    gpu->display_mode = 0x02;
    gpu->gpu_stat = 0x148C0200;
}

void gpu_reset(PsxGpuState *gpu) {
    memset(gpu->vram, 0, sizeof(gpu->vram));
    gpu->draw_x_start = 0;
    gpu->draw_y_start = 0;
    gpu->draw_x_end = 1023;
    gpu->draw_y_end = 1023;
    gpu->draw_offset_x = 0;
    gpu->draw_offset_y = 0;
    gpu->gp0_param_count = 0;
    gpu->gp0_param_index = 0;
    gpu->gpu_stat = 0x148C0200;
}

uint32_t gpu_read32(const PsxGpuState *gpu, uint32_t addr) {
    uint32_t offset = addr & 0x03;
    
    switch (offset) {
    case PSX_GPU_REG_GP0:
        return gpu->gp0_data;
    case PSX_GPU_REG_GP1:
        return gpu->gp1_data;
    case PSX_GPU_STATUS:
        return gpu->gpu_stat;
    }
    
    return 0;
}

void gpu_exec_gp0(PsxGpuState *gpu, uint8_t cmd) {
    switch (cmd & 0xE0) {
    case 0x00:
        break;
    case 0x20:
    case 0x24:
        if (gpu->gp0_param_count >= 3) {
            int x0 = gpu->gp0_params[0] & 0x3FF;
            int y0 = gpu->gp0_params[1] & 0x1FF;
            int x1 = gpu->gp0_params[2] & 0x3FF;
            int y1 = gpu->gp0_params[3] & 0x1FF;
            int x2 = gpu->gp0_params[4] & 0x3FF;
            int y2 = gpu->gp0_params[5] & 0x1FF;
            
            uint8_t r = gpu->gp0_params[6] & 0xFF;
            uint8_t g = gpu->gp0_params[7] & 0xFF;
            uint8_t b = gpu->gp0_params[8] & 0xFF;
            uint16_t color = gpu_rgb555(r, g, b);
            
            gpu_draw_triangle(gpu, x0, y0, x1, y1, x2, y2, color);
        }
        break;
        
    case 0x28:
    case 0x2C:
        if (gpu->gp0_param_count >= 2) {
            int x0 = gpu->gp0_params[0] & 0x3FF;
            int y0 = gpu->gp0_params[1] & 0x1FF;
            int x1 = gpu->gp0_params[2] & 0x3FF;
            int y1 = gpu->gp0_params[3] & 0x1FF;
            
            uint8_t r = gpu->gp0_params[4] & 0xFF;
            uint8_t g = gpu->gp0_params[5] & 0xFF;
            uint8_t b = gpu->gp0_params[6] & 0xFF;
            uint16_t color = gpu_rgb555(r, g, b);
            
            gpu_draw_line(gpu, x0, y0, x1, y1, color);
        }
        break;
        
    case 0x38:
        if (gpu->gp0_param_count >= 2) {
            int x = gpu->gp0_params[0] & 0x3FF;
            int y = gpu->gp0_params[1] & 0x1FF;
            int w = (gpu->gp0_params[2] & 0xFFFF) + 1;
            int h = ((gpu->gp0_params[2] >> 16) & 0xFFFF) + 1;
            
            uint8_t r = gpu->gp0_params[3] & 0xFF;
            uint8_t g = gpu->gp0_params[4] & 0xFF;
            uint8_t b = gpu->gp0_params[5] & 0xFF;
            uint16_t color = gpu_rgb555(r, g, b);
            
            gpu_fill_rect(gpu, x, y, w, h, color);
        }
        break;
        
    case 0xA0:
        if (gpu->gp0_param_count >= 2) {
            int x = gpu->gp0_params[0] & 0x3FF;
            int y = gpu->gp0_params[1] & 0x1FF;
            int w = (gpu->gp0_params[2] & 0xFFFF);
            int h = (gpu->gp0_params[2] >> 16) & 0xFFFF;
            
        }
        break;
        
    default:
        break;
    }
    
    gpu->gp0_param_count = 0;
    gpu->gp0_param_index = 0;
}

void gpu_exec_gp1(PsxGpuState *gpu, uint8_t cmd) {
    switch (cmd & 0xF8) {
    case 0x00:
        gpu_reset(gpu);
        break;
    case 0x08:
        gpu->display_enable = cmd & 0x01;
        break;
    case 0x10:
        gpu->display_x = gpu->gp0_params[0] & 0xFFF;
        gpu->display_y = gpu->gp0_params[1] & 0x3FF;
        break;
    case 0x18:
        gpu->displaymode_x = gpu->gp0_params[0];
        gpu->displaymode_y = gpu->gp0_params[1];
        break;
    case 0x20:
        gpu->draw_x_start = gpu->gp0_params[0] & 0x3FF;
        gpu->draw_y_start = gpu->gp0_params[1] & 0x3FF;
        break;
    case 0x28:
        gpu->draw_x_end = gpu->gp0_params[0] & 0x3FF;
        gpu->draw_y_end = gpu->gp0_params[1] & 0x3FF;
        break;
    case 0x30:
        gpu->draw_offset_x = (int16_t)(gpu->gp0_params[0] & 0x7FF);
        gpu->draw_offset_y = (int16_t)(gpu->gp0_params[1] & 0x7FF);
        break;
    default:
        break;
    }
}

void gpu_write32(PsxGpuState *gpu, uint32_t addr, uint32_t value) {
    uint32_t offset = addr & 0x03;
    
    switch (offset) {
    case PSX_GPU_REG_GP0:
        gpu->gp0_command = value & 0xFF;
        
        if ((gpu->gp0_command & 0xC0) == 0xC0) {
            gpu->gp0_param_count = 1;
            gpu->gp0_params[0] = value;
        } else if ((gpu->gp0_command & 0xE0) == 0x00) {
            switch (gpu->gp0_command) {
            case 0x01:
                gpu->gp0_param_count = 1;
                break;
            case 0x02:
                gpu->gp0_param_count = 1;
                break;
            default:
                break;
            }
        } else {
            gpu->gp0_param_count = (gpu->gp0_command >> 16) & 0x0F;
            if (gpu->gp0_param_count == 0) {
                gpu->gp0_param_count = 3;
            }
            gpu->gp0_param_index = 0;
        }
        
        gpu_exec_gp0(gpu, gpu->gp0_command);
        break;
        
    case PSX_GPU_REG_GP1:
        gpu->gp1_data = value;
        gpu_exec_gp1(gpu, value & 0xFF);
        break;
    }
}

void gpu_update(PsxGpuState *gpu, uint32_t cycles) {
    gpu->gpu_stat |= PSX_GPU_STATUS_READY_FOR_CMD;
    gpu->gpu_stat |= PSX_GPU_STATUS_READY_FOR_VRAM;
}

void gpu_render(PsxGpuState *gpu, uint16_t *framebuffer, int width, int height) {
    int src_x = gpu->display_x;
    int src_y = gpu->display_y;
    int src_w = gpu->display_w;
    int src_h = gpu->display_h;
    
    if (src_w > PSX_GPU_VRAM_WIDTH) src_w = PSX_GPU_VRAM_WIDTH;
    if (src_h > PSX_GPU_VRAM_HEIGHT) src_h = PSX_GPU_VRAM_HEIGHT;
    
    for (int y = 0; y < height; y++) {
        int src_row = (src_y + y) % PSX_GPU_VRAM_HEIGHT;
        
        for (int x = 0; x < width; x++) {
            int src_col = (src_x + x) % PSX_GPU_VRAM_WIDTH;
            
            framebuffer[y * width + x] = gpu->vram[src_row * PSX_GPU_VRAM_WIDTH + src_col];
        }
    }
}
