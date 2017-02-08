// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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

#include "ESP32NVS.h"

#include "nvs.h"

const char * nvs_errors[] = { "OTHER", "NOT_INITIALIZED", "NOT_FOUND", "TYPE_MISMATCH", "READ_ONLY", "NOT_ENOUGH_SPACE", "INVALID_NAME", "INVALID_HANDLE", "REMOVE_FAILED", "KEY_TOO_LONG", "PAGE_FULL", "INVALID_STATE", "INVALID_LENGHT"};
#define nvs_error(e) (((e)>ESP_ERR_NVS_BASE)?nvs_errors[(e)&~(ESP_ERR_NVS_BASE)]:nvs_errors[0])

NVSClass::NVSClass()
    :_handle(0)
    ,_started(false)
    ,_readOnly(false)
{}

NVSClass::~NVSClass(){
    end();
}

bool NVSClass::begin(const char * name, bool readOnly){
    if(_started){
        return false;
    }
    _readOnly = readOnly;
    esp_err_t err = nvs_open(name, readOnly?NVS_READONLY:NVS_READWRITE, &_handle);
    if(err){
        log_e("nvs_open failed: %s", nvs_error(err));
        return false;
    }
    _started = true;
    return true;
}

void NVSClass::end(){
    if(!_started){
        return;
    }
    nvs_close(_handle);
    _started = false;
}

/*
 * Erase
 * */

bool NVSClass::erase(){
    if(!_started || _readOnly){
        return false;
    }
    esp_err_t err = nvs_erase_all(_handle);
    if(err){
        log_e("nvs_erase_all fail: %s", nvs_error(err));
        return false;
    }
    return true;
}

bool NVSClass::erase(const char * key){
    if(!_started || !key || _readOnly){
        return false;
    }
    esp_err_t err = nvs_erase_key(_handle, key);
    if(err){
        log_e("nvs_erase_key fail: %s %s", key, nvs_error(err));
        return false;
    }
    return true;
}

/*
 * Write
 * */

size_t NVSClass::writeChar(const char* key, int8_t value){
    if(!_started || !key || _readOnly){
        return 0;
    }
    esp_err_t err = nvs_set_i8(_handle, key, value);
    if(err){
        log_e("nvs_set_i8 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if(err){
        log_e("nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 1;
}

size_t NVSClass::writeUChar(const char* key, uint8_t value){
    if(!_started || !key || _readOnly){
        return 0;
    }
    esp_err_t err = nvs_set_u8(_handle, key, value);
    if(err){
        log_e("nvs_set_u8 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if(err){
        log_e("nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 1;
}

size_t NVSClass::writeShort(const char* key, int16_t value){
    if(!_started || !key || _readOnly){
        return 0;
    }
    esp_err_t err = nvs_set_i16(_handle, key, value);
    if(err){
        log_e("nvs_set_i16 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if(err){
        log_e("nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 2;
}

size_t NVSClass::writeUShort(const char* key, uint16_t value){
    if(!_started || !key || _readOnly){
        return 0;
    }
    esp_err_t err = nvs_set_u16(_handle, key, value);
    if(err){
        log_e("nvs_set_u16 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if(err){
        log_e("nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 2;
}

size_t NVSClass::writeInt(const char* key, int32_t value){
    if(!_started || !key || _readOnly){
        return 0;
    }
    esp_err_t err = nvs_set_i32(_handle, key, value);
    if(err){
        log_e("nvs_set_i32 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if(err){
        log_e("nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 4;
}

size_t NVSClass::writeUInt(const char* key, uint32_t value){
    if(!_started || !key || _readOnly){
        return 0;
    }
    esp_err_t err = nvs_set_u32(_handle, key, value);
    if(err){
        log_e("nvs_set_u32 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if(err){
        log_e("nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 4;
}

size_t NVSClass::writeLong(const char* key, int64_t value){
    if(!_started || !key || _readOnly){
        return 0;
    }
    esp_err_t err = nvs_set_i64(_handle, key, value);
    if(err){
        log_e("nvs_set_i64 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if(err){
        log_e("nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 8;
}

size_t NVSClass::writeULong(const char* key, uint64_t value){
    if(!_started || !key || _readOnly){
        return 0;
    }
    esp_err_t err = nvs_set_u64(_handle, key, value);
    if(err){
        log_e("nvs_set_u64 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if(err){
        log_e("nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 8;
}

size_t NVSClass::writeString(const char* key, const char* value){
    if(!_started || !key || !value || _readOnly){
        return 0;
    }
    esp_err_t err = nvs_set_str(_handle, key, value);
    if(err){
        log_e("nvs_set_str fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if(err){
        log_e("nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return strlen(value);
}

size_t NVSClass::writeBytes(const char* key, const void* value, size_t len){
    if(!_started || !key || !value || !len || _readOnly){
        return 0;
    }
    esp_err_t err = nvs_set_blob(_handle, key, value, len);
    if(err){
        log_e("nvs_set_blob fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if(err){
        log_e("nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return len;
}

/*
 * Read
 * */

int8_t NVSClass::readChar(const char* key){
    int8_t value = 0;
    if(!_started || !key){
        return value;
    }
    esp_err_t err = nvs_get_i8(_handle, key, &value);
    if(err){
        log_e("nvs_get_i8 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

uint8_t NVSClass::readUChar(const char* key){
    uint8_t value = 0;
    if(!_started || !key){
        return value;
    }
    esp_err_t err = nvs_get_u8(_handle, key, &value);
    if(err){
        log_e("nvs_get_u8 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

int16_t NVSClass::readShort(const char* key){
    int16_t value = 0;
    if(!_started || !key){
        return value;
    }
    esp_err_t err = nvs_get_i16(_handle, key, &value);
    if(err){
        log_e("nvs_get_i16 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

uint16_t NVSClass::readUShort(const char* key){
    uint16_t value = 0;
    if(!_started || !key){
        return value;
    }
    esp_err_t err = nvs_get_u16(_handle, key, &value);
    if(err){
        log_e("nvs_get_u16 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

int32_t NVSClass::readInt(const char* key){
    int32_t value = 0;
    if(!_started || !key){
        return value;
    }
    esp_err_t err = nvs_get_i32(_handle, key, &value);
    if(err){
        log_e("nvs_get_i32 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

uint32_t NVSClass::readUInt(const char* key){
    uint32_t value = 0;
    if(!_started || !key){
        return value;
    }
    esp_err_t err = nvs_get_u32(_handle, key, &value);
    if(err){
        log_e("nvs_get_u32 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

int64_t NVSClass::readLong(const char* key){
    int64_t value = 0;
    if(!_started || !key){
        return value;
    }
    esp_err_t err = nvs_get_i64(_handle, key, &value);
    if(err){
        log_e("nvs_get_i64 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

uint64_t NVSClass::readULong(const char* key){
    uint64_t value = 0;
    if(!_started || !key){
        return value;
    }
    esp_err_t err = nvs_get_u64(_handle, key, &value);
    if(err){
        log_e("nvs_get_u64 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

String NVSClass::readString(const char* key){
    char * value = NULL;
    size_t len = 0;
    if(!_started || !key){
        return String();
    }
    esp_err_t err = nvs_get_str(_handle, key, value, &len);
    if(err){
        log_e("nvs_get_str len fail: %s %s", key, nvs_error(err));
        return String();
    }
    char buf[len];
    value = buf;
    err = nvs_get_str(_handle, key, value, &len);
    if(err){
        log_e("nvs_get_str fail: %s %s", key, nvs_error(err));
        return String();
    }
    return String(buf);
}

size_t NVSClass::readBytes(const char* key, void * buf, size_t maxLen){
    size_t len = 0;
    if(!_started || !key || !buf || !maxLen){
        return 0;
    }
    esp_err_t err = nvs_get_blob(_handle, key, NULL, &len);
    if(err){
        log_e("nvs_get_blob len fail: %s %s", key, nvs_error(err));
        return 0;
    }
    if(len > maxLen){
        log_e("not enough space in buffer: %u < %u", maxLen, len);
        return 0;
    }
    err = nvs_get_blob(_handle, key, buf, &len);
    if(err){
        log_e("nvs_get_blob fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return len;
}

