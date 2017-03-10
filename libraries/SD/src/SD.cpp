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

#include "vfs_api.h"
#include "sd_diskio.h"
#include "FS.h"
#include "SD.h"

using namespace fs;

SDFS::SDFS(FSImplPtr impl): FS(impl), _pdrv(0xFF) {}

bool SDFS::begin(uint8_t ssPin, SPIClass &spi, uint32_t frequency, const char * mountpoint)
{
    if(_pdrv != 0xFF) {
        return true;
    }

    spi.begin();

    _pdrv = sdcard_init(ssPin, &spi, frequency);
    if(_pdrv == 0xFF) {
        return false;
    }

    if(!sdcard_mount(_pdrv, mountpoint)){
        sdcard_uninit(_pdrv);
        _pdrv = 0xFF;
        return false;
    }

    _impl->mountpoint(mountpoint);
    return true;
}

void SDFS::end()
{
    if(_pdrv != 0xFF) {
        _impl->mountpoint(NULL);
        sdcard_unmount(_pdrv);

        sdcard_uninit(_pdrv);
        _pdrv = 0xFF;
    }
}

sdcard_type_t SDFS::cardType()
{
    if(_pdrv == 0xFF) {
        return CARD_NONE;
    }
    return sdcard_type(_pdrv);
}

uint64_t SDFS::cardSize()
{
    if(_pdrv == 0xFF) {
        return 0;
    }
    size_t sectors = sdcard_num_sectors(_pdrv);
    size_t sectorSize = sdcard_sector_size(_pdrv);
    return (uint64_t)sectors * sectorSize;
}

SDFS SD = SDFS(FSImplPtr(new VFSImpl()));
