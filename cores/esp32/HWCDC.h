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
#pragma once

#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32C3

#include <inttypes.h>
#include "Stream.h"

class HWCDC: public Stream
{
public:
    HWCDC();
    ~HWCDC();

    size_t setRxBufferSize(size_t);
    size_t setTxBufferSize(size_t);
    void begin(unsigned long baud=0);
    void end();
    
    int available(void);
    int availableForWrite(void);
    int peek(void);
    int read(void);
    size_t read(uint8_t *buffer, size_t size);
    size_t write(uint8_t);
    size_t write(const uint8_t *buffer, size_t size);
    void flush(void);
    
    inline size_t read(char * buffer, size_t size)
    {
        return read((uint8_t*) buffer, size);
    }
    inline size_t write(const char * buffer, size_t size)
    {
        return write((uint8_t*) buffer, size);
    }
    inline size_t write(const char * s)
    {
        return write((uint8_t*) s, strlen(s));
    }
    inline size_t write(unsigned long n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(long n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(unsigned int n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(int n)
    {
        return write((uint8_t) n);
    }
    operator bool() const;
    void setDebugOutput(bool);
    uint32_t baudRate(){return 115200;}

};

#if ARDUINO_HW_CDC_ON_BOOT //Serial used for USB CDC
extern HWCDC Serial;
#else
extern HWCDC USBSerial;
#endif

#endif /* CONFIG_IDF_TARGET_ESP32C3 */
