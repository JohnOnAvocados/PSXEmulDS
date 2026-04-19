#include "psx_gpu.h"

#include <stdio.h>
#include <string.h>

static inline uint16_t gpu_rgb555(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
}

static inline void gpu_set_pixel_unsafe(uint16_t *vram, int x, int y, uint16_t color) {
    vram[y * PSX_GPU_VRAM_WIDTH + x] = color;
}

void gpu_init(PsxGpuState *gpu) {
    memset(gpu, 0, sizeof(*gpu));
    gpu->display_x = 0;
    gpu->display_y = 0;
    gpu->display_w = 256;
    gpu->display_h = 240;
    gpu->displaymode_x = 0x200;
    gpu->displaymode_y = 0x010;
    gpu->display_enable = 1;
    gpu->display_mode = 0x02;
    gpu->gpu_stat = PSX_GPU_STATUS_READY_FOR_CMD | PSX_GPU_STATUS_READY_FOR_VRAM;
    gpu->texture_page_x = 0;
    gpu->texture_page_y = 0;
    gpu->texture_base_x = 0;
    gpu->texture_base_y = 0;
    gpu->clut_base_x = 0;
    gpu->clut_base_y = 0;
    gpu->drawing.offset_x = 0;
    gpu->drawing.offset_y = 0;
    gpu->drawing.area_x1 = 0;
    gpu->drawing.area_y1 = 0;
    gpu->drawing.area_x2 = 256;
    gpu->drawing.area_y2 = 256;
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
    gpu->gp0_words_read = 0;
    gpu->gpu_stat = PSX_GPU_STATUS_READY_FOR_CMD | PSX_GPU_STATUS_READY_FOR_VRAM;
    gpu->drawing.clip_x1 = 0;
    gpu->drawing.clip_y1 = 0;
    gpu->drawing.clip_x2 = PSX_GPU_VRAM_WIDTH;
    gpu->drawing.clip_y2 = PSX_GPU_VRAM_HEIGHT;
    gpu->drawing.mask = 0;
    gpu->drawing.keep = 0;
    gpu->cmd_mode = 0;
}

static void gpu_fill_rect_fast(PsxGpuState *gpu, int x, int y, int w, int h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > PSX_GPU_VRAM_WIDTH) w = PSX_GPU_VRAM_WIDTH - x;
    if (y + h > PSX_GPU_VRAM_HEIGHT) h = PSX_GPU_VRAM_HEIGHT - y;
    if (w <= 0 || h <= 0) return;
    
    uint16_t *row = &gpu->vram[y * PSX_GPU_VRAM_WIDTH + x];
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            row[i] = color;
        }
        row += PSX_GPU_VRAM_WIDTH;
    }
    gpu->vram_dirty = true;
}

static void gpu_fill_triangle(PsxGpuState *gpu, int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    if (y0 > y1) { int t; t=x0; x0=x1; x1=t; t=y0; y0=y1; y1=t; }
    if (y0 > y2) { int t; t=x0; x0=x2; x2=t; t=y0; y0=y2; y2=t; }
    if (y1 > y2) { int t; t=x1; x1=x2; x2=t; t=y1; y1=y2; y2=t; }
    
    int total_height = y2 - y0;
    if (total_height == 0) return;
    
    for (int y = y0; y <= y2; y++) {
        float t0 = (y != y0 && y1 != y0) ? (float)(y - y0) / (float)(y1 - y0) : 0.0f;
        float t1 = (y != y2) ? (float)(y - y0) / (float)(y2 - y0) : 0.0f;
        
        int x_start = x0 + (int)((float)(x1 - x0) * t0);
        int x_end = x0 + (int)((float)(x2 - x0) * t1);
        
        if (x_start > x_end) { int t = x_start; x_start = x_end; x_end = t; }
        
        if (y >= 0 && y < PSX_GPU_VRAM_HEIGHT) {
            if (x_start < 0) x_start = 0;
            if (x_end >= PSX_GPU_VRAM_WIDTH) x_end = PSX_GPU_VRAM_WIDTH - 1;
            uint16_t *row = &gpu->vram[y * PSX_GPU_VRAM_WIDTH];
            for (int x = x_start; x <= x_end; x++) {
                row[x] = color;
            }
        }
    }
    gpu->vram_dirty = true;
}

/* Unused function removed */

static void gpu_draw_line(PsxGpuState *gpu, int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = (dx > 0 ? dx : -dx) > (dy > 0 ? dy : -dy) ? (dx > 0 ? dx : -dx) : (dy > 0 ? dy : -dy);
    if (steps == 0) {
        gpu_set_pixel_unsafe(gpu->vram, x0, y0, color);
        return;
    }
    float inc_x = (float)dx / (float)steps;
    float inc_y = (float)dy / (float)steps;
    float x = (float)x0;
    float y = (float)y0;
    for (int i = 0; i <= steps; i++) {
        if ((int)x >= 0 && (int)x < PSX_GPU_VRAM_WIDTH && (int)y >= 0 && (int)y < PSX_GPU_VRAM_HEIGHT) {
            gpu->vram[(int)y * PSX_GPU_VRAM_WIDTH + (int)x] = color;
        }
        x += inc_x;
        y += inc_y;
    }
    gpu->vram_dirty = true;
}

static void gpu_vram_to_cpu(PsxGpuState *gpu, uint32_t count, uint32_t base) {
    uint32_t x = base & 0x3FF;
    uint32_t y = (base >> 10) & 0x1FF;
    uint16_t *data = (uint16_t*)&gpu->gp0_data;
    uint32_t read = 0;
    uint32_t w = 0, h = 0;
    
    if (gpu->gp0_param_count >= 1) {
        w = gpu->gp0_params[0] & 0xFFFF;
        h = (gpu->gp0_params[0] >> 16) & 0xFFFF;
    }
    if (w == 0) w = 256;
    if (h == 0) h = 1;
    
    for (uint32_t row = 0; row < h && read < count; row++) {
        for (uint32_t col = 0; col < w && read < count; col++) {
            if (y + row < PSX_GPU_VRAM_HEIGHT && x + col < PSX_GPU_VRAM_WIDTH) {
                *data++ = gpu->vram[(y + row) * PSX_GPU_VRAM_WIDTH + (x + col)];
                read++;
            }
        }
    }
}

static void gpu_cpu_to_vram(PsxGpuState *gpu, uint32_t count) {
    uint32_t x = gpu->gp0_params[0] & 0x3FF;
    uint32_t y = (gpu->gp0_params[1] >> 10) & 0x1FF;
    uint32_t w = gpu->gp0_params[2] & 0xFFFF;
    uint32_t h = (gpu->gp0_params[2] >> 16) & 0xFFFF;
    if (w == 0) w = 256;
    if (h == 0) h = 1;
    
    uint16_t *data = (uint16_t*)&gpu->gp0_data;
    for (uint32_t row = 0; row < h && gpu->gp0_words_read < count; row++) {
        for (uint32_t col = 0; col < w && gpu->gp0_words_read < count; col++) {
            if (y + row < PSX_GPU_VRAM_HEIGHT && x + col < PSX_GPU_VRAM_WIDTH) {
                gpu->vram[(y + row) * PSX_GPU_VRAM_WIDTH + (x + col)] = *data++;
                gpu->gp0_words_read++;
            }
        }
    }
    gpu->vram_dirty = true;
}

static void gpu_vram_to_vram(PsxGpuState *gpu, uint32_t count) {
    uint32_t src_x = gpu->gp0_params[0] & 0x3FF;
    uint32_t src_y = (gpu->gp0_params[1] >> 10) & 0x1FF;
    uint32_t dst_x = gpu->gp0_params[3] & 0x3FF;
    uint32_t dst_y = (gpu->gp0_params[4] >> 10) & 0x1FF;
    uint32_t w = gpu->gp0_params[2] & 0xFFFF;
    uint32_t h = (gpu->gp0_params[2] >> 16) & 0xFFFF;
    if (w == 0) w = 256;
    if (h == 0) h = 256;
    
    for (uint32_t row = 0; row < h; row++) {
        for (uint32_t col = 0; col < w; col++) {
            uint32_t sx = src_x + col;
            uint32_t sy = src_y + row;
            uint32_t dx = dst_x + col;
            uint32_t dy = dst_y + row;
            if (sy < PSX_GPU_VRAM_HEIGHT && sx < PSX_GPU_VRAM_WIDTH &&
                dy < PSX_GPU_VRAM_HEIGHT && dx < PSX_GPU_VRAM_WIDTH) {
                gpu->vram[dy * PSX_GPU_VRAM_WIDTH + dx] = gpu->vram[sy * PSX_GPU_VRAM_WIDTH + sx];
            }
        }
    }
    gpu->vram_dirty = true;
}

static void gpu_draw_gouraud_triangle(PsxGpuState *gpu, int x0, int y0, int x1, int y1, int x2, int y2,
                                      uint8_t r0, uint8_t g0, uint8_t b0,
                                      uint8_t r1, uint8_t g1, uint8_t b1,
                                      uint8_t r2, uint8_t g2, uint8_t b2) {
    if (y0 > y1) {
        int t; t=x0; x0=x1; x1=t; t=y0; y0=y1; y1=t;
        uint8_t tr=r0; r0=r1; r1=tr; uint8_t tg=g0; g0=g1; g1=tg; uint8_t tb=b0; b0=b1; b1=tb;
    }
    if (y0 > y2) {
        int t; t=x0; x0=x2; x2=t; t=y0; y0=y2; y2=t;
        uint8_t tr=r0; r0=r2; r2=tr; uint8_t tg=g0; g0=g2; g2=tg; uint8_t tb=b0; b0=b2; b2=tb;
    }
    if (y1 > y2) {
        int t; t=x1; x1=x2; x2=t; t=y1; y1=y2; y2=t;
        uint8_t tr=r1; r1=r2; r2=tr; uint8_t tg=g1; g1=g2; g2=tg; uint8_t tb=b1; b1=b2; b2=tb;
    }
    
    int total_height = y2 - y0;
    if (total_height == 0) return;
    
    for (int y = y0; y <= y2; y++) {
        float t0 = (y != y0 && y1 != y0) ? (float)(y - y0) / (float)(y1 - y0) : 0.0f;
        float t1 = (y != y2) ? (float)(y - y0) / (float)(y2 - y0) : 0.0f;
        
        int x_l = x0 + (int)((float)(x1 - x0) * t0);
        int x_r = x0 + (int)((float)(x2 - x0) * t1);
        
        uint8_t r_l = r0 + (int)((float)(r1 - r0) * t0);
        uint8_t g_l = g0 + (int)((float)(g1 - g0) * t0);
        uint8_t b_l = b0 + (int)((float)(b1 - b0) * t0);
        
        uint8_t r_r = r0 + (int)((float)(r2 - r0) * t1);
        uint8_t g_r = g0 + (int)((float)(g2 - g0) * t1);
        uint8_t b_r = b0 + (int)((float)(b2 - b0) * t1);
        
        if (x_l > x_r) { int t = x_l; x_l = x_r; x_r = t; }
        
        if (y >= 0 && y < PSX_GPU_VRAM_HEIGHT) {
            uint16_t *row = &gpu->vram[y * PSX_GPU_VRAM_WIDTH];
            for (int x = x_l; x <= x_r && x >= 0 && x < PSX_GPU_VRAM_WIDTH; x++) {
                float blend = (x_r != x_l) ? (float)(x - x_l) / (float)(x_r - x_l) : 0.0f;
                uint8_t rr = r_l + (int)((float)(r_r - r_l) * blend);
                uint8_t gg = g_l + (int)((float)(g_r - g_l) * blend);
                uint8_t bb = b_l + (int)((float)(b_r - b_l) * blend);
                row[x] = gpu_rgb555(rr, gg, bb);
            }
        }
    }
    gpu->vram_dirty = true;
}

uint32_t gpu_read32(const PsxGpuState *gpu, uint32_t addr) {
    (void)addr;
    return gpu->gpu_stat;
}

void gpu_exec_gp0(PsxGpuState *gpu, uint8_t cmd) {
    uint8_t base = cmd & 0xE0;
    
    switch (base) {
    case 0x00:
        if (cmd == 0x01) {
            gpu_reset(gpu);
        } else if (cmd == 0x02) {
            gpu_fill_rect_fast(gpu, 0, 0, PSX_GPU_VRAM_WIDTH, PSX_GPU_VRAM_HEIGHT, gpu->gp0_params[0] >> 16);
        }
        break;
        
    case 0x20:
    case 0x24:
        if (gpu->gp0_param_count >= 6) {
            int x0 = (gpu->gp0_params[0] & 0x3FF) + gpu->draw_offset_x;
            int y0 = (gpu->gp0_params[1] & 0x1FF) + gpu->draw_offset_y;
            int x1 = (gpu->gp0_params[2] & 0x3FF) + gpu->draw_offset_x;
            int y1 = (gpu->gp0_params[3] & 0x1FF) + gpu->draw_offset_y;
            int x2 = (gpu->gp0_params[4] & 0x3FF) + gpu->draw_offset_x;
            int y2 = (gpu->gp0_params[5] & 0x1FF) + gpu->draw_offset_y;
            
            uint8_t r = gpu->gp0_params[6] & 0xFF;
            uint8_t g = gpu->gp0_params[7] & 0xFF;
            uint8_t b = gpu->gp0_params[8] & 0xFF;
            uint16_t color = gpu_rgb555(r, g, b);
            
            gpu_fill_triangle(gpu, x0, y0, x1, y1, x2, y2, color);
        }
        break;
        
    case 0x28:
        if (gpu->gp0_param_count >= 6) {
            if (base == 0x28) {
                int x0 = (gpu->gp0_params[0] & 0x3FF) + gpu->draw_offset_x;
                int y0 = (gpu->gp0_params[1] & 0x1FF) + gpu->draw_offset_y;
                int x1 = (gpu->gp0_params[2] & 0x3FF) + gpu->draw_offset_x;
                int y1 = (gpu->gp0_params[3] & 0x1FF) + gpu->draw_offset_y;
                uint8_t r = gpu->gp0_params[4] & 0xFF;
                uint8_t g = gpu->gp0_params[5] & 0xFF;
                uint8_t b = gpu->gp0_params[6] & 0xFF;
                uint16_t color = gpu_rgb555(r, g, b);
                gpu_draw_line(gpu, x0, y0, x1, y1, color);
            } else if (base == 0x2C && gpu->gp0_param_count >= 9) {
                gpu_draw_gouraud_triangle(gpu,
                    (gpu->gp0_params[0] & 0x3FF) + gpu->draw_offset_x,
                    (gpu->gp0_params[1] & 0x1FF) + gpu->draw_offset_y,
                    (gpu->gp0_params[2] & 0x3FF) + gpu->draw_offset_x,
                    (gpu->gp0_params[3] & 0x1FF) + gpu->draw_offset_y,
                    (gpu->gp0_params[4] & 0x3FF) + gpu->draw_offset_x,
                    (gpu->gp0_params[5] & 0x1FF) + gpu->draw_offset_y,
                    gpu->gp0_params[6] & 0xFF, gpu->gp0_params[7] & 0xFF, gpu->gp0_params[8] & 0xFF,
                    gpu->gp0_params[9] & 0xFF, gpu->gp0_params[10] & 0xFF, gpu->gp0_params[11] & 0xFF,
                    gpu->gp0_params[12] & 0xFF, gpu->gp0_params[13] & 0xFF, gpu->gp0_params[14] & 0xFF);
            }
        }
        break;
        
    case 0x30:
        if (gpu->gp0_param_count >= 6) {
            int x0 = (gpu->gp0_params[0] & 0x3FF) + gpu->draw_offset_x;
            int y0 = (gpu->gp0_params[1] & 0x1FF) + gpu->draw_offset_y;
            int x1 = (gpu->gp0_params[2] & 0x3FF) + gpu->draw_offset_x;
            int y1 = (gpu->gp0_params[3] & 0x1FF) + gpu->draw_offset_y;
            
            uint8_t r = gpu->gp0_params[4] & 0xFF;
            uint8_t g = gpu->gp0_params[5] & 0xFF;
            uint8_t b = gpu->gp0_params[6] & 0xFF;
            uint16_t color = gpu_rgb555(r, g, b);
            
            gpu_draw_line(gpu, x0, y0, x1, y1, color);
        }
        break;
        
    case 0x38:
        if (gpu->gp0_param_count >= 4) {
            int x = (gpu->gp0_params[0] & 0x3FF) + gpu->draw_offset_x;
            int y = (gpu->gp0_params[1] & 0x1FF) + gpu->draw_offset_y;
            int w = ((gpu->gp0_params[2] & 0xFFFF) ? : 1);
            int h = ((gpu->gp0_params[2] >> 16) & 0xFFFF) ? : 1;
            
            uint8_t r = gpu->gp0_params[3] & 0xFF;
            uint8_t g = gpu->gp0_params[4] & 0xFF;
            uint8_t b = gpu->gp0_params[5] & 0xFF;
            uint16_t color = gpu_rgb555(r, g, b);
            
            gpu_fill_rect_fast(gpu, x, y, w, h, color);
        }
        break;
        
    case 0x3C:
        if (gpu->gp0_param_count >= 4) {
            int x = gpu->gp0_params[0] & 0x3FF;
            int y = gpu->gp0_params[1] & 0x1FF;
            int w = ((gpu->gp0_params[2] >> 0) & 0x3FF);
            int h = ((gpu->gp0_params[2] >> 10) & 0x3FF);
            w = w > 0 ? w : 1;
            h = h > 0 ? h : 1;
            
            uint8_t r = gpu->gp0_params[3] & 0xFF;
            uint8_t g = (gpu->gp0_params[3] >> 8) & 0xFF;
            uint8_t b = gpu->gp0_params[4] & 0xFF;
            uint16_t color = gpu_rgb555(r, g, b);
            
            gpu_fill_rect_fast(gpu, x, y, w, h, color);
        }
        break;
        
    case 0x40:
        gpu_vram_to_cpu(gpu, gpu->gp0_params[0] & 0xFFFF, gpu->gp0_params[1]);
        break;
        
    case 0x80:
        gpu_vram_to_vram(gpu, 0);
        break;
        
    case 0xA0:
        gpu_cpu_to_vram(gpu, 2048);
        break;
        
    case 0xC0:
        if (gpu->gp0_param_count >= 2) {
            uint16_t x = gpu->gp0_params[0] & 0x3FF;
            uint16_t y = gpu->gp0_params[1] & 0x1FF;
            gpu->texture_page_x = x / 64;
            gpu->texture_page_y = y / 64;
            gpu->texture_base_x = x;
            gpu->texture_base_y = y;
        }
        break;
        
    case 0xC2:
        if (gpu->gp0_param_count >= 2) {
            uint16_t x = gpu->gp0_params[0] & 0x3FF;
            uint16_t y = gpu->gp0_params[1] & 0x1FF;
            gpu->clut_base_x = x;
            gpu->clut_base_y = y;
            uint8_t num_colors = (gpu->gp0_params[0] >> 14) & 0xFF;
            if (num_colors == 0) num_colors = 1;
            uint16_t clut_start = y * PSX_GPU_VRAM_WIDTH + x;
            for (uint16_t i = 0; i < num_colors && i < 256; i++) {
                uint16_t src = clut_start + i;
                if (src < PSX_GPU_VRAM_WIDTH * PSX_GPU_VRAM_HEIGHT) {
                    gpu->clut[i] = gpu->vram[src];
                }
            }
        }
        break;
        
    default:
        break;
    }
    
    gpu->gp0_param_count = 0;
    gpu->gp0_words_read = 0;
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
    case 0x38:
        gpu->drawing.area_x1 = gpu->gp0_params[0] & 0x3FF;
        gpu->drawing.area_y1 = gpu->gp0_params[1] & 0x3FF;
        break;
    case 0x40:
        gpu->drawing.area_x2 = gpu->gp0_params[0] & 0x3FF;
        gpu->drawing.area_y2 = gpu->gp0_params[1] & 0x3FF;
        break;
    case 0x48:
        gpu->drawing.offset_x = (int16_t)(gpu->gp0_params[0] & 0x7FF);
        gpu->drawing.offset_y = (int16_t)(gpu->gp0_params[1] & 0x7FF);
        break;
    case 0x50:
        gpu->drawing.mask = gpu->gp0_params[0] & 0x03;
        break;
    case 0x58:
        gpu->drawing.keep = gpu->gp0_params[0] & 0x01;
        break;
    case 0x60:
        gpu->texture_page_x = gpu->gp0_params[0] & 0x0F;
        gpu->texture_page_y = (gpu->gp0_params[0] >> 4) & 0x01;
        gpu->color_depth = (gpu->gp0_params[0] >> 7) & 0x01;
        break;
    default:
        break;
    }
}

void gpu_write32(PsxGpuState *gpu, uint32_t addr, uint32_t value) {
    uint32_t offset = addr & 0x03;
    uint8_t cmd = value & 0xFF;
    
    if (offset == 0) {
        if ((cmd & 0xC0) == 0xC0) {
            gpu->gp0_params[gpu->gp0_param_count++] = value;
            gpu->gp0_words_read++;
            gpu->gp0_data = value;
            return;
        }
        
        gpu->cmd_mode = cmd;
        gpu->gp0_param_count = 0;
        gpu->gp0_words_read = 0;
        
        switch (cmd) {
        case 0x01:
        case 0x02:
            gpu->gp0_params[0] = value;
            gpu->gp0_param_count = 1;
            break;
        case 0x00:
            break;
        default:
            if (gpu->gp0_param_count < 15) {
                gpu->gp0_params[gpu->gp0_param_count++] = value;
            }
            if (gpu->gp0_param_count >= 3 || (cmd & 0xE0) == 0x00) {
                gpu_exec_gp0(gpu, cmd);
            }
            return;
        }
        
        gpu_exec_gp0(gpu, cmd);
    } else if (offset == 1) {
        gpu->gp1_data = value;
        gpu_exec_gp1(gpu, value & 0xFF);
    }
}

void gpu_update(PsxGpuState *gpu, uint32_t cycles) {
    gpu->gpu_stat |= PSX_GPU_STATUS_READY_FOR_CMD;
    gpu->gpu_stat |= PSX_GPU_STATUS_READY_FOR_VRAM;
    (void)cycles;
}

void gpu_render(PsxGpuState *gpu, uint16_t *framebuffer, int width, int height) {
    if (!gpu->vram_dirty && gpu->display_enable == 0) {
        return;
    }
    
    int src_x = gpu->display_x;
    int src_y = gpu->display_y;
    
    for (int y = 0; y < height; y++) {
        int row = (src_y + y) % PSX_GPU_VRAM_HEIGHT;
        for (int x = 0; x < width; x++) {
            int col = (src_x + x) % PSX_GPU_VRAM_WIDTH;
            framebuffer[y * width + x] = gpu->vram[row * PSX_GPU_VRAM_WIDTH + col];
        }
    }
    
    gpu->vram_dirty = false;
}