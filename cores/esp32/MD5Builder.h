/* 
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the esp8266 core for Arduino environment.
 
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef __ESP8266_MD5_BUILDER__
#define __ESP8266_MD5_BUILDER__

#include <WString.h>
#include <Stream.h>

#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/rom/md5_hash.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/md5_hash.h"
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/md5_hash.h"
#endif

class MD5Builder
{
private:
    struct MD5Context _ctx;
    uint8_t _buf[16];
public:
    void begin(void);
    void add(uint8_t * data, uint16_t len);
    void add(const char * data)
    {
        add((uint8_t*)data, strlen(data));
    }
    void add(char * data)
    {
        add((const char*)data);
    }
    void add(String data)
    {
        add(data.c_str());
    }
    void addHexString(const char * data);
    void addHexString(char * data)
    {
        addHexString((const char*)data);
    }
    void addHexString(String data)
    {
        addHexString(data.c_str());
    }
    bool addStream(Stream & stream, const size_t maxLen);
    void calculate(void);
    void getBytes(uint8_t * output);
    void getChars(char * output);
    String toString(void);
};


#endif
