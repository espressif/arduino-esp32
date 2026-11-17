/**
 StreamString.cpp

 Copyright (c) 2015 Markus Sattler. All rights reserved.
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
#include "StreamString.h"

size_t StreamString::write(const uint8_t *data, size_t size) {
  if (size && data) {
    const unsigned int newlen = length() + size;
    if (reserve(newlen + 1)) {
      memcpy((void *)(wbuffer() + len()), (const void *)data, size);
      setLen(newlen);
      *(wbuffer() + newlen) = 0x00;  // add null for string end
      return size;
    }
  }
  return 0;
}

size_t StreamString::write(uint8_t data) {
  return concat((char)data);
}

int StreamString::available() {
  return length();
}

int StreamString::read() {
  if (length()) {
    char c = charAt(0);
    remove(0, 1);
    return c;
  }
  return -1;
}

size_t StreamString::readBytes(char *buffer, size_t buffLen) {
  if (!buffLen || !buffer) {
    return 0;
  }
  uint32_t start = millis();
  size_t count = 0;
  while (count < buffLen) {
    const unsigned int len = length();
    if (len) {
      const size_t available = min(buffLen - count, (size_t)len);
      memcpy(buffer + count, c_str(), available);
      remove(0, available);
      count += available;
      // Match Stream::readBytes(): the timeout applies to waiting for the
      // next byte, not to the whole call.
      start = millis();
      continue;
    }
    if (_timeout == 0 || millis() - start >= _timeout) {
      break;
    }
  }
  return count;
}

int StreamString::peek() {
  if (length()) {
    char c = charAt(0);
    return c;
  }
  return -1;
}

size_t StreamString::readBytesUntil(char terminator, char *buffer, size_t size) {
  if (!buffer || !size) {
    return 0;
  }
  size_t count = 0;
  while (count < size) {
    const unsigned int available = length();
    if (available) {
      const char *data = c_str();
      const size_t remaining = size - count;
      const size_t chunk = min((size_t)available, remaining);
      const char *end = (const char *)memchr(data, terminator, chunk);
      const size_t take = end ? (size_t)(end - data) : chunk;
      if (take) {
        memcpy(buffer + count, data, take);
        count += take;
      }
      if (end && take < remaining) {
        // The terminator was read before the buffer limit: consume it too.
        remove(0, take + 1);
        break;
      }
      remove(0, take);
      if (take == remaining) {
        // Buffer limit reached: leave the terminator (if any) in the stream.
        break;
      }
      continue;
    }
    const int c = timedRead();
    if (c < 0) {
      break;
    }
    if ((char)c == terminator) {
      break;
    }
    buffer[count++] = (char)c;
  }
  return count;
}

String StreamString::readString() {
  String result;
  while (true) {
    const unsigned int available = length();
    if (available) {
      if (!result.concat(c_str(), available)) {
        // Do not consume data if the result could not be extended.
        break;
      }
      remove(0, available);
      continue;
    }
    const int c = timedRead();
    if (c < 0) {
      break;
    }
    result += (char)c;
  }
  return result;
}

String StreamString::readStringUntil(char terminator) {
  String result;
  while (true) {
    const unsigned int available = length();
    if (available) {
      const char *data = c_str();
      const char *end = (const char *)memchr(data, terminator, available);
      const size_t take = end ? (size_t)(end - data) : (size_t)available;
      if (take) {
        if (!result.concat(data, take)) {
          // Do not consume data if the result could not be extended.
          break;
        }
      }
      if (end) {
        remove(0, take + 1);
        break;
      }
      remove(0, take);
      continue;
    }
    const int c = timedRead();
    if (c < 0) {
      break;
    }
    if ((char)c == terminator) {
      break;
    }
    result += (char)c;
  }
  return result;
}

void StreamString::flush() {}
