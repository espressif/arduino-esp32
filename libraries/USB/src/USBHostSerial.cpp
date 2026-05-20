// Copyright 2015-2025 Espressif Systems (Shanghai) PTE LTD
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

#include "USBHostSerial.h"

#if SOC_USB_OTG_SUPPORTED && CONFIG_TINYUSB_ENABLED && CFG_TUH_ENABLED && CFG_TUH_CDC

#include "USBHost.h"
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "class/cdc/cdc_host.h"
#include "host/usbh.h"

static SemaphoreHandle_t s_usb_host_serial_tx_mutex;

USBHostSerialClass::USBHostSerialClass()
  : _mounted(false)
  , _binding_valid(false)
  , _cdc_idx(0)
  , _dev_addr(0)
  , _itf_num(0)
  , _begin_baud(0)
  , _begin_stop_bits(CDC_LINE_CODING_STOP_BITS_1)
  , _begin_parity(CDC_LINE_CODING_PARITY_NONE)
  , _begin_data_bits(8)
  , _tx_timeout_ms(250) {}

USBHostSerialClass::~USBHostSerialClass() {
  end();
}

void USBHostSerialClass::ensureTxMutex() {
  if (s_usb_host_serial_tx_mutex == NULL) {
    s_usb_host_serial_tx_mutex = xSemaphoreCreateMutex();
  }
}

bool USBHostSerialClass::mounted() const {
  if (!_binding_valid) {
    return false;
  }
  if (!tuh_cdc_mounted(_cdc_idx)) {
    return false;
  }
  return tuh_cdc_itf_get_index(_dev_addr, _itf_num) == _cdc_idx;
}

void USBHostSerialClass::begin(unsigned long baud) {
  ensureTxMutex();
  _begin_baud = baud;
  _begin_stop_bits = CDC_LINE_CODING_STOP_BITS_1;
  _begin_parity = CDC_LINE_CODING_PARITY_NONE;
  _begin_data_bits = 8;
  applyBeginLineCodingIfNeeded();
}

void USBHostSerialClass::end() {
  _mounted = false;
  _binding_valid = false;
  _dev_addr = 0;
  _itf_num = 0;
  if (s_usb_host_serial_tx_mutex != NULL) {
    vSemaphoreDelete(s_usb_host_serial_tx_mutex);
    s_usb_host_serial_tx_mutex = NULL;
  }
}

void USBHostSerialClass::applyBeginLineCodingIfNeeded() {
  if (!mounted() || _begin_baud == 0) {
    return;
  }
  (void)setLineCoding(_begin_baud, _begin_stop_bits, _begin_parity, _begin_data_bits);
  (void)connectDefault();
}

uint32_t USBHostSerialClass::baudRate() {
  if (!mounted()) {
    return _begin_baud;
  }
  cdc_line_coding_t lc;
  memset(&lc, 0, sizeof(lc));
  if (!tuh_cdc_get_line_coding_local(_cdc_idx, &lc)) {
    return _begin_baud;
  }
  return lc.bit_rate;
}

int USBHostSerialClass::available() {
  if (!mounted()) {
    return 0;
  }
  uint32_t n = tuh_cdc_read_available(_cdc_idx);
  return (n > (uint32_t)INT_MAX) ? INT_MAX : (int)n;
}

int USBHostSerialClass::peek() {
  if (!mounted()) {
    return -1;
  }
  uint8_t ch = 0;
  if (tuh_cdc_peek(_cdc_idx, &ch)) {
    return (int)ch;
  }
  return -1;
}

int USBHostSerialClass::read() {
  uint8_t c = 0;
  if (read(&c, 1) == 0) {
    return -1;
  }
  return (int)c;
}

size_t USBHostSerialClass::read(uint8_t *buffer, size_t size) {
  if (!mounted() || buffer == NULL || size == 0) {
    return 0;
  }
  return (size_t)tuh_cdc_read(_cdc_idx, buffer, (uint32_t)size);
}

int USBHostSerialClass::availableForWrite() {
  if (!mounted()) {
    return 0;
  }
  ensureTxMutex();
  if (xSemaphoreTake(s_usb_host_serial_tx_mutex, pdMS_TO_TICKS(_tx_timeout_ms)) != pdPASS) {
    return 0;
  }
  uint32_t a = tuh_cdc_write_available(_cdc_idx);
  xSemaphoreGive(s_usb_host_serial_tx_mutex);
  return (a > (uint32_t)INT_MAX) ? INT_MAX : (int)a;
}

void USBHostSerialClass::flush() {
  if (!mounted()) {
    return;
  }
  ensureTxMutex();
  if (xSemaphoreTake(s_usb_host_serial_tx_mutex, pdMS_TO_TICKS(_tx_timeout_ms)) != pdPASS) {
    return;
  }
  (void)tuh_cdc_write_flush(_cdc_idx);
  xSemaphoreGive(s_usb_host_serial_tx_mutex);
}

uint32_t USBHostSerialClass::writeFlush() {
  if (!mounted()) {
    return 0;
  }
  ensureTxMutex();
  if (xSemaphoreTake(s_usb_host_serial_tx_mutex, pdMS_TO_TICKS(_tx_timeout_ms)) != pdPASS) {
    return 0;
  }
  uint32_t n = tuh_cdc_write_flush(_cdc_idx);
  xSemaphoreGive(s_usb_host_serial_tx_mutex);
  return n;
}

size_t USBHostSerialClass::write(uint8_t c) {
  return write(&c, 1);
}

size_t USBHostSerialClass::write(const uint8_t *buffer, size_t size) {
  if (!mounted() || buffer == NULL || size == 0) {
    return 0;
  }
  ensureTxMutex();
  if (xSemaphoreTake(s_usb_host_serial_tx_mutex, pdMS_TO_TICKS(_tx_timeout_ms)) != pdPASS) {
    return 0;
  }

  size_t so_far = 0;
  uint32_t deadline = millis() + _tx_timeout_ms;
  while (so_far < size) {
    if ((int32_t)(millis() - deadline) >= 0) {
      break;
    }
    uint32_t n = tuh_cdc_write(_cdc_idx, buffer + so_far, (uint32_t)(size - so_far));
    if (n) {
      so_far += n;
      (void)tuh_cdc_write_flush(_cdc_idx);
    } else {
      if (!USBHost.tuhBackgroundActive()) {
        tuh_task();
      } else {
        yield();
      }
    }
  }

  xSemaphoreGive(s_usb_host_serial_tx_mutex);
  return so_far;
}

bool USBHostSerialClass::readClear() {
  if (!mounted()) {
    return false;
  }
  return tuh_cdc_read_clear(_cdc_idx);
}

bool USBHostSerialClass::setLineCoding(uint32_t baudrate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits) {
  if (!mounted()) {
    return false;
  }
  cdc_line_coding_t line_coding = {0};
  line_coding.bit_rate = baudrate;
  line_coding.stop_bits = stop_bits;
  line_coding.parity = parity;
  line_coding.data_bits = data_bits;
  return tuh_cdc_set_line_coding_sync(_cdc_idx, &line_coding) == XFER_RESULT_SUCCESS;
}

bool USBHostSerialClass::setControlLineState(uint16_t line_state) {
  if (!mounted()) {
    return false;
  }
  return tuh_cdc_set_control_line_state_sync(_cdc_idx, line_state) == XFER_RESULT_SUCCESS;
}

bool USBHostSerialClass::connectDefault() {
  if (!mounted()) {
    return false;
  }
  return tuh_cdc_connect_sync(_cdc_idx) == XFER_RESULT_SUCCESS;
}

bool USBHostSerialClass::disconnectControl() {
  if (!mounted()) {
    return false;
  }
  return tuh_cdc_disconnect_sync(_cdc_idx) == XFER_RESULT_SUCCESS;
}

void USBHostSerialClass::onCdcMount(uint8_t idx) {
  tuh_itf_info_t info = {0};
  if (!tuh_cdc_itf_get_info(idx, &info)) {
    return;
  }

  _cdc_idx = idx;
  _dev_addr = info.daddr;
  _itf_num = info.desc.bInterfaceNumber;
  _binding_valid = true;
  _mounted = true;

  log_v("HOST CDC mounted");

  applyBeginLineCodingIfNeeded();
}

void USBHostSerialClass::onCdcUnmount(uint8_t idx) {
  (void)idx;
  _mounted = false;
  _binding_valid = false;
  log_v("HOST CDC unmounted");
}

USBHostSerialClass USBHostSerial;

// TinyUSB callbacks — strong symbols override weak stubs in esp32-hal-tinyusb.c
extern "C" {

void tuh_cdc_mount_cb(uint8_t idx) {
  USBHostSerial.onCdcMount(idx);
}

void tuh_cdc_umount_cb(uint8_t idx) {
  USBHostSerial.onCdcUnmount(idx);
}

void tuh_cdc_rx_cb(uint8_t idx) {
  (void)idx;
}

void tuh_cdc_tx_complete_cb(uint8_t idx) {
  (void)idx;
}

}  // extern "C"

#endif /* SOC_USB_OTG_SUPPORTED && CONFIG_TINYUSB_ENABLED && CFG_TUH_ENABLED && CFG_TUH_CDC */
