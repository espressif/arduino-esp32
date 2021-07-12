/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 hathach for Adafruit Industries
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

#ifndef ADAFRUIT_USBD_WEBUSB_H_
#define ADAFRUIT_USBD_WEBUSB_H_

#include "Stream.h"
#include "arduino/Adafruit_USBD_Device.h"

#define WEBUSB_URL_DEF(_name, _scheme, _url)                                   \
  struct TU_ATTR_PACKED {                                                      \
    uint8_t bLength;                                                           \
    uint8_t bDescriptorType;                                                   \
    uint8_t bScheme;                                                           \
    char url[3 + sizeof(_url)];                                                \
  } const _name = {3 + sizeof(_url) - 1, 3, _scheme, _url}

class Adafruit_USBD_WebUSB : public Stream, public Adafruit_USBD_Interface {
public:
  typedef void (*linestate_callback_t)(bool connected);
  Adafruit_USBD_WebUSB(void);

  bool begin(void);

  bool setLandingPage(const void *url);
  void setLineStateCallback(linestate_callback_t fp);

  // Stream interface to use with MIDI Library
  virtual int read(void);
  virtual int available(void);
  virtual int peek(void);
  virtual void flush(void);
  virtual size_t write(uint8_t b);

  virtual size_t write(const uint8_t *buffer, size_t size);
  size_t write(const char *buffer, size_t size) {
    return write((const uint8_t *)buffer, size);
  }

  bool connected(void);
  operator bool();

  // from Adafruit_USBD_Interface
  virtual uint16_t getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf,
                                          uint16_t bufsize);

private:
  bool _connected;
  const uint8_t *_url;
  linestate_callback_t _linestate_cb;

  // Make all tinyusb callback friend to access private data
  friend bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                         tusb_control_request_t const *request);
};

#endif /* ADAFRUIT_USBD_WEBUSB_H_ */
