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
}
#include "sdkconfig.h"
#include "LittleFS.h"

#ifdef CONFIG_LITTLEFS_PAGE_SIZE
extern "C" {
#include "esp_littlefs.h"
}

using namespace fs;

class LittleFSImpl : public VFSImpl {
public:
  LittleFSImpl();
  virtual ~LittleFSImpl() {}
};

LittleFSImpl::LittleFSImpl() {}

LittleFSFS::LittleFSFS() : FS(FSImplPtr(new LittleFSImpl())), partitionLabel_(NULL) {}

LittleFSFS::~LittleFSFS() {
  if (partitionLabel_) {
    free(partitionLabel_);
    partitionLabel_ = NULL;
  }
}

bool LittleFSFS::begin(bool formatOnFail, const char *basePath, uint8_t maxOpenFiles, const char *partitionLabel) {

  if (partitionLabel_) {
    free(partitionLabel_);
    partitionLabel_ = NULL;
  }

  if (partitionLabel) {
    partitionLabel_ = strdup(partitionLabel);
  }

  if (esp_littlefs_mounted(partitionLabel_)) {
    log_w("LittleFS Already Mounted!");
    return true;
  }

  esp_vfs_littlefs_conf_t conf = {
    .base_path = basePath,
    .partition_label = partitionLabel_,
    .partition = NULL,
    .format_if_mount_failed = false,
    .read_only = false,
    .dont_mount = false,
    .grow_on_mount = true
  };

  esp_err_t err = esp_vfs_littlefs_register(&conf);
  if (err == ESP_FAIL && formatOnFail) {
    if (format()) {
      err = esp_vfs_littlefs_register(&conf);
    }
  }
  if (err != ESP_OK) {
    log_e("Mounting LittleFS failed! Error: %d", err);
    return false;
  }
  _impl->mountpoint(basePath);
  return true;
}

void LittleFSFS::end() {
  if (esp_littlefs_mounted(partitionLabel_)) {
    esp_err_t err = esp_vfs_littlefs_unregister(partitionLabel_);
    if (err) {
      log_e("Unmounting LittleFS failed! Error: %d", err);
      return;
    }
    _impl->mountpoint(NULL);
  }
}

bool LittleFSFS::format() {
  disableCore0WDT();
  esp_err_t err = esp_littlefs_format(partitionLabel_);
  enableCore0WDT();
  if (err) {
    log_e("Formatting LittleFS failed! Error: %d", err);
    return false;
  }
  return true;
}

size_t LittleFSFS::totalBytes() {
  size_t total, used;
  if (esp_littlefs_info(partitionLabel_, &total, &used)) {
    return 0;
  }
  return total;
}

size_t LittleFSFS::usedBytes() {
  size_t total, used;
  if (esp_littlefs_info(partitionLabel_, &total, &used)) {
    return 0;
  }
  return used;
}

LittleFSFS LittleFS;
#endif
