#include "psx_sio.h"
#include <string.h>

void sio_init(PsxSioState *sio) {
    memset(sio, 0, sizeof(*sio));
    sio->status = PSX_SIO_STATUS_TX_READY | PSX_SIO_STATUS_TX_EMPTY;
    sio->mode = 0x40;
    sio->control = 0;
    sio->baud = 0x0C;
    sio->connected = false;
}

void sio_reset(PsxSioState *sio) {
    sio->status = PSX_SIO_STATUS_TX_READY | PSX_SIO_STATUS_TX_EMPTY | PSX_SIO_STATUS_RESET;
    sio->control = 0;
    sio->tx_buffer = 0;
    sio->rx_buffer = 0;
}

uint8_t sio_read8(PsxSioState *sio, uint32_t addr) {
    uint32_t offset = addr - PSX_SIO_BASE_ADDR;
    
    switch (offset) {
    case 0:
        return sio->data;
    case 4:
        return (uint8_t)(sio->status & 0xFF);
    case 8:
        return (uint8_t)(sio->mode & 0xFF);
    case 9:
        return (uint8_t)((sio->mode >> 8) & 0xFF);
    case 10:
        return (uint8_t)(sio->control & 0xFF);
    case 11:
        return (uint8_t)((sio->control >> 8) & 0xFF);
    case 14:
        return (uint8_t)(sio->baud & 0xFF);
    case 15:
        return (uint8_t)((sio->baud >> 8) & 0xFF);
    default:
        return 0xFF;
    }
}

void sio_write8(PsxSioState *sio, uint32_t addr, uint8_t value) {
    uint32_t offset = addr - PSX_SIO_BASE_ADDR;
    
    switch (offset) {
    case 0:
        if (sio->tx_enabled) {
            sio->tx_buffer = value;
            sio->status |= PSX_SIO_STATUS_TX_READY;
        }
        break;
    case 4:
        sio->status = value;
        break;
    case 8:
        sio->mode = (sio->mode & 0xFF00) | value;
        break;
    case 9:
        sio->mode = (sio->mode & 0x00FF) | (value << 8);
        break;
    case 10:
        sio->control = (sio->control & 0xFF00) | value;
        sio->tx_enabled = value & PSX_SIO_CTRL_TX_ENABLE;
        sio->rx_enabled = value & PSX_SIO_CTRL_RX_ENABLE;
        sio->tx_irq_enabled = value & PSX_SIO_CTRL_TX_IRQ;
        sio->rx_irq_enabled = value & PSX_SIO_CTRL_RX_IRQ;
        
        if (value & PSX_SIO_CTRL_RESET) {
            sio_reset(sio);
        }
        break;
    case 11:
        sio->control = (sio->control & 0x00FF) | (value << 8);
        break;
    case 14:
        sio->baud = (sio->baud & 0xFF00) | value;
        break;
    case 15:
        sio->baud = (sio->baud & 0x00FF) | (value << 8);
        break;
    default:
        break;
    }
}

uint32_t sio_read32(PsxSioState *sio, uint32_t addr) {
    uint8_t lo = sio_read8(sio, addr);
    uint8_t hi = sio_read8(sio, addr + 1);
    return (hi << 8) | lo;
}

void sio_write32(PsxSioState *sio, uint32_t addr, uint32_t value) {
    sio_write8(sio, addr, value & 0xFF);
    sio_write8(sio, addr + 1, (value >> 8) & 0xFF);
}

void sio_update(PsxSioState *sio) {
    if (sio->status & PSX_SIO_STATUS_RX_READY) {
        sio->status &= ~PSX_SIO_STATUS_RX_READY;
    }
    
    if (sio->connected && (sio->rx_enabled)) {
        sio->status |= PSX_SIO_STATUS_RX_READY;
    }
    
    if ((sio->status & PSX_SIO_STATUS_TX_READY) == 0 && sio->tx_enabled) {
        sio->status |= PSX_SIO_STATUS_TX_READY | PSX_SIO_STATUS_TX_EMPTY;
    }
}