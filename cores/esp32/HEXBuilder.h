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

#ifndef HEXBuilder_h
#define HEXBuilder_h

#include <WString.h>
#include <Stream.h>

class HEXBuilder {
public:
  static size_t hex2bytes(unsigned char* out, size_t maxlen, String& in);
  static size_t hex2bytes(unsigned char* out, size_t maxlen, const char* in);

  static String bytes2hex(const unsigned char* in, size_t len);
  static size_t bytes2hex(char* out, size_t maxlen, const unsigned char* in, size_t len);
};
#endif
