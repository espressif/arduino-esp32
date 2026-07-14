/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
 * Copyright 2017 Neil Kolban
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "BTAddress.h"
#include "HEXBuilder.h"
#include <stdio.h>

BTAddress::BTAddress() {
  memset(_addr, 0, sizeof(_addr));
  _type = Public;
}

BTAddress::BTAddress(const uint8_t addr[6], Type type) : _type(type) {
  memcpy(_addr, addr, 6);
}

BTAddress::BTAddress(const char *str) {
  memset(_addr, 0, sizeof(_addr));
  _type = Public;
  if (str) {
    parseString(str);
  }
}

BTAddress::BTAddress(const String &str) {
  memset(_addr, 0, sizeof(_addr));
  _type = Public;
  parseString(str.c_str());
}

void BTAddress::parseString(const char *str) {
  if (!str) {
    return;
  }
  // Accept "AA:BB:CC:DD:EE:FF" (17 chars) or "AABBCCDDEEFF" (12 chars). Both are
  // MSB-first in the string; we store LSB-first. HEXBuilder::hex2bytes parses the
  // hex nibbles and silently skips the ':' separators, so one call covers both
  // forms and reuses the core hex parser instead of pulling in the ~9 KB newlib
  // scanf engine. A length that is neither, or any non-hex junk, yields a byte
  // count other than 6 and is rejected.
  size_t len = strlen(str);
  if (len != 17 && len != 12) {
    return;
  }
  uint8_t msb[6];
  if (HEXBuilder::hex2bytes(msb, sizeof(msb), str) != 6) {
    return;
  }
  for (int i = 0; i < 6; i++) {
    _addr[i] = msb[5 - i];
  }
}

bool BTAddress::operator==(const BTAddress &other) const {
  return _type == other._type && memcmp(_addr, other._addr, 6) == 0;
}

bool BTAddress::operator!=(const BTAddress &other) const {
  return !(*this == other);
}

bool BTAddress::operator<(const BTAddress &other) const {
  int cmp = memcmp(_addr, other._addr, 6);
  if (cmp != 0) {
    return cmp < 0;
  }
  return _type < other._type;
}

BTAddress::operator bool() const {
  return !isZero();
}

const uint8_t *BTAddress::data() const {
  return _addr;
}

void BTAddress::toEspBdAddr(uint8_t out[6]) const {
  memcpy(out, _addr, 6);
}

BTAddress::Type BTAddress::type() const {
  return _type;
}

String BTAddress::toString(bool uppercase) const {
  char buf[18];
  const char *fmt = uppercase ? "%02X:%02X:%02X:%02X:%02X:%02X" : "%02x:%02x:%02x:%02x:%02x:%02x";
  snprintf(buf, sizeof(buf), fmt, _addr[5], _addr[4], _addr[3], _addr[2], _addr[1], _addr[0]);
  return String(buf);
}

bool BTAddress::isZero() const {
  static const uint8_t zero[6] = {};
  return memcmp(_addr, zero, 6) == 0;
}
