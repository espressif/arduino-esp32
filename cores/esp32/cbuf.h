/*
 cbuf.h - Circular buffer implementation
 Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"

class cbuf {
public:
  cbuf(size_t size);
  ~cbuf();

  size_t resizeAdd(size_t addSize);
  size_t resize(size_t newSize);

  size_t available() const;
  size_t size();
  size_t room() const;
  bool empty() const;
  bool full() const;

  int peek();

  int read();
  size_t read(char* dst, size_t size);

  size_t write(char c);
  size_t write(const char* src, size_t size);

  void flush();
  size_t remove(size_t size);

  cbuf* next;
  bool has_peek;
  uint8_t peek_byte;

protected:
  RingbufHandle_t _buf = NULL;
#if !CONFIG_DISABLE_HAL_LOCKS
  SemaphoreHandle_t _lock = NULL;
#endif
};
