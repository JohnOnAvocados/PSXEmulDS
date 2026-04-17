#ifndef PSX_CDROM_H
#define PSX_CDROM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define PSX_CDROM_REG_INDEX        0x00
#define PSX_CDROM_REG_RESPONSE     0x01
#define PSX_CDROM_REG_DATA        0x02
#define PSX_CDROM_REG_STATUS      0x03

#define PSX_CDROM_INDEX_CMD        0x00
#define PSX_CDROM_INDEX_VOLUME     0x01
#define PSX_CDROM_INDEX_VOLUME_L   0x02
#define PSX_CDROM_INDEX_VOLUME_R   0x03
#define PSX_CDROM_INDEX_APIN      0x04
#define PSX_CDROM_INDEX_APOUT      0x05
#define PSX_CDROM_INDEX_C1774      0x06
#define PSX_CDROM_INDEX_C1778      0x07

#define PSX_CDROM_STATUS_BUSY      (1 << 0)
#define PSX_CDROM_STATUS_READ     (1 << 1)
#define PSX_CDROM_STATUS_SEEK      (1 << 2)
#define PSX_CDROM_STATUS_PLAY      (1 << 3)
#define PSX_CDROM_STATUS_ERROR     (1 << 4)
#define PSX_CDROM_STATUS_TRANS     (1 << 5)

#define PSX_CDROM_CMD_GETSTAT      0x01
#define PSX_CDROM_CMD_SETLOC       0x02
#define PSX_CDROM_CMD_PLAY         0x03
#define PSX_CDROM_CMD_FORWARD      0x04
#define PSX_CDROM_CMD_BACKWARD     0x05
#define PSX_CDROM_CMD_READN        0x06
#define PSX_CDROM_CMD_STANDBY      0x07
#define PSX_CDROM_CMD_STOP         0x08
#define PSX_CDROM_CMD_PAUSE        0x09
#define PSX_CDROM_CMD_INIT         0x0A
#define PSX_CDROM_CMD_MUTE         0x0B
#define PSX_CDROM_CMD_DEMUTE       0x0C
#define PSX_CDROM_CMD_GETVOL       0x0D
#define PSX_CDROM_CMD_SETVOL       0x0E
#define PSX_CDROM_CMD_GETTD        0x0F

#define PSX_CDROM_RESPONSE_ACK     0x01
#define PSX_CDROM_RESPONSE_COMPLETE 0x02
#define PSX_CDROM_RESPONSE_DATA     0x03
#define PSX_CDROM_RESPONSE_ERROR    0x04

#define PSX_CDROM_SECTOR_SIZE      2352
#define PSX_CDROM_SYNC_SIZE        12
#define PSX_CDROM_HEADER_SIZE       4
#define PSX_CDROM_DATA_SIZE         2048

typedef struct PsxCdromState {
    uint8_t index;
    uint8_t status;
    uint8_t response_fifo[16];
    uint8_t response_count;
    uint8_t response_pos;
    uint8_t data_fifo[2048];
    uint16_t data_count;
    uint16_t data_pos;
    uint8_t command_fifo[16];
    uint8_t command_count;
    uint8_t command_pos;
    uint8_t volume_l;
    uint8_t volume_r;
    uint8_t volume_l2;
    uint8_t volume_r2;
    uint32_t current_location;
    uint32_t target_location;
    bool playing;
    bool reading;
    bool seeking;
    uint8_t last_response;
    FILE *cd_image;
    char image_path[256];
    uint8_t *cd_buffer;
    size_t cd_buffer_size;
    uint32_t cd_sectors;
    uint8_t track_count;
    uint32_t track_lba[4];
} PsxCdromState;

void cdrom_init(PsxCdromState *cdrom);
void cdrom_reset(PsxCdromState *cdrom);
void cdrom_load_image(PsxCdromState *cdrom, const char *path);
uint8_t cdrom_read8(PsxCdromState *cdrom, uint32_t addr);
void cdrom_write8(PsxCdromState *cdrom, uint32_t addr, uint8_t value);
bool cdrom_read_sector(PsxCdromState *cdrom, uint32_t sector, uint8_t *buffer);
void cdrom_exec_command(PsxCdromState *cdrom, uint8_t cmd);
void cdrom_update(PsxCdromState *cdrom, uint32_t cycles);

#endif
