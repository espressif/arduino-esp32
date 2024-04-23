/*
 cbuf.cpp - Circular buffer implementation
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

#include "cbuf.h"
#include "esp32-hal-log.h"

#if CONFIG_DISABLE_HAL_LOCKS
#define CBUF_MUTEX_CREATE()
#define CBUF_MUTEX_LOCK()
#define CBUF_MUTEX_UNLOCK()
#define CBUF_MUTEX_DELETE()
#else
#define CBUF_MUTEX_CREATE()            \
  if (_lock == NULL) {                 \
    _lock = xSemaphoreCreateMutex();   \
    if (_lock == NULL) {               \
      log_e("failed to create mutex"); \
    }                                  \
  }
#define CBUF_MUTEX_LOCK()                          \
  if (_lock != NULL) {                             \
    xSemaphoreTakeRecursive(_lock, portMAX_DELAY); \
  }
#define CBUF_MUTEX_UNLOCK()         \
  if (_lock != NULL) {              \
    xSemaphoreGiveRecursive(_lock); \
  }
#define CBUF_MUTEX_DELETE()      \
  if (_lock != NULL) {           \
    SemaphoreHandle_t l = _lock; \
    _lock = NULL;                \
    vSemaphoreDelete(l);         \
  }
#endif

cbuf::cbuf(size_t size) : next(NULL), has_peek(false), peek_byte(0), _buf(xRingbufferCreate(size, RINGBUF_TYPE_BYTEBUF)) {
  if (_buf == NULL) {
    log_e("failed to allocate ring buffer");
  }
  CBUF_MUTEX_CREATE();
}

cbuf::~cbuf() {
  CBUF_MUTEX_LOCK();
  if (_buf != NULL) {
    RingbufHandle_t b = _buf;
    _buf = NULL;
    vRingbufferDelete(b);
  }
  CBUF_MUTEX_UNLOCK();
  CBUF_MUTEX_DELETE();
}

size_t cbuf::resizeAdd(size_t addSize) {
  return resize(size() + addSize);
}

size_t cbuf::resize(size_t newSize) {
  CBUF_MUTEX_LOCK();
  size_t _size = size();
  if (newSize == _size) {
    return _size;
  }

  // not lose any data
  // if data can be lost use remove or flush before resize
  size_t bytes_available = available();
  if (newSize < bytes_available) {
    CBUF_MUTEX_UNLOCK();
    log_e("new size is less than the currently available data size");
    return _size;
  }

  RingbufHandle_t newbuf = xRingbufferCreate(newSize, RINGBUF_TYPE_BYTEBUF);
  if (newbuf == NULL) {
    CBUF_MUTEX_UNLOCK();
    log_e("failed to allocate new ring buffer");
    return _size;
  }

  if (_buf != NULL) {
    if (bytes_available) {
      char *old_data = (char *)malloc(bytes_available);
      if (old_data == NULL) {
        vRingbufferDelete(newbuf);
        CBUF_MUTEX_UNLOCK();
        log_e("failed to allocate temporary buffer");
        return _size;
      }
      bytes_available = read(old_data, bytes_available);
      if (!bytes_available) {
        free(old_data);
        vRingbufferDelete(newbuf);
        CBUF_MUTEX_UNLOCK();
        log_e("failed to read previous data");
        return _size;
      }
      if (xRingbufferSend(newbuf, (void *)old_data, bytes_available, 0) != pdTRUE) {
        write(old_data, bytes_available);
        free(old_data);
        vRingbufferDelete(newbuf);
        CBUF_MUTEX_UNLOCK();
        log_e("failed to restore previous data");
        return _size;
      }
      free(old_data);
    }

    RingbufHandle_t b = _buf;
    _buf = newbuf;
    vRingbufferDelete(b);
  } else {
    _buf = newbuf;
  }
  CBUF_MUTEX_UNLOCK();
  return newSize;
}

size_t cbuf::available() const {
  size_t available = 0;
  if (_buf != NULL) {
    vRingbufferGetInfo(_buf, NULL, NULL, NULL, NULL, (UBaseType_t *)&available);
  }
  if (has_peek) {
    available++;
  }
  return available;
}

size_t cbuf::size() {
  size_t _size = 0;
  if (_buf != NULL) {
    _size = xRingbufferGetMaxItemSize(_buf);
  }
  return _size;
}

size_t cbuf::room() const {
  size_t _room = 0;
  if (_buf != NULL) {
    _room = xRingbufferGetCurFreeSize(_buf);
  }
  return _room;
}

bool cbuf::empty() const {
  return available() == 0;
}

bool cbuf::full() const {
  return room() == 0;
}

int cbuf::peek() {
  if (!available()) {
    return -1;
  }

  int c;

  CBUF_MUTEX_LOCK();
  if (has_peek) {
    c = peek_byte;
  } else {
    c = read();
    if (c >= 0) {
      has_peek = true;
      peek_byte = c;
    }
  }
  CBUF_MUTEX_UNLOCK();
  return c;
}

int cbuf::read() {
  char result = 0;
  if (!read(&result, 1)) {
    return -1;
  }
  return static_cast<int>(result);
}

size_t cbuf::read(char *dst, size_t size) {
  CBUF_MUTEX_LOCK();
  size_t bytes_available = available();
  if (!bytes_available || !size) {
    CBUF_MUTEX_UNLOCK();
    return 0;
  }

  if (has_peek) {
    if (dst != NULL) {
      *dst++ = peek_byte;
    }
    size--;
  }

  size_t size_read = 0;
  if (size) {
    size_t received_size = 0;
    size_t size_to_read = (size < bytes_available) ? size : bytes_available;
    uint8_t *received_buff = (uint8_t *)xRingbufferReceiveUpTo(_buf, &received_size, 0, size_to_read);
    if (received_buff != NULL) {
      if (dst != NULL) {
        memcpy(dst, received_buff, received_size);
      }
      vRingbufferReturnItem(_buf, received_buff);
      size_read = received_size;
      size_to_read -= received_size;
      // wrap around data
      if (size_to_read) {
        received_size = 0;
        received_buff = (uint8_t *)xRingbufferReceiveUpTo(_buf, &received_size, 0, size_to_read);
        if (received_buff != NULL) {
          if (dst != NULL) {
            memcpy(dst + size_read, received_buff, received_size);
          }
          vRingbufferReturnItem(_buf, received_buff);
          size_read += received_size;
        } else {
          log_e("failed to read wrap around data from ring buffer");
        }
      }
    } else {
      log_e("failed to read from ring buffer");
    }
  }

  if (has_peek) {
    has_peek = false;
    size_read++;
  }

  CBUF_MUTEX_UNLOCK();
  return size_read;
}

size_t cbuf::write(char c) {
  return write(&c, 1);
}

size_t cbuf::write(const char *src, size_t size) {
  CBUF_MUTEX_LOCK();
  size_t bytes_available = room();
  if (!bytes_available || !size) {
    CBUF_MUTEX_UNLOCK();
    return 0;
  }
  size_t size_to_write = (size < bytes_available) ? size : bytes_available;
  if (xRingbufferSend(_buf, (void *)src, size_to_write, 0) != pdTRUE) {
    CBUF_MUTEX_UNLOCK();
    log_e("failed to write to ring buffer");
    return 0;
  }
  CBUF_MUTEX_UNLOCK();
  return size_to_write;
}

void cbuf::flush() {
  read(NULL, available());
}

size_t cbuf::remove(size_t size) {
  CBUF_MUTEX_LOCK();
  size_t bytes_available = available();
  if (bytes_available && size) {
    size_t size_to_remove = (size < bytes_available) ? size : bytes_available;
    bytes_available -= read(NULL, size_to_remove);
  }
  CBUF_MUTEX_UNLOCK();
  return bytes_available;
}
