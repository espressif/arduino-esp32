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

extern "C" {
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_spiffs.h"
}

#include "SPIFFS.h"

using namespace fs;

class SPIFFSImpl : public VFSImpl
{
public:
    SPIFFSImpl();
    virtual ~SPIFFSImpl() { }
    virtual bool exists(const char* path);
};

SPIFFSImpl::SPIFFSImpl()
{
}

bool SPIFFSImpl::exists(const char* path)
{
    File f = open(path, "r");
    return (f == true) && !f.isDirectory();
}

SPIFFSFS::SPIFFSFS() : FS(FSImplPtr(new SPIFFSImpl()))
{

}

bool SPIFFSFS::begin(bool formatOnFail, const char * basePath, uint8_t maxOpenFiles)
{
    if(esp_spiffs_mounted(NULL)){
        log_w("SPIFFS Already Mounted!");
        return true;
    }

    esp_vfs_spiffs_conf_t conf = {
      .base_path = basePath,
      .partition_label = NULL,
      .max_files = maxOpenFiles,
      .format_if_mount_failed = false
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if(err == ESP_FAIL && formatOnFail){
        if(format()){
            err = esp_vfs_spiffs_register(&conf);
        }
    }
    if(err != ESP_OK){
        log_e("Mounting SPIFFS failed! Error: %d", err);
        return false;
    }
    _impl->mountpoint(basePath);
    return true;
}

void SPIFFSFS::end()
{
    if(esp_spiffs_mounted(NULL)){
        esp_err_t err = esp_vfs_spiffs_unregister(NULL);
        if(err){
            log_e("Unmounting SPIFFS failed! Error: %d", err);
            return;
        }
        _impl->mountpoint(NULL);
    }
}

bool SPIFFSFS::format()
{
    disableCore0WDT();
    esp_err_t err = esp_spiffs_format(NULL);
    enableCore0WDT();
    if(err){
        log_e("Formatting SPIFFS failed! Error: %d", err);
        return false;
    }
    return true;
}

size_t SPIFFSFS::totalBytes()
{
    size_t total,used;
    if(esp_spiffs_info(NULL, &total, &used)){
        return 0;
    }
    return total;
}

size_t SPIFFSFS::usedBytes()
{
    size_t total,used;
    if(esp_spiffs_info(NULL, &total, &used)){
        return 0;
    }
    return used;
}

SPIFFSFS SPIFFS;

