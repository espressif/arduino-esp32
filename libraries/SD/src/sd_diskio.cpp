// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "sd_diskio.h"
extern "C" {
    #include "diskio.h"
    #include "ffconf.h"
    #include "ff.h"
    //#include "esp_vfs.h"
    #include "esp_vfs_fat.h"
    char CRC7(const char* data, int length);
    unsigned short CRC16(const char* data, int length);
}

typedef enum {
    GO_IDLE_STATE           = 0,
    SEND_OP_COND            = 1,
    SEND_CID                = 2,
    SEND_RELATIVE_ADDR      = 3,
    SEND_SWITCH_FUNC        = 6,
    SEND_IF_COND            = 8,
    SEND_CSD                = 9,
    STOP_TRANSMISSION       = 12,
    SEND_STATUS             = 13,
    SET_BLOCKLEN            = 16,
    READ_BLOCK_SINGLE       = 17,
    READ_BLOCK_MULTIPLE     = 18,
    SEND_NUM_WR_BLOCKS      = 22,
    SET_WR_BLK_ERASE_COUNT  = 23,
    WRITE_BLOCK_SINGLE      = 24,
    WRITE_BLOCK_MULTIPLE    = 25,
    APP_OP_COND             = 41,
    APP_CLR_CARD_DETECT     = 42,
    APP_CMD                 = 55,
    READ_OCR                = 58,
    CRC_ON_OFF              = 59
} ardu_sdcard_command_t;

typedef struct {
    uint8_t ssPin;
    SPIClass * spi;
    int frequency;
    char * base_path;
    sdcard_type_t type;
    unsigned long sectors;
    bool supports_crc;
    int status;
} ardu_sdcard_t;

static ardu_sdcard_t* s_cards[FF_VOLUMES] = { NULL };

/*
 * SD SPI
 * */

bool sdWait(uint8_t pdrv, int timeout)
{
    char resp;
    uint32_t start = millis();

    do {
        resp = s_cards[pdrv]->spi->transfer(0xFF);
    } while (resp == 0x00 && (millis() - start) < (unsigned int)timeout);

    return (resp > 0x00);
}

void sdStop(uint8_t pdrv)
{
    s_cards[pdrv]->spi->write(0xFD);
}

void sdDeselectCard(uint8_t pdrv)
{
    ardu_sdcard_t * card = s_cards[pdrv];
    digitalWrite(card->ssPin, HIGH);
}

bool sdSelectCard(uint8_t pdrv)
{
    ardu_sdcard_t * card = s_cards[pdrv];
    digitalWrite(card->ssPin, LOW);
    sdWait(pdrv, 300);
    return true;
}

char sdCommand(uint8_t pdrv, char cmd, unsigned int arg, unsigned int* resp)
{
    char token;
    ardu_sdcard_t * card = s_cards[pdrv];

    for (int f = 0; f < 3; f++) {
        if (cmd == SEND_NUM_WR_BLOCKS || cmd == SET_WR_BLK_ERASE_COUNT || cmd == APP_OP_COND || cmd == APP_CLR_CARD_DETECT) {
            token = sdCommand(pdrv, APP_CMD, 0, NULL);
            sdDeselectCard(pdrv);
            if (token > 1) {
                return token;
            }
            if(!sdSelectCard(pdrv)) {
                return 0xFF;
            }
        }

        char cmdPacket[7];
        cmdPacket[0] = cmd | 0x40;
        cmdPacket[1] = arg >> 24;
        cmdPacket[2] = arg >> 16;
        cmdPacket[3] = arg >> 8;
        cmdPacket[4] = arg;
        if(card->supports_crc || cmd == GO_IDLE_STATE || cmd == SEND_IF_COND) {
            cmdPacket[5] = (CRC7(cmdPacket, 5) << 1) | 0x01;
        } else {
            cmdPacket[5] = 0x01;
        }
        cmdPacket[6] = 0xFF;

        card->spi->writeBytes((uint8_t*)cmdPacket, (cmd == STOP_TRANSMISSION)?7:6);

        for (int i = 0; i < 9; i++) {
            token = card->spi->transfer(0xFF);
            if (!(token & 0x80)) {
                break;
            }
        }

        if (token == 0xFF) {
            log_w("no token received");
            sdDeselectCard(pdrv);
            delay(100);
            sdSelectCard(pdrv);
            continue;
        } else if (token & 0x08) {
            log_w("crc error");
            sdDeselectCard(pdrv);
            delay(100);
            sdSelectCard(pdrv);
            continue;
        } else if (token > 1) {
            log_w("token error [%u] 0x%x", cmd, token);
            break;
        }

        if (cmd == SEND_STATUS && resp) {
            *resp = card->spi->transfer(0xFF);
        } else if ((cmd == SEND_IF_COND || cmd == READ_OCR) && resp) {
            *resp = card->spi->transfer32(0xFFFFFFFF);
        }

        break;
    }

    return token;
}

bool sdReadBytes(uint8_t pdrv, char* buffer, int length)
{
    char token;
    unsigned short crc;
    ardu_sdcard_t * card = s_cards[pdrv];

    uint32_t start = millis();
    do {
        token = card->spi->transfer(0xFF);
    } while (token == 0xFF && (millis() - start) < 500);

    if (token != 0xFE) {
        return false;
    }

    card->spi->transferBytes(NULL, (uint8_t*)buffer, length);
    crc = card->spi->transfer16(0xFFFF);
    return (!card->supports_crc || crc == CRC16(buffer, length));
}

char sdWriteBytes(uint8_t pdrv, const char* buffer, char token)
{
    ardu_sdcard_t * card = s_cards[pdrv];
    unsigned short crc = (card->supports_crc)?CRC16(buffer, 512):0xFFFF;
    if (!sdWait(pdrv, 500)) {
        return false;
    }

    card->spi->write(token);
    card->spi->writeBytes((uint8_t*)buffer, 512);
    card->spi->write16(crc);
    return (card->spi->transfer(0xFF) & 0x1F);
}

/*
 * SPI SDCARD Communication
 * */

char sdTransaction(uint8_t pdrv, char cmd, unsigned int arg, unsigned int* resp)
{
    if(!sdSelectCard(pdrv)) {
        return 0xFF;
    }
    char token = sdCommand(pdrv, cmd, arg, resp);
    sdDeselectCard(pdrv);
    return token;
}

bool sdReadSector(uint8_t pdrv, char* buffer, unsigned long long sector)
{
    for (int f = 0; f < 3; f++) {
        if(!sdSelectCard(pdrv)) {
            break;
        }
        if (!sdCommand(pdrv, READ_BLOCK_SINGLE, (s_cards[pdrv]->type == CARD_SDHC) ? sector : sector << 9, NULL)) {
            bool success = sdReadBytes(pdrv, buffer, 512);
            sdDeselectCard(pdrv);
            if (success) {
                return true;
            }
        } else {
            break;
        }
    }
    sdDeselectCard(pdrv);
    return false;
}

bool sdReadSectors(uint8_t pdrv, char* buffer, unsigned long long sector, int count)
{
    for (int f = 0; f < 3;) {
        if(!sdSelectCard(pdrv)) {
            break;
        }

        if (!sdCommand(pdrv, READ_BLOCK_MULTIPLE, (s_cards[pdrv]->type == CARD_SDHC) ? sector : sector << 9, NULL)) {
            do {
                if (!sdReadBytes(pdrv, buffer, 512)) {
                    f++;
                    break;
                }

                sector++;
                buffer += 512;
                f = 0;
            } while (--count);

            if (sdCommand(pdrv, STOP_TRANSMISSION, 0, NULL)) {
                log_e("command failed");
                break;
            }

            sdDeselectCard(pdrv);
            if (count == 0) {
                return true;
            }
        } else {
            break;
        }
    }
    sdDeselectCard(pdrv);
    return false;
}

bool sdWriteSector(uint8_t pdrv, const char* buffer, unsigned long long sector)
{
    for (int f = 0; f < 3; f++) {
        if(!sdSelectCard(pdrv)) {
            break;
        }
        if (!sdCommand(pdrv, WRITE_BLOCK_SINGLE, (s_cards[pdrv]->type == CARD_SDHC) ? sector : sector << 9, NULL)) {
            char token = sdWriteBytes(pdrv, buffer, 0xFE);
            sdDeselectCard(pdrv);

            if (token == 0x0A) {
                continue;
            } else if (token == 0x0C) {
                return false;
            }

            unsigned int resp;
            if (sdTransaction(pdrv, SEND_STATUS, 0, &resp) || resp) {
                return false;
            }
            return true;
        } else {
            break;
        }
    }
    sdDeselectCard(pdrv);
    return false;
}

bool sdWriteSectors(uint8_t pdrv, const char* buffer, unsigned long long sector, int count)
{
    char token;
    const char* currentBuffer = buffer;
    unsigned long long currentSector = sector;
    int currentCount = count;
    ardu_sdcard_t * card = s_cards[pdrv];

    for (int f = 0; f < 3;) {
        if (card->type != CARD_MMC) {
            if (sdTransaction(pdrv, SET_WR_BLK_ERASE_COUNT, currentCount, NULL)) {
                break;
            }
        }

        if(!sdSelectCard(pdrv)) {
            break;
        }

        if (!sdCommand(pdrv, WRITE_BLOCK_MULTIPLE, (card->type == CARD_SDHC) ? currentSector : currentSector << 9, NULL)) {
            do {
                token = sdWriteBytes(pdrv, currentBuffer, 0xFC);
                if (token != 0x05) {
                    f++;
                    break;
                }
                currentBuffer += 512;
                f = 0;
            } while (--currentCount);

            if (!sdWait(pdrv, 500)) {
                break;
            }

            if (currentCount == 0) {
                sdStop(pdrv);
                sdDeselectCard(pdrv);

                unsigned int resp;
                if (sdTransaction(pdrv, SEND_STATUS, 0, &resp) || resp) {
                    return false;
                }
                return true;
            } else {
                if (sdCommand(pdrv, STOP_TRANSMISSION, 0, NULL)) {
                    break;
                }

                sdDeselectCard(pdrv);

                if (token == 0x0A) {
                    unsigned int writtenBlocks = 0;
                    if (card->type != CARD_MMC && sdSelectCard(pdrv)) {
                        if (!sdCommand(pdrv, SEND_NUM_WR_BLOCKS, 0, NULL)) {
                            char acmdData[4];
                            if (sdReadBytes(pdrv, acmdData, 4)) {
                                writtenBlocks = acmdData[0] << 24;
                                writtenBlocks |= acmdData[1] << 16;
                                writtenBlocks |= acmdData[2] << 8;
                                writtenBlocks |= acmdData[3];
                            }
                        }
                        sdDeselectCard(pdrv);
                    }
                    currentBuffer = buffer + (writtenBlocks << 9);
                    currentSector = sector + writtenBlocks;
                    currentCount = count - writtenBlocks;
                    continue;
                } else {
                    return false;
                }
            }
        } else {
            break;
        }
    }
    sdDeselectCard(pdrv);
    return false;
}

unsigned long sdGetSectorsCount(uint8_t pdrv)
{
    for (int f = 0; f < 3; f++) {
        if(!sdSelectCard(pdrv)) {
            break;
        }

        if (!sdCommand(pdrv, SEND_CSD, 0, NULL)) {
            char csd[16];
            bool success = sdReadBytes(pdrv, csd, 16);
            sdDeselectCard(pdrv);
            if (success) {
                if ((csd[0] >> 6) == 0x01) {
                    unsigned long size = (
                         ((unsigned long)(csd[7] & 0x3F) << 16)
                         | ((unsigned long)csd[8] << 8)
                         | csd[9]
                     ) + 1;
                    return size << 10;
                }
                unsigned long size = (
                     ((unsigned long)(csd[6] & 0x03) << 10)
                     | ((unsigned long)csd[7] << 2)
                     | ((csd[8] & 0xC0) >> 6)
                 ) + 1;
                size <<= ((
                     ((csd[9] & 0x03) << 1)
                     | ((csd[10] & 0x80) >> 7)
                 ) + 2);
                size <<= (csd[5] & 0x0F);
                return size >> 9;
            }
        } else {
            break;
        }
    }

    sdDeselectCard(pdrv);
    return 0;
}


namespace
{

struct AcquireSPI
{
    ardu_sdcard_t *card;
    explicit AcquireSPI(ardu_sdcard_t* card)
        : card(card)
    {
        card->spi->beginTransaction(SPISettings(card->frequency, MSBFIRST, SPI_MODE0));
    }
    AcquireSPI(ardu_sdcard_t* card, int frequency)
        : card(card)
    {
        card->spi->beginTransaction(SPISettings(frequency, MSBFIRST, SPI_MODE0));
    }
    ~AcquireSPI()
    {
        card->spi->endTransaction();
    }
private:
    AcquireSPI(AcquireSPI const&);
    AcquireSPI& operator=(AcquireSPI const&);
};

}


/*
 * FATFS API
 * */

DSTATUS ff_sd_initialize(uint8_t pdrv)
{
    char token;
    unsigned int resp;
    unsigned int start;
    ardu_sdcard_t * card = s_cards[pdrv];

    if (!(card->status & STA_NOINIT)) {
        return card->status;
    }

    AcquireSPI card_locked(card, 400000);

    digitalWrite(card->ssPin, HIGH);
    for (uint8_t i = 0; i < 20; i++) {
        card->spi->transfer(0XFF);
    }

    if (sdTransaction(pdrv, GO_IDLE_STATE, 0, NULL) != 1) {
        log_w("GO_IDLE_STATE failed");
        goto unknown_card;
    }

    token = sdTransaction(pdrv, CRC_ON_OFF, 1, NULL);
    if (token == 0x5) {
        //old card maybe
        card->supports_crc = false;
    } else if (token != 1) {
        log_w("CRC_ON_OFF failed: %u", token);
        goto unknown_card;
    }

    if (sdTransaction(pdrv, SEND_IF_COND, 0x1AA, &resp) == 1) {
        if ((resp & 0xFFF) != 0x1AA) {
            log_w("SEND_IF_COND failed: %03X", resp & 0xFFF);
            goto unknown_card;
        }

        if (sdTransaction(pdrv, READ_OCR, 0, &resp) != 1 || !(resp & (1 << 20))) {
            log_w("READ_OCR failed: %X", resp);
            goto unknown_card;
        }

        start = millis();
        do {
            token = sdTransaction(pdrv, APP_OP_COND, 0x40100000, NULL);
        } while (token == 1 && (millis() - start) < 1000);

        if (token) {
            log_w("APP_OP_COND failed: %u", token);
            goto unknown_card;
        }

        if (!sdTransaction(pdrv, READ_OCR, 0, &resp)) {
            if (resp & (1 << 30)) {
                card->type = CARD_SDHC;
            } else {
                card->type = CARD_SD;
            }
        } else {
            log_w("READ_OCR failed: %X", resp);
            goto unknown_card;
        }
    } else {
        if (sdTransaction(pdrv, READ_OCR, 0, &resp) != 1 || !(resp & (1 << 20))) {
            log_w("READ_OCR failed: %X", resp);
            goto unknown_card;
        }

        start = millis();
        do {
            token = sdTransaction(pdrv, APP_OP_COND, 0x100000, NULL);
        } while (token == 0x01 && (millis() - start) < 1000);

        if (!token) {
            card->type = CARD_SD;
        } else {
            start = millis();
            do {
                token = sdTransaction(pdrv, SEND_OP_COND, 0x100000, NULL);
            } while (token != 0x00 && (millis() - start) < 1000);

            if (token == 0x00) {
                card->type = CARD_MMC;
            } else {
                log_w("SEND_OP_COND failed: %u", token);
                goto unknown_card;
            }
        }
    }

    if (card->type != CARD_MMC) {
        if (sdTransaction(pdrv, APP_CLR_CARD_DETECT, 0, NULL)) {
            log_w("APP_CLR_CARD_DETECT failed");
            goto unknown_card;
        }
    }

    if (card->type != CARD_SDHC) {
        if (sdTransaction(pdrv, SET_BLOCKLEN, 512, NULL) != 0x00) {
            log_w("SET_BLOCKLEN failed");
            goto unknown_card;
        }
    }

    card->sectors = sdGetSectorsCount(pdrv);

    if (card->frequency > 25000000) {
        card->frequency = 25000000;
    }

    card->status &= ~STA_NOINIT;
    return card->status;

unknown_card:
    card->type = CARD_UNKNOWN;
    return card->status;
}

DSTATUS ff_sd_status(uint8_t pdrv)
{
    return s_cards[pdrv]->status;
}

DRESULT ff_sd_read(uint8_t pdrv, uint8_t* buffer, DWORD sector, UINT count)
{
    ardu_sdcard_t * card = s_cards[pdrv];
    if (card->status & STA_NOINIT) {
        return RES_NOTRDY;
    }
    DRESULT res = RES_OK;

    AcquireSPI lock(card);

    if (count > 1) {
        res = sdReadSectors(pdrv, (char*)buffer, sector, count) ? RES_OK : RES_ERROR;
    } else {
        res = sdReadSector(pdrv, (char*)buffer, sector) ? RES_OK : RES_ERROR;
    }
    return res;
}

DRESULT ff_sd_write(uint8_t pdrv, const uint8_t* buffer, DWORD sector, UINT count)
{
    ardu_sdcard_t * card = s_cards[pdrv];
    if (card->status & STA_NOINIT) {
        return RES_NOTRDY;
    }

    if (card->status & STA_PROTECT) {
        return RES_WRPRT;
    }
    DRESULT res = RES_OK;

    AcquireSPI lock(card);

    if (count > 1) {
        res = sdWriteSectors(pdrv, (const char*)buffer, sector, count) ? RES_OK : RES_ERROR;
    }
    res = sdWriteSector(pdrv, (const char*)buffer, sector) ? RES_OK : RES_ERROR;
    return res;
}

DRESULT ff_sd_ioctl(uint8_t pdrv, uint8_t cmd, void* buff)
{
    switch(cmd) {
    case CTRL_SYNC:
        {
            AcquireSPI lock(s_cards[pdrv]);
            if (sdSelectCard(pdrv)) {
                sdDeselectCard(pdrv);
                return RES_OK;
            }
        }
        return RES_ERROR;
    case GET_SECTOR_COUNT:
        *((unsigned long*) buff) = s_cards[pdrv]->sectors;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *((WORD*) buff) = 512;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *((uint32_t*)buff) = 1;
        return RES_OK;
    }
    return RES_PARERR;
}


/*
 * Public methods
 * */

uint8_t sdcard_uninit(uint8_t pdrv)
{
    ardu_sdcard_t * card = s_cards[pdrv];
    if (pdrv >= FF_VOLUMES || card == NULL) {
        return 1;
    }
    ff_diskio_register(pdrv, NULL);
    s_cards[pdrv] = NULL;
    esp_err_t err = ESP_OK;
    if (card->base_path) {
        err = esp_vfs_fat_unregister_path(card->base_path);
    }
    free(card);
    return err;
}

uint8_t sdcard_init(uint8_t cs, SPIClass * spi, int hz)
{

    uint8_t pdrv = 0xFF;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK || pdrv == 0xFF) {
        return pdrv;
    }

    ardu_sdcard_t * card = (ardu_sdcard_t *)malloc(sizeof(ardu_sdcard_t));
    if (!card) {
        return 0xFF;
    }

    card->base_path = NULL;
    card->frequency = hz;
    card->spi = spi;
    card->ssPin = cs;

    card->supports_crc = true;
    card->type = CARD_NONE;
    card->status = STA_NOINIT;

    pinMode(card->ssPin, OUTPUT);
    digitalWrite(card->ssPin, HIGH);

    s_cards[pdrv] = card;

    static const ff_diskio_impl_t sd_impl = {
        .init = &ff_sd_initialize,
        .status = &ff_sd_status,
        .read = &ff_sd_read,
        .write = &ff_sd_write,
        .ioctl = &ff_sd_ioctl
    };
    ff_diskio_register(pdrv, &sd_impl);

    return pdrv;
}

uint8_t sdcard_unmount(uint8_t pdrv)
{
    ardu_sdcard_t * card = s_cards[pdrv];
    if (pdrv >= FF_VOLUMES || card == NULL) {
        return 1;
    }
    card->status |= STA_NOINIT;
    card->type = CARD_NONE;

    char drv[3] = {(char)('0' + pdrv), ':', 0};
    f_mount(NULL, drv, 0);
    return 0;
}

bool sdcard_mount(uint8_t pdrv, const char* path, uint8_t max_files)
{
    ardu_sdcard_t * card = s_cards[pdrv];
    if(pdrv >= FF_VOLUMES || card == NULL){
        return false;
    }

    if(card->base_path){
        free(card->base_path);
    }
    card->base_path = strdup(path);

    FATFS* fs;
    char drv[3] = {(char)('0' + pdrv), ':', 0};
    esp_err_t err = esp_vfs_fat_register(path, drv, max_files, &fs);
    if (err == ESP_ERR_INVALID_STATE) {
        log_e("esp_vfs_fat_register failed 0x(%x): SD is registered.", err);
        return false;
    } else if (err != ESP_OK) {
        log_e("esp_vfs_fat_register failed 0x(%x)", err);
        return false;
    }

    FRESULT res = f_mount(fs, drv, 1);
    if (res != FR_OK) {
        log_e("f_mount failed 0x(%x)", res);
        esp_vfs_fat_unregister_path(path);
        return false;
    }
    AcquireSPI lock(card);
    card->sectors = sdGetSectorsCount(pdrv);
    return true;
}

uint32_t sdcard_num_sectors(uint8_t pdrv)
{
    ardu_sdcard_t * card = s_cards[pdrv];
    if(pdrv >= FF_VOLUMES || card == NULL){
        return 0;
    }
    return card->sectors;
}

uint32_t sdcard_sector_size(uint8_t pdrv)
{
    if(pdrv >= FF_VOLUMES || s_cards[pdrv] == NULL){
        return 0;
    }
    return 512;
}

sdcard_type_t sdcard_type(uint8_t pdrv)
{
    ardu_sdcard_t * card = s_cards[pdrv];
    if(pdrv >= FF_VOLUMES || card == NULL){
        return CARD_NONE;
    }
    return card->type;
}
