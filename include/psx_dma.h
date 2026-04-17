#ifndef PSX_DMA_H
#define PSX_DMA_H

#include <stdbool.h>
#include <stdint.h>

#define PSX_DMA_CHANNEL_COUNT 7

#define PSX_DMA_REG_DPCR    0x00

#define PSX_DMA_CHANNEL_SIZE 0x10

#define PSX_DMA_DPCR_ENABLE(n)   (1 << ((n) * 4))
#define PSX_DMA_DPCR_START(n)    (1 << ((n) * 4 + 1))
#define PSX_DMA_DPCR_DIRECTION(n) (1 << ((n) * 4 + 2))
#define PSX_DMA_DPCR_TRIGGER(n)  (1 << ((n) * 4 + 3))

#define PSX_DMA_MODE_START      (1 << 0)
#define PSX_DMA_MODE_DIRECTION  (1 << 1)
#define PSX_DMA_MODE_ADDRESS_FIX (1 << 2)
#define PSX_DMA_MODE_TRANSFER_MODE (1 << 3)
#define PSX_DMA_MODE_TRIGGER   (1 << 4)

#define PSX_DMA_DEST_GPU     0
#define PSX_DMA_DEST_SPU     1
#define PSX_DMA_DEST_OTC     2
#define PSX_DMA_DEST_MDEC_IN 3
#define PSX_DMA_DEST_MDEC_OUT 4
#define PSX_DMA_DEST_GPU_TEX 5

#define PSX_DMA_SOURCE_MDEC_IN   0
#define PSX_DMA_SOURCE_MDEC_OUT  1
#define PSX_DMA_SOURCE_GPU       2
#define PSX_DMA_SOURCE_CDROM     3
#define PSX_DMA_SOURCE_SPU       4

typedef struct {
    uint32_t base_addr;
    uint32_t current_addr;
    uint16_t block_size;
    uint16_t block_count;
    uint8_t control;
    bool enabled;
    bool busy;
    uint8_t dest;
    uint8_t source;
} PsxDmaChannel;

typedef struct PsxDmaState {
    PsxDmaChannel channels[PSX_DMA_CHANNEL_COUNT];
    uint8_t dpcr;
    uint8_t dpcr_ext;
    uint8_t dpcr_ext2;
} PsxDmaState;

void dma_init(PsxDmaState *dma);
void dma_reset(PsxDmaState *dma);
uint32_t dma_read32(const PsxDmaState *dma, uint32_t addr);
void dma_write32(PsxDmaState *dma, uint32_t addr, uint32_t value);
void dma_update(PsxDmaState *dma, uint32_t cycles);

typedef void (*PsxDmaCallback)(void *user_data, uint32_t addr, uint32_t size);
void dma_set_gpu_callback(PsxDmaCallback callback, void *user_data);
void dma_set_cdrom_callback(PsxDmaCallback callback, void *user_data);
void dma_set_spu_callback(PsxDmaCallback callback, void *user_data);

#endif
