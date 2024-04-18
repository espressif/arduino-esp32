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
#include <HEXBuilder.h>
#include <MD5Builder.h>

void MD5Builder::begin(void) {
  memset(_buf, 0x00, ESP_ROM_MD5_DIGEST_LEN);
  esp_rom_md5_init(&_ctx);
}

void MD5Builder::add(const uint8_t* data, size_t len) {
  esp_rom_md5_update(&_ctx, data, len);
}

void MD5Builder::addHexString(const char* data) {
  size_t len = strlen(data);
  uint8_t* tmp = (uint8_t*)malloc(len / 2);
  if (tmp == NULL) {
    return;
  }
  hex2bytes(tmp, len / 2, data);
  add(tmp, len / 2);
  free(tmp);
}

bool MD5Builder::addStream(Stream& stream, const size_t maxLen) {
  const int buf_size = 512;
  int maxLengthLeft = maxLen;
  uint8_t* buf = (uint8_t*)malloc(buf_size);

  if (!buf) {
    return false;
  }

  int bytesAvailable = stream.available();
  while ((bytesAvailable > 0) && (maxLengthLeft > 0)) {

    // determine number of bytes to read
    int readBytes = bytesAvailable;
    if (readBytes > maxLengthLeft) {
      readBytes = maxLengthLeft;  // read only until max_len
    }
    if (readBytes > buf_size) {
      readBytes = buf_size;  // not read more the buffer can handle
    }

    // read data and check if we got something
    int numBytesRead = stream.readBytes(buf, readBytes);
    if (numBytesRead < 1) {
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

void MD5Builder::calculate(void) {
  esp_rom_md5_final(_buf, &_ctx);
}

void MD5Builder::getBytes(uint8_t* output) {
  memcpy(output, _buf, ESP_ROM_MD5_DIGEST_LEN);
}

void MD5Builder::getChars(char* output) {
  bytes2hex(output, ESP_ROM_MD5_DIGEST_LEN * 2 + 1, _buf, ESP_ROM_MD5_DIGEST_LEN);
}

String MD5Builder::toString(void) {
  char out[(ESP_ROM_MD5_DIGEST_LEN * 2) + 1];
  getChars(out);
  return String(out);
}
