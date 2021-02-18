// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "vfs_api.h"
extern "C" {
#include "esp_vfs_fat.h"
#include "diskio.h"
#include "diskio_wl.h"
#include "vfs_fat_internal.h"
}
#include "FFat.h"

using namespace fs;

F_Fat::F_Fat(FSImplPtr impl)
    : FS(impl)
{}

const esp_partition_t *check_ffat_partition(const char* label)
{
    const esp_partition_t* ck_part = esp_partition_find_first(
       ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, label);
    if (!ck_part) {
        log_e("No FAT partition found with label %s", label);
        return NULL;
    }
    return ck_part;
}

bool F_Fat::begin(bool formatOnFail, const char * basePath, uint8_t maxOpenFiles, const char * partitionLabel)
{
    if(_wl_handle != WL_INVALID_HANDLE){
        log_w("Already Mounted!");
        return true;
    }     

    if (!check_ffat_partition(partitionLabel)){
        log_e("No fat partition found on flash");
        return false;
    }

    esp_vfs_fat_mount_config_t conf = {
      .format_if_mount_failed = formatOnFail,
      .max_files = maxOpenFiles,
      .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount(basePath, partitionLabel, &conf, &_wl_handle);
    if(err){
        log_e("Mounting FFat partition failed! Error: %d", err);
        esp_vfs_fat_spiflash_unmount(basePath, _wl_handle);
        _wl_handle = WL_INVALID_HANDLE;
        return false;
    }
    _impl->mountpoint(basePath);
    return true;
}

void F_Fat::end()
{
    if(_wl_handle != WL_INVALID_HANDLE){
        esp_err_t err = esp_vfs_fat_spiflash_unmount(_impl->mountpoint(), _wl_handle);
        if(err){
            log_e("Unmounting FFat partition failed! Error: %d", err);
            return;
        }
        _wl_handle = WL_INVALID_HANDLE;
        _impl->mountpoint(NULL);
    }
}

bool F_Fat::format(bool full_wipe, char* partitionLabel)
{
    esp_err_t result;
    bool res = true;
    if(_wl_handle != WL_INVALID_HANDLE){
        log_w("Already Mounted!");
        return false;
    }
    wl_handle_t temp_handle;
// Attempt to mount to see if there is already data
    const esp_partition_t *ffat_partition = check_ffat_partition(partitionLabel);
    if (!ffat_partition){
        log_w("No partition!");
        return false;
    } 
    result = wl_mount(ffat_partition, &temp_handle);

    if (result == ESP_OK) {
// Wipe disk- quick just wipes the FAT. Full zeroes the whole disk
        uint32_t wipe_size = full_wipe ? wl_size(temp_handle) : 16384;
        wl_erase_range(temp_handle, 0, wipe_size);
        wl_unmount(temp_handle);
    } else {
        res = false;
        log_w("wl_mount failed!");
    }
// Now do a mount with format_if_fail (which it will)
    esp_vfs_fat_mount_config_t conf = {
      .format_if_mount_failed = true,
      .max_files = 1,
      .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };
    result = esp_vfs_fat_spiflash_mount("/format_ffat", partitionLabel, &conf, &temp_handle);
    esp_vfs_fat_spiflash_unmount("/format_ffat", temp_handle);
    if (result != ESP_OK){
        res = false;
        log_w("esp_vfs_fat_spiflash_mount failed!");
    }
    return res;
}

size_t F_Fat::totalBytes()
{
    FATFS *fs;
    DWORD free_clust, tot_sect, sect_size;

    BYTE pdrv = ff_diskio_get_pdrv_wl(_wl_handle);
    char drv[3] = {(char)(48+pdrv), ':', 0};
    if ( f_getfree(drv, &free_clust, &fs) != FR_OK){
        return 0;
    }
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    sect_size = CONFIG_WL_SECTOR_SIZE;
    return tot_sect * sect_size;
}

size_t F_Fat::usedBytes()
{
    FATFS *fs;
    DWORD free_clust, used_sect, sect_size;

    BYTE pdrv = ff_diskio_get_pdrv_wl(_wl_handle);
    char drv[3] = {(char)(48+pdrv), ':', 0};
    if ( f_getfree(drv, &free_clust, &fs) != FR_OK){
        return 0;
    }
    used_sect = (fs->n_fatent - 2 - free_clust) * fs->csize;
    sect_size = CONFIG_WL_SECTOR_SIZE;
    return used_sect * sect_size;
}

size_t F_Fat::freeBytes()
{

    FATFS *fs;
    DWORD free_clust, free_sect, sect_size;

    BYTE pdrv = ff_diskio_get_pdrv_wl(_wl_handle);
    char drv[3] = {(char)(48+pdrv), ':', 0};
    if ( f_getfree(drv, &free_clust, &fs) != FR_OK){
        return 0;
    }
    free_sect = free_clust * fs->csize;
    sect_size = CONFIG_WL_SECTOR_SIZE;
    return free_sect * sect_size;
}

bool F_Fat::exists(const char* path)
{
    File f = open(path, "r");
    return (f == true) && !f.isDirectory();
}

bool F_Fat::exists(const String& path)
{
    return exists(path.c_str());
}


F_Fat FFat = F_Fat(FSImplPtr(new VFSImpl()));
