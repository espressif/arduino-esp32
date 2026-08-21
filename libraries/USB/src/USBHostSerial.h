// Copyright 2015-2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED

#include "sdkconfig.h"
#if CONFIG_TINYUSB_ENABLED

#include "tusb_config.h"
#if CFG_TUH_ENABLED && CFG_TUH_CDC

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "Stream.h"

/**
 * USB Host CDC serial — Stream API like device USBCDC / USBSerial.
 * Needs USBHost.begin() and USBHost.task() in loop().
 * CFG_TUH_CDC sets the host CDC pool size (prebuilt often 1).
 */
class USBHostSerialClass : public Stream {
public:
  USBHostSerialClass();
  ~USBHostSerialClass();

  /** Arduino-side mount cache (loop-safe; does not call TinyUSB). */
  bool mounted() const;

  operator bool() const {
    return mounted();
  }

  /** Default line coding for next attach (0 = skip auto SET_LINE_CODING). */
  void begin(unsigned long baud = 0);
  void end();

  void setTxTimeoutMs(uint32_t timeout) {
    _tx_timeout_ms = timeout;
  }
  uint32_t getTxTimeoutMs() const {
    return _tx_timeout_ms;
  }

  int available() override;
  int availableForWrite() override;
  int peek() override;
  int read() override;
  size_t read(uint8_t *buffer, size_t size);
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  void flush() override;

  inline size_t read(char *buffer, size_t size) {
    return read((uint8_t *)buffer, size);
  }
  inline size_t write(const char *buffer, size_t size) {
    return write((const uint8_t *)buffer, size);
  }
  inline size_t write(const char *s) {
    return write((const uint8_t *)s, strlen(s));
  }
  inline size_t write(unsigned long n) {
    return write((uint8_t)n);
  }
  inline size_t write(long n) {
    return write((uint8_t)n);
  }
  inline size_t write(unsigned int n) {
    return write((uint8_t)n);
  }
  inline size_t write(int n) {
    return write((uint8_t)n);
  }

  uint32_t baudRate();

  uint8_t devAddr() const {
    return _dev_addr;
  }
  uint8_t interfaceNumber() const {
    return _itf_num;
  }
  uint8_t cdcIndex() const {
    return _cdc_idx;
  }

  /* Prefer bulk read() over Stream's timed byte loop. */
  size_t readBytes(char *buffer, size_t length) override {
    return read((uint8_t *)buffer, length);
  }
  size_t readBytes(uint8_t *buffer, size_t length) override {
    return read(buffer, length);
  }

  bool setLineCoding(uint32_t baudrate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits);
  bool setControlLineState(uint16_t line_state);
  bool connectDefault();
  bool disconnectControl();
  bool readClear();

  /** @internal TinyUSB CDC mount / umount. */
  void onCdcMount(uint8_t idx);
  void onCdcUnmount(uint8_t idx);

private:
  void applyBeginLineCodingIfNeeded();
  static void ensureTxMutex();

  volatile bool _mounted;
  bool _binding_valid;
  uint8_t _cdc_idx;
  uint8_t _dev_addr;
  uint8_t _itf_num;
  unsigned long _begin_baud;
  uint8_t _begin_stop_bits;
  uint8_t _begin_parity;
  uint8_t _begin_data_bits;
  uint32_t _tx_timeout_ms;
};

extern USBHostSerialClass USBHostSerial;

#endif /* CFG_TUH_ENABLED && CFG_TUH_CDC */
#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
