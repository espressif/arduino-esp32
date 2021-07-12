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

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUD_CDC

#include "Arduino.h"

#include "Adafruit_TinyUSB_API.h"

#include "Adafruit_USBD_CDC.h"
#include "Adafruit_USBD_Device.h"

// Starting endpoints; adjusted elsewhere as needed
#define EPOUT 0x00
#define EPIN 0x80

// SerialTinyUSB can be macro expanding to "Serial" on supported cores
Adafruit_USBD_CDC SerialTinyUSB;

//------------- Static member -------------//
uint8_t Adafruit_USBD_CDC::_instance_count = 0;

uint8_t Adafruit_USBD_CDC::getInstanceCount(void) { return _instance_count; }

Adafruit_USBD_CDC::Adafruit_USBD_CDC(void) { _instance = INVALID_INSTANCE; }

uint16_t Adafruit_USBD_CDC::getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf,
                                                   uint16_t bufsize) {
  // CDC is mostly always existed for DFU
  // usb core will automatically update endpoint number
  uint8_t desc[] = {TUD_CDC_DESCRIPTOR(itfnum, 0, EPIN, 8, EPOUT, EPIN, 64)};
  uint16_t const len = sizeof(desc);

  if (bufsize < len) {
    return 0;
  }

  memcpy(buf, desc, len);
  return len;
}

// Baud and config is ignore in CDC
void Adafruit_USBD_CDC::begin(uint32_t baud) {
  (void)baud;

  // already called begin()
  if (isValid()) {
    return;
  }

  // too many instances
  if (!(_instance_count < CFG_TUD_CDC)) {
    return;
  }

  _instance = _instance_count++;
  this->setStringDescriptor("TinyUSB Serial");
  USBDevice.addInterface(*this);
}

void Adafruit_USBD_CDC::begin(uint32_t baud, uint8_t config) {
  (void)config;
  this->begin(baud);
}

void Adafruit_USBD_CDC::end(void) {
  // Reset configuration descriptor without Serial as CDC
  USBDevice.clearConfiguration();
  _instance_count = 0;
}

uint32_t Adafruit_USBD_CDC::baud(void) {
  if (!isValid()) {
    return 0;
  }

  cdc_line_coding_t coding;
  tud_cdc_n_get_line_coding(_instance, &coding);

  return coding.bit_rate;
}

uint8_t Adafruit_USBD_CDC::stopbits(void) {
  if (!isValid()) {
    return 0;
  }

  cdc_line_coding_t coding;
  tud_cdc_n_get_line_coding(_instance, &coding);

  return coding.stop_bits;
}

uint8_t Adafruit_USBD_CDC::paritytype(void) {
  if (!isValid()) {
    return 0;
  }

  cdc_line_coding_t coding;
  tud_cdc_n_get_line_coding(_instance, &coding);

  return coding.parity;
}

uint8_t Adafruit_USBD_CDC::numbits(void) {
  if (!isValid()) {
    return 0;
  }

  cdc_line_coding_t coding;
  tud_cdc_n_get_line_coding(_instance, &coding);

  return coding.data_bits;
}

int Adafruit_USBD_CDC::dtr(void) {
  if (!isValid()) {
    return 0;
  }

  return tud_cdc_n_connected(_instance);
}

Adafruit_USBD_CDC::operator bool() {
  if (!isValid()) {
    return false;
  }

  bool ret = tud_cdc_n_connected(_instance);

  // Add an yield to run usb background in case sketch block wait as follows
  // while( !Serial ) {}
  if (!ret) {
    yield();
  }
  return ret;
}

int Adafruit_USBD_CDC::available(void) {
  if (!isValid()) {
    return 0;
  }

  uint32_t count = tud_cdc_n_available(_instance);

  // Add an yield to run usb background in case sketch block wait as follows
  // while( !Serial.available() ) {}
  if (!count) {
    yield();
  }

  return count;
}

int Adafruit_USBD_CDC::peek(void) {
  if (!isValid()) {
    return -1;
  }

  uint8_t ch;
  return tud_cdc_n_peek(_instance, &ch) ? (int)ch : -1;
}

int Adafruit_USBD_CDC::read(void) {
  if (!isValid()) {
    return -1;
  }
  return (int)tud_cdc_n_read_char(_instance);
}

void Adafruit_USBD_CDC::flush(void) {
  if (!isValid()) {
    return;
  }

  tud_cdc_n_write_flush(_instance);
}

size_t Adafruit_USBD_CDC::write(uint8_t ch) { return write(&ch, 1); }

size_t Adafruit_USBD_CDC::write(const uint8_t *buffer, size_t size) {
  if (!isValid()) {
    return 0;
  }

  size_t remain = size;
  while (remain && tud_cdc_n_connected(_instance)) {
    size_t wrcount = tud_cdc_n_write(_instance, buffer, remain);
    remain -= wrcount;
    buffer += wrcount;

    // Write FIFO is full, run usb background to flush
    if (remain) {
      yield();
    }
  }

  return size - remain;
}

int Adafruit_USBD_CDC::availableForWrite(void) {
  if (!isValid()) {
    return 0;
  }
  return tud_cdc_n_write_available(_instance);
}

extern "C" {

// Invoked when cdc when line state changed e.g connected/disconnected
// Use to reset to DFU when disconnect with 1200 bps
void tud_cdc_line_state_cb(uint8_t instance, bool dtr, bool rts) {
  (void)rts;

  // DTR = false is counted as disconnected
  if (!dtr) {
    // touch1200 only with first CDC instance (Serial)
    if (instance == 0) {
      cdc_line_coding_t coding;
      tud_cdc_get_line_coding(&coding);

      if (coding.bit_rate == 1200) {
        TinyUSB_Port_EnterDFU();
      }
    }
  }
}
}

#endif // TUSB_OPT_DEVICE_ENABLED
