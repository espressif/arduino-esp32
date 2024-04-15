/*
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the esp32 core for Arduino environment.

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

static uint8_t hex_char_to_byte(uint8_t c) {
  return (c >= 'a' && c <= 'f') ? (c - ((uint8_t)'a' - 0xa)) : (c >= 'A' && c <= 'F') ? (c - ((uint8_t)'A' - 0xA))
                                                             : (c >= '0' && c <= '9') ? (c - (uint8_t)'0')
                                                                                      : 0x10;  // unknown char is 16
}

size_t HEXBuilder::hex2bytes(unsigned char *out, size_t maxlen, String &in) {
  return hex2bytes(out, maxlen, in.c_str());
}

size_t HEXBuilder::hex2bytes(unsigned char *out, size_t maxlen, const char *in) {
  size_t len = 0;
  for (; *in; in++) {
    uint8_t c = hex_char_to_byte(*in);
    // Silently skip anything unknown.
    if (c > 15)
      continue;

    if (len & 1) {
      if (len / 2 < maxlen)
        out[len / 2] |= c;
    } else {
      if (len / 2 < maxlen)
        out[len / 2] = c << 4;
    }
    len++;
  }
  return (len + 1) / 2;
}

size_t HEXBuilder::bytes2hex(char *out, size_t maxlen, const unsigned char *in, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (i * 2 + 1 < maxlen) {
      sprintf(out + (i * 2), "%02x", in[i]);
    }
  }
  return len * 2 + 1;
}

String HEXBuilder::bytes2hex(const unsigned char *in, size_t len) {
  size_t maxlen = len * 2 + 1;
  char *out = (char *)malloc(maxlen);
  if (!out) return String();
  bytes2hex(out, maxlen, in, len);
  String ret = String(out);
  free(out);
  return ret;
}
