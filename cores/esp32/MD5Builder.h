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

  Modified 10 Jan 2024 by Lucas Saavedra Vaz (Use abstract class HashBuilder)
*/
#ifndef MD5Builder_h
#define MD5Builder_h

#include <WString.h>
#include <Stream.h>

#include "esp_system.h"
#include "esp_rom_md5.h"

#include "HashBuilder.h"

class MD5Builder : public HashBuilder {
private:
  md5_context_t _ctx;
  uint8_t _buf[ESP_ROM_MD5_DIGEST_LEN];
public:
  void begin(void) override;

  using HashBuilder::add;
  void add(const uint8_t* data, size_t len) override;

  using HashBuilder::addHexString;
  void addHexString(const char* data) override;

  bool addStream(Stream& stream, const size_t maxLen) override;
  void calculate(void) override;
  void getBytes(uint8_t* output) override;
  void getChars(char* output) override;
  String toString(void) override;
};

#endif
