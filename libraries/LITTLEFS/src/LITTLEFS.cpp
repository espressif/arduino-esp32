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
    return (f == true);
}

LITTLEFSFS::LITTLEFSFS() : FS(FSImplPtr(new LITTLEFSImpl())), partitionLabel_(NULL)
{
}

LITTLEFSFS::~LITTLEFSFS()
{
    if (partitionLabel_){
        free(partitionLabel_);
        partitionLabel_ = NULL;
    }
}

bool LITTLEFSFS::begin(bool formatOnFail, const char * basePath, uint8_t maxOpenFiles, const char * partitionLabel)
{

    if (partitionLabel_){
        free(partitionLabel_);
        partitionLabel_ = NULL;
    }

    if (partitionLabel){
        partitionLabel_ = strdup(partitionLabel);
    }

    if(esp_littlefs_mounted(partitionLabel_)){
        log_w("LITTLEFS Already Mounted!");
        return true;
    }

    esp_vfs_littlefs_conf_t conf = {
      .base_path = basePath,
      .partition_label = partitionLabel_,
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
    if(esp_littlefs_mounted(partitionLabel_)){
        esp_err_t err = esp_vfs_littlefs_unregister(partitionLabel_);
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
    esp_err_t err = esp_littlefs_format(partitionLabel_);
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
    if(esp_littlefs_info(partitionLabel_, &total, &used)){
        return 0;
    }
    return total;
}

size_t LITTLEFSFS::usedBytes()
{
    size_t total,used;
    if(esp_littlefs_info(partitionLabel_, &total, &used)){
        return 0;
    }
    return used;
}

LITTLEFSFS LITTLEFS;

