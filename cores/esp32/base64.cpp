/**
 * base64.cpp
 *
 * Created on: 09.12.2015
 *
 * Copyright (c) 2015 Markus Sattler. All rights reserved.
 * This file is part of the ESP31B core for Arduino.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "Arduino.h"
extern "C" {
#include "libb64/cdecode.h"
#include "libb64/cencode.h"
}
#include "base64.h"

/**
 * convert input data to base64
 * @param data const uint8_t *
 * @param length size_t
 * @return String
 */
String base64::encode(const uint8_t *data, size_t length) {
  size_t size = base64_encode_expected_len(length) + 1;
  char *buffer = (char *)malloc(size);
  if (buffer) {
    base64_encodestate _state;
    base64_init_encodestate(&_state);
    int len = base64_encode_block((const char *)&data[0], length, &buffer[0], &_state);
    len = base64_encode_blockend((buffer + len), &_state);

    String base64 = String(buffer);
    free(buffer);
    return base64;
  }
  return String("-FAIL-");
}

/**
 * convert input data to base64
 * @param text const String&
 * @return String
 */
String base64::encode(const String &text) {
  return base64::encode((uint8_t *)text.c_str(), text.length());
}
