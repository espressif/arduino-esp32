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

#ifndef _ESP32NVS_H_
#define _ESP32NVS_H_

#include "Arduino.h"

class NVSClass {
    protected:
        uint32_t _handle;
        bool _started;
        bool _readOnly;
    public:
        NVSClass();
        ~NVSClass();

        bool begin(const char * name, bool readOnly=false);
        void end();

        bool clear();
        bool remove(const char * key);

        size_t setChar(const char* key, int8_t value);
        size_t setUChar(const char* key, uint8_t value);
        size_t setShort(const char* key, int16_t value);
        size_t setUShort(const char* key, uint16_t value);
        size_t setInt(const char* key, int32_t value);
        size_t setUInt(const char* key, uint32_t value);
        size_t setLong(const char* key, int64_t value);
        size_t setULong(const char* key, uint64_t value);
        size_t setString(const char* key, const char* value);
        size_t setString(const char* key, String value);
        size_t setBytes(const char* key, const void* value, size_t len);

        int8_t getChar(const char* key, int8_t defaultValue = 0);
        uint8_t getUChar(const char* key, uint8_t defaultValue = 0);
        int16_t getShort(const char* key, int16_t defaultValue = 0);
        uint16_t getUShort(const char* key, uint16_t defaultValue = 0);
        int32_t getInt(const char* key, int32_t defaultValue = 0);
        uint32_t getUInt(const char* key, uint32_t defaultValue = 0);
        int64_t getLong(const char* key, int64_t defaultValue = 0);
        uint64_t getULong(const char* key, uint64_t defaultValue = 0);
        size_t getString(const char* key, char* value, size_t maxLen);
        String getString(const char* key, String defaultValue = String());
        size_t getBytes(const char* key, void * buf, size_t maxLen);
};

#endif
