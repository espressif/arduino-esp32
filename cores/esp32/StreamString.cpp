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
    if(size && data) {
        const unsigned int newlen = length() + size;
        if(reserve(newlen + 1)) {
            memcpy((void *) (wbuffer() + len()), (const void *) data, size);
            setLen(newlen);
            *(wbuffer() + newlen) = 0x00; // add null for string end
            return size;
        }
    }
    return 0;
}

size_t StreamString::write(uint8_t data) {
    return concat((char) data);
}

int StreamString::available() {
    return length();
}

int StreamString::read() {
    if(length()) {
        char c = charAt(0);
        remove(0, 1);
        return c;

    }
    return -1;
}

int StreamString::peek() {
    if(length()) {
        char c = charAt(0);
        return c;
    }
    return -1;
}

void StreamString::flush() {
}

