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
#include "esp_sleep.h"
#include "esp_spi_flash.h"
#include <memory>
#include <soc/soc.h>
#include <esp_partition.h>
extern "C" {
#include "esp_ota_ops.h"
#include "esp_image_format.h"
}
#include <MD5Builder.h>

#include "soc/spi_reg.h"
#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/rom/spi_flash.h"
#include "soc/efuse_reg.h"
#define ESP_FLASH_IMAGE_BASE 0x1000     // Flash offset containing flash size and spi mode
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/spi_flash.h"
#include "soc/efuse_reg.h"
#define ESP_FLASH_IMAGE_BASE 0x1000
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/spi_flash.h"
#include "soc/efuse_reg.h"
#define ESP_FLASH_IMAGE_BASE 0x0000     // Esp32s3 is located at 0x0000
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/spi_flash.h"
#define ESP_FLASH_IMAGE_BASE 0x0000     // Esp32c3 is located at 0x0000
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/spi_flash.h"
#define ESP_FLASH_IMAGE_BASE 0x1000
#endif

// REG_SPI_BASE is not defined for S3/C3 ??

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3
#ifdef REG_SPI_BASE
#undef REG_SPI_BASE
#endif  // REG_SPI_BASE
#define REG_SPI_BASE(i) (DR_REG_SPI1_BASE + (((i) > 1) ? (((i) * 0x1000) + 0x20000) : (((~(i)) & 1) * 0x1000)))
#endif  // TARGET

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

void EspClass::deepSleep(uint64_t time_us)
{
    esp_deep_sleep(time_us);
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
	if(psramFound()){
	    multi_heap_info_t info;
	    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
	    return info.total_free_bytes + info.total_allocated_bytes;
	}
	return 0;
}

uint32_t EspClass::getFreePsram(void)
{
	if(psramFound()){
	    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
	}
	return 0;
}

uint32_t EspClass::getMinFreePsram(void)
{
	if(psramFound()){
	    return heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
	}
	return 0;
}

uint32_t EspClass::getMaxAllocPsram(void)
{
	if(psramFound()){
	    return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
	}
	return 0;
}

static uint32_t sketchSize(sketchSize_t response) {
    esp_image_metadata_t data;
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) return 0;
    const esp_partition_pos_t running_pos  = {
        .offset = running->address,
        .size = running->size,
    };
    data.start_addr = running_pos.offset;
    esp_image_verify(ESP_IMAGE_VERIFY, &running_pos, &data);
    if (response) {
        return running_pos.size - data.image_len;
    } else {
        return data.image_len;
    }
}

uint32_t EspClass::getSketchSize () {
    return sketchSize(SKETCH_SIZE_TOTAL);
}

String EspClass::getSketchMD5()
{
    static String result;
    if (result.length()) {
        return result;
    }
    uint32_t lengthLeft = getSketchSize();

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        log_e("Partition could not be found");

        return String();
    }
    const size_t bufSize = SPI_FLASH_SEC_SIZE;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[bufSize]);
    uint32_t offset = 0;
    if(!buf.get()) {
        log_e("Not enough memory to allocate buffer");

        return String();
    }
    MD5Builder md5;
    md5.begin();
    while( lengthLeft > 0) {
        size_t readBytes = (lengthLeft < bufSize) ? lengthLeft : bufSize;
        if (!ESP.flashRead(running->address + offset, reinterpret_cast<uint32_t*>(buf.get()), (readBytes + 3) & ~3)) {
            log_e("Could not read buffer from flash");

            return String();
        }
        md5.add(buf.get(), readBytes);
        lengthLeft -= readBytes;
        offset += readBytes;

        #if CONFIG_FREERTOS_UNICORE
        delay(1);  // Fix solo WDT
        #endif
    }
    md5.calculate();
    result = md5.toString();
    return result;
}

uint32_t EspClass::getFreeSketchSpace () {
    const esp_partition_t* _partition = esp_ota_get_next_update_partition(NULL);
    if(!_partition){
        return 0;
    }

    return _partition->size;
}

uint8_t EspClass::getChipRevision(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    return chip_info.revision;
}

const char * EspClass::getChipModel(void)
{
#if CONFIG_IDF_TARGET_ESP32
    uint32_t chip_ver = REG_GET_FIELD(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_VER_PKG);
    uint32_t pkg_ver = chip_ver & 0x7;
    switch (pkg_ver) {
        case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ6 :
            if (getChipRevision() == 3)
                return "ESP32-D0WDQ6-V3";  
            else
                return "ESP32-D0WDQ6";
        case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ5 :
            if (getChipRevision() == 3)
                return "ESP32-D0WD-V3";  
            else
                return "ESP32-D0WD";
        case EFUSE_RD_CHIP_VER_PKG_ESP32D2WDQ5 :
            return "ESP32-D2WD";
        case EFUSE_RD_CHIP_VER_PKG_ESP32PICOD2 :
            return "ESP32-PICO-D2";
        case EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4 :
            return "ESP32-PICO-D4";
        case EFUSE_RD_CHIP_VER_PKG_ESP32PICOV302 :
            return "ESP32-PICO-V3-02";
        case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDR2V3 :
            return "ESP32-D0WDR2-V3";
        default:
            return "Unknown";
    }
#elif CONFIG_IDF_TARGET_ESP32S2
    uint32_t pkg_ver = REG_GET_FIELD(EFUSE_RD_MAC_SPI_SYS_3_REG, EFUSE_PKG_VERSION);
    switch (pkg_ver) {
    case 0:
      return "ESP32-S2";
    case 1:
      return "ESP32-S2FH16";
    case 2:
      return "ESP32-S2FH32";
    default:
      return "ESP32-S2 (Unknown)";
    }
#elif CONFIG_IDF_TARGET_ESP32S3
    return "ESP32-S3";
#elif CONFIG_IDF_TARGET_ESP32C3
    return "ESP32-C3";
#endif
}

uint8_t EspClass::getChipCores(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    return chip_info.cores;
}

const char * EspClass::getSdkVersion(void)
{
    return esp_get_idf_version();
}

uint32_t ESP_getFlashChipId(void)
{
  uint32_t id = g_rom_flashchip.device_id;
  id = ((id & 0xff) << 16) | ((id >> 16) & 0xff) | (id & 0xff00);
  return id;
}

uint32_t EspClass::getFlashChipSize(void)
{
  uint32_t id = (ESP_getFlashChipId() >> 16) & 0xFF;
  return 2 << (id - 1);
}

uint32_t EspClass::getFlashChipSpeed(void)
{
    esp_image_header_t fhdr;
    if(flashRead(ESP_FLASH_IMAGE_BASE, (uint32_t*)&fhdr, sizeof(esp_image_header_t)) && fhdr.magic != ESP_IMAGE_HEADER_MAGIC) {
        return 0;
    }
    return magicFlashChipSpeed(fhdr.spi_speed);
}

FlashMode_t EspClass::getFlashChipMode(void)
{
   #if CONFIG_IDF_TARGET_ESP32S2
   uint32_t spi_ctrl = REG_READ(PERIPHS_SPI_FLASH_CTRL);
   #else
   uint32_t spi_ctrl = REG_READ(SPI_CTRL_REG(0));
   #endif
   /* Not all of the following constants are already defined in older versions of spi_reg.h, so do it manually for now*/
   if (spi_ctrl & BIT(24)) { //SPI_FREAD_QIO
       return (FM_QIO);
   } else if (spi_ctrl & BIT(20)) { //SPI_FREAD_QUAD
       return (FM_QOUT);
   } else if (spi_ctrl &  BIT(23)) { //SPI_FREAD_DIO
       return (FM_DIO);
   } else if (spi_ctrl & BIT(14)) { // SPI_FREAD_DUAL
       return (FM_DOUT);
   } else if (spi_ctrl & BIT(13)) { //SPI_FASTRD_MODE
       return (FM_FAST_READ);
   } else {
       return (FM_SLOW_READ);
   }
   return (FM_DOUT);
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

bool EspClass::partitionEraseRange(const esp_partition_t *partition, uint32_t offset, size_t size) 
{
    return esp_partition_erase_range(partition, offset, size) == ESP_OK;
}

bool EspClass::partitionWrite(const esp_partition_t *partition, uint32_t offset, uint32_t *data, size_t size) 
{
    return esp_partition_write(partition, offset, data, size) == ESP_OK;
}

bool EspClass::partitionRead(const esp_partition_t *partition, uint32_t offset, uint32_t *data, size_t size) 
{
    return esp_partition_read(partition, offset, data, size) == ESP_OK;
}

uint64_t EspClass::getEfuseMac(void)
{
    uint64_t _chipmacid = 0LL;
    esp_efuse_mac_get_default((uint8_t*) (&_chipmacid));
    return _chipmacid;
}
