extern "C" {
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_spiffs.h"
}

#include "SPIFFS.h"

using namespace fs;

SPIFFSImpl::SPIFFSImpl()
{
}

bool SPIFFSImpl::exists(const char* path) {
    File f = open(path, "r");
    return (f == true) && !f.isDirectory();
}

SPIFFSFS::SPIFFSFS() : FS(FSImplPtr(new SPIFFSImpl())) {

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

bool SPIFFSFS::format() {
    disableCore0WDT();
    esp_err_t err = esp_spiffs_format(NULL);
    enableCore0WDT();
    if(err){
        log_e("Formatting SPIFFS failed! Error: %d", err);
        return false;
    }
    return true;
}

size_t SPIFFSFS::totalBytes() {
    size_t total,used;
    if(esp_spiffs_info(NULL, &total, &used)){
        return 0;
    }
    return total;
}

size_t SPIFFSFS::usedBytes() {
    size_t total,used;
    if(esp_spiffs_info(NULL, &total, &used)){
        return 0;
    }
    return used;
}

SPIFFSFS SPIFFS;

