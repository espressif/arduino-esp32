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

        bool erase();
        bool erase(const char * key);

        size_t writeChar(const char* key, int8_t value);
        size_t writeUChar(const char* key, uint8_t value);
        size_t writeShort(const char* key, int16_t value);
        size_t writeUShort(const char* key, uint16_t value);
        size_t writeInt(const char* key, int32_t value);
        size_t writeUInt(const char* key, uint32_t value);
        size_t writeLong(const char* key, int64_t value);
        size_t writeULong(const char* key, uint64_t value);
        size_t writeString(const char* key, const char* value);
        size_t writeBytes(const char* key, const void* value, size_t len);

        int8_t readChar(const char* key);
        uint8_t readUChar(const char* key);
        int16_t readShort(const char* key);
        uint16_t readUShort(const char* key);
        int32_t readInt(const char* key);
        uint32_t readUInt(const char* key);
        int64_t readLong(const char* key);
        uint64_t readULong(const char* key);
        String readString(const char* key);
        size_t readBytes(const char* key, void * buf, size_t maxLen);
};

#endif
