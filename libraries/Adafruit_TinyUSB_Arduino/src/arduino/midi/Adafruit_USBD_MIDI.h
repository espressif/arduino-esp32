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

#ifndef ADAFRUIT_USBD_MIDI_H_
#define ADAFRUIT_USBD_MIDI_H_

#include "Stream.h"
#include "arduino/Adafruit_USBD_Device.h"

class Adafruit_USBD_MIDI : public Stream, public Adafruit_USBD_Interface {
public:
  Adafruit_USBD_MIDI(void);

  bool begin(void);

  // for MIDI library
  bool begin(uint32_t baud) {
    (void)baud;
    return begin();
  }

  // Stream interface to use with MIDI Library
  virtual int read(void);
  virtual size_t write(uint8_t b);
  virtual int available(void);
  virtual int peek(void);
  virtual void flush(void);

  using Stream::write;

  // Raw MIDI USB packet interface.
  bool writePacket(const uint8_t packet[4]);
  bool readPacket(uint8_t packet[4]);

  // from Adafruit_USBD_Interface
  virtual uint16_t getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf,
                                          uint16_t bufsize);

  void setCables(uint8_t n_cables);

private:
  uint8_t _n_cables;
};

#endif /* ADAFRUIT_USBD_MIDI_H_ */
