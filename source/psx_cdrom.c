#include "psx_cdrom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t bcd_to_dec(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static uint8_t dec_to_bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

void cdrom_init(PsxCdromState *cdrom) {
    memset(cdrom, 0, sizeof(*cdrom));
    cdrom->status = PSX_CDROM_STATUS_BUSY;
    cdrom->volume_l = 0x80;
    cdrom->volume_r = 0x80;
    cdrom->volume_l2 = 0x80;
    cdrom->volume_r2 = 0x80;
}

void cdrom_reset(PsxCdromState *cdrom) {
    cdrom->index = 0;
    cdrom->status = PSX_CDROM_STATUS_BUSY;
    cdrom->response_count = 0;
    cdrom->response_pos = 0;
    cdrom->data_count = 0;
    cdrom->data_pos = 0;
    cdrom->command_count = 0;
    cdrom->command_pos = 0;
    cdrom->playing = false;
    cdrom->reading = false;
    cdrom->seeking = false;
}

void cdrom_load_image(PsxCdromState *cdrom, const char *path) {
    if (cdrom->cd_image) {
        fclose(cdrom->cd_image);
        cdrom->cd_image = NULL;
    }
    
    if (cdrom->cd_buffer) {
        free(cdrom->cd_buffer);
        cdrom->cd_buffer = NULL;
    }
    
    if (path == NULL) {
        strncpy(cdrom->image_path, "", sizeof(cdrom->image_path) - 1);
        cdrom->cd_sectors = 0;
        return;
    }
    
    strncpy(cdrom->image_path, path, sizeof(cdrom->image_path) - 1);
    cdrom->image_path[sizeof(cdrom->image_path) - 1] = '\0';
    
    cdrom->cd_image = fopen(path, "rb");
    if (cdrom->cd_image) {
        fseek(cdrom->cd_image, 0, SEEK_END);
        long size = ftell(cdrom->cd_image);
        fseek(cdrom->cd_image, 0, SEEK_SET);
        
        cdrom->cd_sectors = size / PSX_CDROM_SECTOR_SIZE;
        
        cdrom->cd_buffer = (uint8_t*)malloc(PSX_CDROM_SECTOR_SIZE);
        if (cdrom->cd_buffer == NULL) {
            fclose(cdrom->cd_image);
            cdrom->cd_image = NULL;
            cdrom->cd_sectors = 0;
        }
    }
}

bool cdrom_read_sector(PsxCdromState *cdrom, uint32_t sector, uint8_t *buffer) {
    if (cdrom->cd_image == NULL || buffer == NULL) {
        return false;
    }
    
    if (sector >= cdrom->cd_sectors) {
        return false;
    }
    
    fseek(cdrom->cd_image, sector * PSX_CDROM_SECTOR_SIZE, SEEK_SET);
    
    if (fread(buffer, 1, PSX_CDROM_SECTOR_SIZE, cdrom->cd_image) != PSX_CDROM_SECTOR_SIZE) {
        return false;
    }
    
    return true;
}

void cdrom_push_response(PsxCdromState *cdrom, uint8_t value) {
    if (cdrom->response_count < 16) {
        cdrom->response_fifo[cdrom->response_count++] = value;
    }
}

void cdrom_exec_command(PsxCdromState *cdrom, uint8_t cmd) {
    cdrom->status |= PSX_CDROM_STATUS_BUSY;
    cdrom->response_count = 0;
    cdrom->data_count = 0;
    
    switch (cmd) {
    case PSX_CDROM_CMD_GETSTAT:
        cdrom_push_response(cdrom, 0x02);
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status |= PSX_CDROM_STATUS_TRANS;
        break;
        
    case PSX_CDROM_CMD_SETLOC:
        if (cdrom->command_count >= 3) {
            uint8_t min = cdrom->command_fifo[0];
            uint8_t sec = cdrom->command_fifo[1];
            uint8_t frame = cdrom->command_fifo[2];
            
            cdrom->target_location = bcd_to_dec(min) * 60 * 75 +
                                    bcd_to_dec(sec) * 75 +
                                    bcd_to_dec(frame);
            cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_ACK);
        }
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status |= PSX_CDROM_STATUS_TRANS;
        break;
        
    case PSX_CDROM_CMD_PLAY:
        cdrom->playing = true;
        cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_ACK);
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status |= PSX_CDROM_STATUS_TRANS;
        break;
        
    case PSX_CDROM_CMD_READN:
        cdrom->reading = true;
        cdrom->seeking = true;
        cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_ACK);
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status |= PSX_CDROM_STATUS_READ | PSX_CDROM_STATUS_SEEK;
        cdrom->status &= ~PSX_CDROM_STATUS_TRANS;
        break;
        
    case PSX_CDROM_CMD_STOP:
        cdrom->reading = false;
        cdrom->playing = false;
        cdrom->seeking = false;
        cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_ACK);
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status |= PSX_CDROM_STATUS_TRANS;
        break;
        
    case PSX_CDROM_CMD_PAUSE:
        cdrom->reading = false;
        cdrom->playing = false;
        cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_ACK);
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status &= ~PSX_CDROM_STATUS_READ;
        cdrom->status |= PSX_CDROM_STATUS_TRANS;
        break;
        
    case PSX_CDROM_CMD_INIT:
        cdrom->reading = false;
        cdrom->playing = false;
        cdrom->current_location = 0;
        cdrom->target_location = 0;
        cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_ACK);
        cdrom->status = PSX_CDROM_STATUS_BUSY | PSX_CDROM_STATUS_TRANS;
        break;
        
    case PSX_CDROM_CMD_MUTE:
        cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_ACK);
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status |= PSX_CDROM_STATUS_TRANS;
        break;
        
    case PSX_CDROM_CMD_DEMUTE:
        cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_ACK);
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status |= PSX_CDROM_STATUS_TRANS;
        break;
        
    case PSX_CDROM_CMD_GETVOL:
        cdrom_push_response(cdrom, cdrom->volume_l);
        cdrom_push_response(cdrom, cdrom->volume_r);
        cdrom_push_response(cdrom, cdrom->volume_l2);
        cdrom_push_response(cdrom, cdrom->volume_r2);
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status |= PSX_CDROM_STATUS_TRANS;
        break;
        
    case PSX_CDROM_CMD_SETVOL:
        if (cdrom->command_count >= 4) {
            cdrom->volume_l = cdrom->command_fifo[0];
            cdrom->volume_r = cdrom->command_fifo[1];
            cdrom->volume_l2 = cdrom->command_fifo[2];
            cdrom->volume_r2 = cdrom->command_fifo[3];
        }
        cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_ACK);
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status |= PSX_CDROM_STATUS_TRANS;
        break;
        
    case PSX_CDROM_CMD_GETTD:
        if (cdrom->command_count >= 1) {
            uint8_t track = cdrom->command_fifo[0];
            if (track == 0) {
                cdrom_push_response(cdrom, dec_to_bcd((cdrom->cd_sectors / 75 / 60) & 0xFF));
                cdrom_push_response(cdrom, dec_to_bcd((cdrom->cd_sectors / 75) % 60));
                cdrom_push_response(cdrom, dec_to_bcd(cdrom->cd_sectors % 75));
            } else {
                cdrom_push_response(cdrom, 0x00);
                cdrom_push_response(cdrom, 0x00);
                cdrom_push_response(cdrom, 0x00);
            }
        }
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status |= PSX_CDROM_STATUS_TRANS;
        break;
        
    default:
        cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_ACK);
        cdrom->status &= ~PSX_CDROM_STATUS_BUSY;
        cdrom->status |= PSX_CDROM_STATUS_TRANS;
        break;
    }
    
    cdrom->command_count = 0;
    cdrom->command_pos = 0;
}

uint8_t cdrom_read8(PsxCdromState *cdrom, uint32_t addr) {
    uint32_t offset = addr & 0x03;
    
    switch (offset) {
    case PSX_CDROM_REG_INDEX:
        return cdrom->index;
        
    case PSX_CDROM_REG_RESPONSE:
        if (cdrom->response_pos < cdrom->response_count) {
            return cdrom->response_fifo[cdrom->response_pos++];
        }
        return 0;
        
    case PSX_CDROM_REG_DATA:
        if (cdrom->data_pos < cdrom->data_count) {
            return cdrom->data_fifo[cdrom->data_pos++];
        }
        return 0;
        
    case PSX_CDROM_REG_STATUS:
    {
        uint8_t status = cdrom->status;
        if (cdrom->response_count > 0) {
            status |= 0x40;
        }
        if (cdrom->data_count > 0) {
            status |= 0x80;
        }
        return status;
    }
    }
    
    return 0;
}

void cdrom_write8(PsxCdromState *cdrom, uint32_t addr, uint8_t value) {
    uint32_t offset = addr & 0x03;
    
    switch (offset) {
    case PSX_CDROM_REG_INDEX:
        cdrom->index = value & 0x03;
        break;
        
    case 0x02:
        if (cdrom->index == 0) {
            if ((value & 0x80) == 0) {
                if (cdrom->command_count < 16) {
                    cdrom->command_fifo[cdrom->command_count++] = value;
                }
            } else {
                cdrom_exec_command(cdrom, value & 0x7F);
            }
        }
        break;
        
    case 0x03:
        if (cdrom->index == PSX_CDROM_INDEX_VOLUME_L) {
            cdrom->volume_l = value;
        } else if (cdrom->index == PSX_CDROM_INDEX_VOLUME_R) {
            cdrom->volume_r = value;
        }
        break;
    }
}

void cdrom_update(PsxCdromState *cdrom, uint32_t cycles) {
    if (cdrom->reading && !cdrom->seeking) {
        if (cdrom->data_count == 0 && cdrom->cd_buffer) {
            if (cdrom_read_sector(cdrom, cdrom->current_location, cdrom->cd_buffer)) {
                memcpy(cdrom->data_fifo, cdrom->cd_buffer + PSX_CDROM_SYNC_SIZE + PSX_CDROM_HEADER_SIZE, 
                       PSX_CDROM_DATA_SIZE);
                cdrom->data_count = PSX_CDROM_DATA_SIZE;
                cdrom->data_pos = 0;
                
                cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_DATA);
                
                cdrom->current_location++;
            }
        }
    }
    
    if (cdrom->seeking && cdrom->current_location != cdrom->target_location) {
        if (cdrom->current_location < cdrom->target_location) {
            cdrom->current_location++;
        } else {
            cdrom->current_location--;
        }
        
        if (cdrom->current_location == cdrom->target_location) {
            cdrom->seeking = false;
            cdrom_push_response(cdrom, PSX_CDROM_RESPONSE_COMPLETE);
        }
    }
}
