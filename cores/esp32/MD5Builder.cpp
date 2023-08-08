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
#include <Arduino.h>
#include <MD5Builder.h>

static uint8_t hex_char_to_byte(uint8_t c)
{
    return  (c >= 'a' && c <= 'f') ? (c - ((uint8_t)'a' - 0xa)) :
            (c >= 'A' && c <= 'F') ? (c - ((uint8_t)'A' - 0xA)) :
            (c >= '0' &&  c<= '9') ? (c - (uint8_t)'0') : 0;
}

void MD5Builder::begin(void)
{
    memset(_buf, 0x00, ESP_ROM_MD5_DIGEST_LEN);
    esp_rom_md5_init(&_ctx);
}

void MD5Builder::add(uint8_t * data, uint16_t len)
{
    esp_rom_md5_update(&_ctx, data, len);
}

void MD5Builder::addHexString(const char * data)
{
    uint16_t i, len = strlen(data);
    uint8_t * tmp = (uint8_t*)malloc(len/2);
    if(tmp == NULL) {
        return;
    }
    for(i=0; i<len; i+=2) {
        uint8_t high = hex_char_to_byte(data[i]);
        uint8_t low = hex_char_to_byte(data[i+1]);
        tmp[i/2] = (high & 0x0F) << 4 | (low & 0x0F);
    }
    add(tmp, len/2);
    free(tmp);
}

bool MD5Builder::addStream(Stream & stream, const size_t maxLen)
{
    const int buf_size = 512;
    int maxLengthLeft = maxLen;
    uint8_t * buf = (uint8_t*) malloc(buf_size);

    if(!buf) {
        return false;
    }

    int bytesAvailable = stream.available();
    while((bytesAvailable > 0) && (maxLengthLeft > 0)) {

        // determine number of bytes to read
        int readBytes = bytesAvailable;
        if(readBytes > maxLengthLeft) {
            readBytes = maxLengthLeft ;    // read only until max_len
        }
        if(readBytes > buf_size) {
            readBytes = buf_size;    // not read more the buffer can handle
        }

        // read data and check if we got something
        int numBytesRead = stream.readBytes(buf, readBytes);
        if(numBytesRead< 1) {
            free(buf);
            return false;
        }

        // Update MD5 with buffer payload
        esp_rom_md5_update(&_ctx, buf, numBytesRead);

        // update available number of bytes
        maxLengthLeft -= numBytesRead;
        bytesAvailable = stream.available();
    }
    free(buf);
    return true;
}

void MD5Builder::calculate(void)
{
    esp_rom_md5_final(_buf, &_ctx);
}

void MD5Builder::getBytes(uint8_t * output)
{
    memcpy(output, _buf, ESP_ROM_MD5_DIGEST_LEN);
}

void MD5Builder::getChars(char * output)
{
    for(uint8_t i = 0; i < ESP_ROM_MD5_DIGEST_LEN; i++) {
        sprintf(output + (i * 2), "%02x", _buf[i]);
    }
}

String MD5Builder::toString(void)
{
    char out[(ESP_ROM_MD5_DIGEST_LEN * 2) + 1];
    getChars(out);
    return String(out);
}
