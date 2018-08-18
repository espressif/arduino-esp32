/*
 Esp.cpp - ESP31B-specific APIs
 Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Arduino.h"
#include "Esp.h"
#include "rom/spi_flash.h"
#include "esp_sleep.h"
#include "esp_spi_flash.h"
#include <memory>
#include <soc/soc.h>
#include <soc/efuse_reg.h>

/* Main header of binary image */
typedef struct {
    uint8_t magic;
    uint8_t segment_count;
    uint8_t spi_mode;      /* flash read mode (esp_image_spi_mode_t as uint8_t) */
    uint8_t spi_speed: 4;  /* flash frequency (esp_image_spi_freq_t as uint8_t) */
    uint8_t spi_size: 4;   /* flash chip size (esp_image_flash_size_t as uint8_t) */
    uint32_t entry_addr;
    uint8_t encrypt_flag;    /* encrypt flag */
    uint8_t extra_header[15]; /* ESP32 additional header, unused by second bootloader */
} esp_image_header_t;

#define ESP_IMAGE_HEADER_MAGIC 0xE9

/**
 * User-defined Literals
 *  usage:
 *
 *   uint32_t = test = 10_MHz; // --> 10000000
 */

unsigned long long operator"" _kHz(unsigned long long x)
{
    return x * 1000;
}

unsigned long long operator"" _MHz(unsigned long long x)
{
    return x * 1000 * 1000;
}

unsigned long long operator"" _GHz(unsigned long long x)
{
    return x * 1000 * 1000 * 1000;
}

unsigned long long operator"" _kBit(unsigned long long x)
{
    return x * 1024;
}

unsigned long long operator"" _MBit(unsigned long long x)
{
    return x * 1024 * 1024;
}

unsigned long long operator"" _GBit(unsigned long long x)
{
    return x * 1024 * 1024 * 1024;
}

unsigned long long operator"" _kB(unsigned long long x)
{
    return x * 1024;
}

unsigned long long operator"" _MB(unsigned long long x)
{
    return x * 1024 * 1024;
}

unsigned long long operator"" _GB(unsigned long long x)
{
    return x * 1024 * 1024 * 1024;
}


EspClass ESP;

void EspClass::deepSleep(uint32_t time_us)
{
    esp_deep_sleep(time_us);
}

uint32_t EspClass::getCycleCount()
{
    uint32_t ccount;
    __asm__ __volatile__("esync; rsr %0,ccount":"=a" (ccount));
    return ccount;
}

void EspClass::restart(void)
{
    esp_restart();
}

uint32_t EspClass::getHeapSize(void)
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    return info.total_free_bytes + info.total_allocated_bytes;
}

uint32_t EspClass::getFreeHeap(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
}

uint32_t EspClass::getMinFreeHeap(void)
{
    return heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
}

uint32_t EspClass::getMaxAllocHeap(void)
{
    return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
}

uint32_t EspClass::getPsramSize(void)
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    return info.total_free_bytes + info.total_allocated_bytes;
}

uint32_t EspClass::getFreePsram(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

uint32_t EspClass::getMinFreePsram(void)
{
    return heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
}

uint32_t EspClass::getMaxAllocPsram(void)
{
    return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}

uint8_t EspClass::getChipRevision(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    return chip_info.revision;
}

const char * EspClass::getSdkVersion(void)
{
    return esp_get_idf_version();
}

uint32_t EspClass::getFlashChipSize(void)
{
    esp_image_header_t fhdr;
    if(flashRead(0x1000, (uint32_t*)&fhdr, sizeof(esp_image_header_t)) && fhdr.magic != ESP_IMAGE_HEADER_MAGIC) {
        return 0;
    }
    return magicFlashChipSize(fhdr.spi_size);
}

uint32_t EspClass::getFlashChipSpeed(void)
{
    esp_image_header_t fhdr;
    if(flashRead(0x1000, (uint32_t*)&fhdr, sizeof(esp_image_header_t)) && fhdr.magic != ESP_IMAGE_HEADER_MAGIC) {
        return 0;
    }
    return magicFlashChipSpeed(fhdr.spi_speed);
}

FlashMode_t EspClass::getFlashChipMode(void)
{
    esp_image_header_t fhdr;
    if(flashRead(0x1000, (uint32_t*)&fhdr, sizeof(esp_image_header_t)) && fhdr.magic != ESP_IMAGE_HEADER_MAGIC) {
        return FM_UNKNOWN;
    }
    return magicFlashChipMode(fhdr.spi_mode);
}

uint32_t EspClass::magicFlashChipSize(uint8_t byte)
{
    switch(byte & 0x0F) {
    case 0x0: // 8 MBit (1MB)
        return (1_MB);
    case 0x1: // 16 MBit (2MB)
        return (2_MB);
    case 0x2: // 32 MBit (4MB)
        return (4_MB);
    case 0x3: // 64 MBit (8MB)
        return (8_MB);
    case 0x4: // 128 MBit (16MB)
        return (16_MB);
    default: // fail?
        return 0;
    }
}

uint32_t EspClass::magicFlashChipSpeed(uint8_t byte)
{
    switch(byte & 0x0F) {
    case 0x0: // 40 MHz
        return (40_MHz);
    case 0x1: // 26 MHz
        return (26_MHz);
    case 0x2: // 20 MHz
        return (20_MHz);
    case 0xf: // 80 MHz
        return (80_MHz);
    default: // fail?
        return 0;
    }
}

FlashMode_t EspClass::magicFlashChipMode(uint8_t byte)
{
    FlashMode_t mode = (FlashMode_t) byte;
    if(mode > FM_SLOW_READ) {
        mode = FM_UNKNOWN;
    }
    return mode;
}

bool EspClass::flashEraseSector(uint32_t sector)
{
    return spi_flash_erase_sector(sector) == ESP_OK;
}

// Warning: These functions do not work with encrypted flash
bool EspClass::flashWrite(uint32_t offset, uint32_t *data, size_t size)
{
    return spi_flash_write(offset, (uint32_t*) data, size) == ESP_OK;
}

bool EspClass::flashRead(uint32_t offset, uint32_t *data, size_t size)
{
    return spi_flash_read(offset, (uint32_t*) data, size) == ESP_OK;
}


uint64_t EspClass::getEfuseMac(void)
{
    uint64_t _chipmacid = 0LL;
    esp_efuse_mac_get_default((uint8_t*) (&_chipmacid));
    return _chipmacid;
}
