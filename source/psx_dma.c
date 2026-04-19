#include "psx.h"
#include "psx_dma.h"

#include <stdio.h>
#include <string.h>

#define PSX_DMA_BASE_ADDR 0x1F801080

static PsxDmaCallback g_gpu_callback = NULL;
static void *g_gpu_user_data = NULL;
static PsxDmaCallback g_cdrom_callback = NULL;
static void *g_cdrom_user_data = NULL;
static PsxDmaCallback g_spu_callback = NULL;
static void *g_spu_user_data = NULL;

void dma_set_gpu_callback(PsxDmaCallback callback, void *user_data) {
    g_gpu_callback = callback;
    g_gpu_user_data = user_data;
}

void dma_set_cdrom_callback(PsxDmaCallback callback, void *user_data) {
    g_cdrom_callback = callback;
    g_cdrom_user_data = user_data;
}

void dma_set_spu_callback(PsxDmaCallback callback, void *user_data) {
    g_spu_callback = callback;
    g_spu_user_data = user_data;
}

void dma_init(PsxDmaState *dma) {
    memset(dma, 0, sizeof(*dma));
    dma->dpcr = 0;
}

void dma_reset(PsxDmaState *dma) {
    for (int i = 0; i < PSX_DMA_CHANNEL_COUNT; i++) {
        dma->channels[i].base_addr = 0;
        dma->channels[i].current_addr = 0;
        dma->channels[i].block_size = 0;
        dma->channels[i].block_count = 0;
        dma->channels[i].control = 0;
        dma->channels[i].enabled = false;
        dma->channels[i].busy = false;
    }
    dma->dpcr = 0;
}

uint32_t dma_read32(const PsxDmaState *dma, uint32_t addr) {
    uint32_t offset = addr - PSX_DMA_BASE_ADDR;
    
    if (offset >= 0x00 && offset < 0x10) {
        uint8_t channel = offset / PSX_DMA_CHANNEL_SIZE;
        uint8_t reg = offset % PSX_DMA_CHANNEL_SIZE;
        
        if (channel >= PSX_DMA_CHANNEL_COUNT) {
            return 0;
        }
        
        const PsxDmaChannel *ch = &dma->channels[channel];
        
        switch (reg) {
        case 0x00:
        case 0x04:
        case 0x08:
        case 0x0C:
            return ch->base_addr;
        default:
            return 0;
        }
    }
    
    if (offset >= 0x70 && offset < 0x74) {
        uint8_t reg = offset - 0x70;
        switch (reg) {
        case 0x00:
            return dma->dpcr;
        case 0x04:
            return dma->dpcr_ext;
        default:
            return 0;
        }
    }
    
    return 0;
}

void dma_write32(PsxDmaState *dma, uint32_t addr, uint32_t value) {
    uint32_t offset = addr - PSX_DMA_BASE_ADDR;
    
    if (offset >= 0x00 && offset < 0x10 * PSX_DMA_CHANNEL_COUNT) {
        uint8_t channel = offset / PSX_DMA_CHANNEL_SIZE;
        uint8_t reg = offset % PSX_DMA_CHANNEL_SIZE;
        
        if (channel >= PSX_DMA_CHANNEL_COUNT) {
            return;
        }
        
        PsxDmaChannel *ch = &dma->channels[channel];
        
        switch (reg) {
        case 0x00:
            ch->base_addr = value;
            break;
        case 0x04:
            ch->current_addr = value;
            break;
        case 0x08:
            ch->block_size = value & 0xFFFF;
            break;
        case 0x0C:
            ch->block_count = value & 0xFFFF;
            ch->control = (value >> 16) & 0xFF;
            
            if ((ch->control & PSX_DMA_MODE_START) && (dma->dpcr & PSX_DMA_DPCR_ENABLE(channel))) {
                ch->enabled = true;
                ch->busy = true;
                ch->current_addr = ch->base_addr;
                
                uint32_t total_size = (uint32_t)ch->block_size * (uint32_t)ch->block_count * 4;
                
                switch (channel) {
                case 0:
                    if (g_gpu_callback) {
                        g_gpu_callback(g_gpu_user_data, ch->base_addr, total_size);
                    }
                    break;
                case 1:
                    if (g_cdrom_callback) {
                        g_cdrom_callback(g_cdrom_user_data, ch->base_addr, total_size);
                    }
                    break;
                case 2:
                    if (g_spu_callback) {
                        g_spu_callback(g_spu_user_data, ch->base_addr, total_size);
                    }
                    break;
                case 3:
                    break;
                }
                
                ch->busy = false;
                ch->enabled = false;
            }
            break;
        }
        
        return;
    }
    
    if (offset >= 0x70 && offset < 0x74) {
        uint8_t reg = offset - 0x70;
        switch (reg) {
        case 0x00:
            dma->dpcr = value & 0xFF;
            break;
        case 0x04:
            dma->dpcr_ext = value & 0xFF;
            break;
        default:
            break;
        }
        return;
    }
}

void dma_update(PsxDmaState *dma, uint32_t cycles) {
    (void)cycles;
    for (int i = 0; i < PSX_DMA_CHANNEL_COUNT; i++) {
        if (dma->channels[i].busy) {
            dma->channels[i].busy = false;
            dma->channels[i].enabled = false;
        }
    }
}