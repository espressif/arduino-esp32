// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
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
#include "ff.h"
#include "FS.h"
#include "SD.h"

using namespace fs;

SDFS::SDFS(FSImplPtr impl): FS(impl), _pdrv(0xFF) {}

bool SDFS::begin(uint8_t ssPin, SPIClass &spi, uint32_t frequency, const char * mountpoint, uint8_t max_files)
{
    if(_pdrv != 0xFF) {
        return true;
    }

    spi.begin();

    _pdrv = sdcard_init(ssPin, &spi, frequency);
    if(_pdrv == 0xFF) {
        return false;
    }

    if(!sdcard_mount(_pdrv, mountpoint, max_files)){
        sdcard_unmount(_pdrv);
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

uint64_t SDFS::totalBytes()
{
	FATFS* fsinfo;
	DWORD fre_clust;
	if(f_getfree("0:",&fre_clust,&fsinfo)!= 0) return 0;
    uint64_t size = ((uint64_t)(fsinfo->csize))*(fsinfo->n_fatent - 2)
#if _MAX_SS != 512
        *(fsinfo->ssize);
#else
        *512;
#endif
	return size;
}

uint64_t SDFS::usedBytes()
{
	FATFS* fsinfo;
	DWORD fre_clust;
	if(f_getfree("0:",&fre_clust,&fsinfo)!= 0) return 0;
	uint64_t size = ((uint64_t)(fsinfo->csize))*((fsinfo->n_fatent - 2) - (fsinfo->free_clst))
#if _MAX_SS != 512
        *(fsinfo->ssize);
#else
        *512;
#endif
	return size;
}

SDFS SD = SDFS(FSImplPtr(new VFSImpl()));
