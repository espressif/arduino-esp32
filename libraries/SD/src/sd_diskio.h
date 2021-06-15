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
#ifndef _SD_DISKIO_H_
#define _SD_DISKIO_H_

#include "Arduino.h"
#include "SPI.h"
#include "sd_defines.h"
// #include "diskio.h"

uint8_t sdcard_init(uint8_t cs, SPIClass * spi, int hz);
uint8_t sdcard_uninit(uint8_t pdrv);

bool sdcard_mount(uint8_t pdrv, const char* path, uint8_t max_files, bool format_if_empty);
uint8_t sdcard_unmount(uint8_t pdrv);

sdcard_type_t sdcard_type(uint8_t pdrv);
uint32_t sdcard_num_sectors(uint8_t pdrv);
uint32_t sdcard_sector_size(uint8_t pdrv);
bool sd_read_raw(uint8_t pdrv, uint8_t* buffer, uint32_t sector);
bool sd_write_raw(uint8_t pdrv, uint8_t* buffer, uint32_t sector);

#endif /* _SD_DISKIO_H_ */
