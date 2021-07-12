/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef ADAFRUIT_USBD_CDC_H_
#define ADAFRUIT_USBD_CDC_H_

#include "Adafruit_TinyUSB_API.h"

#ifdef __cplusplus

#include "Adafruit_USBD_Interface.h"
#include "Stream.h"

class Adafruit_USBD_CDC : public Stream, public Adafruit_USBD_Interface {
public:
  Adafruit_USBD_CDC(void);

  static uint8_t getInstanceCount(void);

  // fron Adafruit_USBD_Interface
  virtual uint16_t getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf,
                                          uint16_t bufsize);

  void setPins(uint8_t pin_rx, uint8_t pin_tx) {
    (void)pin_rx;
    (void)pin_tx;
  }
  void begin(uint32_t baud);
  void begin(uint32_t baud, uint8_t config);
  void end(void);

  // return line coding set by host
  uint32_t baud(void);
  uint8_t stopbits(void);
  uint8_t paritytype(void);
  uint8_t numbits(void);
  int dtr(void);

  // Stream API
  virtual int available(void);
  virtual int peek(void);
  virtual int read(void);
  virtual void flush(void);
  virtual size_t write(uint8_t);

  virtual size_t write(const uint8_t *buffer, size_t size);
  size_t write(const char *buffer, size_t size) {
    return write((const uint8_t *)buffer, size);
  }

  virtual int availableForWrite(void);
  using Print::write; // pull in write(str) from Print
  operator bool();

private:
  enum { INVALID_INSTANCE = 0xffu };
  static uint8_t _instance_count;

  uint8_t _instance;

  bool isValid(void) { return _instance != INVALID_INSTANCE; }
};

// "Serial" is used with TinyUSB CDC
#if defined(USE_TINYUSB) &&                                                    \
    !(defined(ARDUINO_ARCH_ESP32) && ARDUINO_SERIAL_PORT == 0)
extern Adafruit_USBD_CDC Serial;
#define SerialTinyUSB Serial
#endif

// Serial is probably used with HW Uart
#ifndef SerialTinyUSB
extern Adafruit_USBD_CDC SerialTinyUSB;
#endif

#endif // __cplusplus
#endif
