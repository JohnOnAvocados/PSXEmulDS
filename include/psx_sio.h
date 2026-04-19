#ifndef PSX_SIO_H
#define PSX_SIO_H

#include <stdbool.h>
#include <stdint.h>

#define PSX_SIO_BASE_ADDR 0x1F801050
#define PSX_SIO_SIZE 0x10

#define PSX_SIO_STATUS_TX_READY     (1 << 0)
#define PSX_SIO_STATUS_RX_READY    (1 << 1)
#define PSX_SIO_STATUS_TX_EMPTY     (1 << 2)
#define PSX_SIO_STATUS_ERROR        (1 << 3)
#define PSX_SIO_STATUS_TX_IRQ       (1 << 4)
#define PSX_SIO_STATUS_RX_IRQ       (1 << 5)
#define PSX_SIO_STATUS_CLOCK       (1 << 6)
#define PSX_SIO_STATUS_RESET       (1 << 7)

#define PSX_SIO_MODE_8BIT          0x00
#define PSX_SIO_MODE_7BIT         0x01
#define PSX_SIO_MODE_9BIT          0x02

#define PSX_SIO_CTRL_TX_ENABLE    (1 << 0)
#define PSX_SIO_CTRL_RX_ENABLE    (1 << 1)
#define PSX_SIO_CTRL_TX_IRQ        (1 << 2)
#define PSX_SIO_CTRL_RX_IRQ        (1 << 3)
#define PSX_SIO_CTRL_RESET        (1 << 4)

typedef struct {
    uint8_t data;
    uint32_t status;
    uint16_t mode;
    uint16_t control;
    uint16_t baud;
    uint8_t misc;
    uint8_t tx_buffer;
    uint8_t rx_buffer;
    bool tx_irq_enabled;
    bool rx_irq_enabled;
    bool tx_enabled;
    bool rx_enabled;
    bool connected;
} PsxSioState;

void sio_init(PsxSioState *sio);
void sio_reset(PsxSioState *sio);
uint8_t sio_read8(PsxSioState *sio, uint32_t addr);
void sio_write8(PsxSioState *sio, uint32_t addr, uint8_t value);
uint32_t sio_read32(PsxSioState *sio, uint32_t addr);
void sio_write32(PsxSioState *sio, uint32_t addr, uint32_t value);
void sio_update(PsxSioState *sio);

#endif