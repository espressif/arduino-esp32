// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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

//esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t * conf);

//esp_err_t esp_vfs_littlefs_unregister(const char* partition_label);
//esp_err_t esp_littlefs_format(const char* partition_label);
//esp_err_t esp_littlefs_info(const char* partition_label, size_t *total_bytes, size_t *used_bytes);

//#define LFS_NAME "spiffs"
static constexpr const char LFS_NAME[] = "spiffs";

#include "vfs_api.h"

extern "C" {
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_littlefs.h"
}

#include "LITTLEFS.h"

using namespace fs;

class LITTLEFSImpl : public VFSImpl
{
public:
    LITTLEFSImpl();
    virtual ~LITTLEFSImpl() { }
    virtual bool exists(const char* path);
};

LITTLEFSImpl::LITTLEFSImpl()
{
}

bool LITTLEFSImpl::exists(const char* path)
{
    File f = open(path, "r");
    return (f == true) && !f.isDirectory();
}

LITTLEFSFS::LITTLEFSFS() : FS(FSImplPtr(new LITTLEFSImpl()))
{

}

bool LITTLEFSFS::begin(bool formatOnFail, const char * basePath, uint8_t maxOpenFiles)
{
    if(esp_littlefs_mounted(LFS_NAME)){
        log_w("LITTLEFS Already Mounted!");
        return true;
    }

    esp_vfs_littlefs_conf_t conf = {
      .base_path = basePath,
      .partition_label = LFS_NAME,
      //.max_files = maxOpenFiles,
      .format_if_mount_failed = false
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if(err == ESP_FAIL && formatOnFail){
        if(format()){
            err = esp_vfs_littlefs_register(&conf);
        }
    }
    if(err != ESP_OK){
        log_e("Mounting LITTLEFS failed! Error: %d", err);
        return false;
    }
    _impl->mountpoint(basePath);
    return true;
}

void LITTLEFSFS::end()
{
    if(esp_littlefs_mounted(LFS_NAME)){
        esp_err_t err = esp_vfs_littlefs_unregister(LFS_NAME);
        if(err){
            log_e("Unmounting LITTLEFS failed! Error: %d", err);
            return;
        }
        _impl->mountpoint(NULL);
    }
}

bool LITTLEFSFS::format()
{
    disableCore0WDT();
    esp_err_t err = esp_littlefs_format(LFS_NAME);
    enableCore0WDT();
    if(err){
        log_e("Formatting LITTLEFS failed! Error: %d", err);
        return false;
    }
    return true;
}

size_t LITTLEFSFS::totalBytes()
{
    size_t total,used;
    if(esp_littlefs_info(LFS_NAME, &total, &used)){
        return 0;
    }
    return total;
}

size_t LITTLEFSFS::usedBytes()
{
    size_t total,used;
    if(esp_littlefs_info(LFS_NAME, &total, &used)){
        return 0;
    }
    return used;
}

LITTLEFSFS LITTLEFS;

