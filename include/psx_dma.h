#ifndef PSX_DMA_H
#define PSX_DMA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PSX_DMA_CHANNEL_COUNT 7
#define PSX_DMA_CHANNEL_SIZE  0x10

#define PSX_DMA_MDEC_IN   0
#define PSX_DMA_MDEC_OUT  1
#define PSX_DMA_GPU      2
#define PSX_DMA_CDROM   3
#define PSX_DMA_SPU     4
#define PSX_DMA_PIO     5
#define PSX_DMA_OTC     6

#define PSX_DMA_MODE_START  0x0100
#define PSX_DMA_MODE_END  0x0200
#define PSX_DMA_MODE_TRIG 0x1000
#define PSX_DMA_MODE_LINK 0x4000
#define PSX_DMA_MODE_BUSY 0x100000

#define PSX_DMA_DPCR_ENABLE(ch) (1 << (ch))

typedef struct {
    uint32_t base_addr;
    uint32_t current_addr;
    uint16_t block_size;
    uint16_t block_count;
    uint8_t control;
    bool enabled;
    bool busy;
} PsxDmaChannel;

typedef struct {
    PsxDmaChannel channels[PSX_DMA_CHANNEL_COUNT];
    uint8_t dpcr;
    uint8_t dpcr_ext;
} PsxDmaState;

typedef void (*PsxDmaCallback)(void *user_data, uint32_t addr, uint32_t size);

void dma_init(PsxDmaState *dma);
void dma_reset(PsxDmaState *dma);

uint32_t dma_read32(const PsxDmaState *dma, uint32_t addr);
void dma_write32(PsxDmaState *dma, uint32_t addr, uint32_t value);

void dma_set_gpu_callback(PsxDmaCallback callback, void *user_data);
void dma_set_cdrom_callback(PsxDmaCallback callback, void *user_data);
void dma_set_spu_callback(PsxDmaCallback callback, void *user_data);

void dma_update(PsxDmaState *dma, uint32_t cycles);

#endif