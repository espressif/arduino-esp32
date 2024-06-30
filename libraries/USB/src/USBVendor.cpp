// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
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
#include "USBVendor.h"
#if SOC_USB_OTG_SUPPORTED

#if CONFIG_TINYUSB_VENDOR_ENABLED

#include "esp32-hal-tinyusb.h"

ESP_EVENT_DEFINE_BASE(ARDUINO_USB_VENDOR_EVENTS);
esp_err_t arduino_usb_event_post(esp_event_base_t event_base, int32_t event_id, void *event_data, size_t event_data_size, TickType_t ticks_to_wait);
esp_err_t arduino_usb_event_handler_register_with(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);

static USBVendor *_Vendor = NULL;
static QueueHandle_t rx_queue = NULL;
static uint8_t USB_VENDOR_ENDPOINT_SIZE = 64;

uint16_t tusb_vendor_load_descriptor(uint8_t *dst, uint8_t *itf) {
  uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB Vendor");
  uint8_t ep_num = tinyusb_get_free_duplex_endpoint();
  TU_VERIFY(ep_num != 0);
  uint8_t descriptor[TUD_VENDOR_DESC_LEN] = {// Interface number, string index, EP Out & IN address, EP size
                                             TUD_VENDOR_DESCRIPTOR(*itf, str_index, ep_num, (uint8_t)(0x80 | ep_num), USB_VENDOR_ENDPOINT_SIZE)
  };
  *itf += 1;
  memcpy(dst, descriptor, TUD_VENDOR_DESC_LEN);
  return TUD_VENDOR_DESC_LEN;
}

void tud_vendor_rx_cb(uint8_t itf) {
  size_t len = tud_vendor_n_available(itf);
  log_v("%u", len);
  if (len) {
    uint8_t buffer[len];
    len = tud_vendor_n_read(itf, buffer, len);
    log_buf_v(buffer, len);
    if (_Vendor) {
      _Vendor->_onRX(buffer, len);
    }
  } else {
    if (_Vendor) {
      _Vendor->_onRX(NULL, len);
    }
  }
}

extern "C" bool tinyusb_vendor_control_request_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  log_v(
    "Port: %u, Stage: %u, Direction: %u, Type: %u, Recipient: %u, bRequest: 0x%x, wValue: %u, wIndex: %u, wLength: %u", rhport, stage,
    request->bmRequestType_bit.direction, request->bmRequestType_bit.type, request->bmRequestType_bit.recipient, request->bRequest, request->wValue,
    request->wIndex, request->wLength
  );

  if (_Vendor) {
    return _Vendor->_onRequest(rhport, stage, (arduino_usb_control_request_t const *)request);
  }
  return false;
}

USBVendor::USBVendor(uint8_t endpoint_size) : itf(0), cb(NULL) {
  if (!_Vendor) {
    _Vendor = this;
    if (endpoint_size <= 64) {
      USB_VENDOR_ENDPOINT_SIZE = endpoint_size;
    }
    tinyusb_enable_interface(USB_INTERFACE_VENDOR, TUD_VENDOR_DESC_LEN, tusb_vendor_load_descriptor);
  } else {
    itf = _Vendor->itf;
    cb = _Vendor->cb;
  }
}

size_t USBVendor::setRxBufferSize(size_t rx_queue_len) {
  if (rx_queue) {
    if (!rx_queue_len) {
      vQueueDelete(rx_queue);
      rx_queue = NULL;
    }
    return 0;
  }
  rx_queue = xQueueCreate(rx_queue_len, sizeof(uint8_t));
  if (!rx_queue) {
    return 0;
  }
  return rx_queue_len;
}

void USBVendor::begin() {
  setRxBufferSize(256);  //default if not preset
}

void USBVendor::end() {
  setRxBufferSize(0);
}

void USBVendor::onEvent(esp_event_handler_t callback) {
  onEvent(ARDUINO_USB_VENDOR_ANY_EVENT, callback);
}

void USBVendor::onEvent(arduino_usb_vendor_event_t event, esp_event_handler_t callback) {
  arduino_usb_event_handler_register_with(ARDUINO_USB_VENDOR_EVENTS, event, callback, this);
}

bool USBVendor::mounted() {
  return tud_vendor_n_mounted(itf);
}

bool USBVendor::sendResponse(uint8_t rhport, arduino_usb_control_request_t const *request, void *data, size_t len) {
  if (!request) {
    return false;
  }
  if (!data || !len) {
    return tud_control_status(rhport, (tusb_control_request_t const *)request);
  } else {
    return tud_control_xfer(rhport, (tusb_control_request_t const *)request, data, len);
  }
}

void USBVendor::onRequest(arduino_usb_vendor_control_request_handler_t handler) {
  cb = handler;
}

bool USBVendor::_onRequest(uint8_t rhport, uint8_t stage, arduino_usb_control_request_t const *request) {
  if (cb) {
    return cb(rhport, stage, request);
  }
  return false;
}

void USBVendor::_onRX(const uint8_t *buffer, size_t len) {
  for (uint32_t i = 0; i < len; i++) {
    if (rx_queue == NULL || !xQueueSend(rx_queue, buffer + i, 0)) {
      len = i + 1;
      log_e("RX Queue Overflow");
      break;
    }
  }
  arduino_usb_vendor_event_data_t p;
  p.data.len = len;
  arduino_usb_event_post(ARDUINO_USB_VENDOR_EVENTS, ARDUINO_USB_VENDOR_DATA_EVENT, &p, sizeof(arduino_usb_vendor_event_data_t), portMAX_DELAY);
}

size_t USBVendor::write(const uint8_t *buffer, size_t len) {
  if (!mounted()) {
    log_e("not mounted");
    return 0;
  }
  size_t max_len = tud_vendor_n_write_available(itf);
  if (len > max_len) {
    len = max_len;
  }
  if (len) {
    return tud_vendor_n_write(itf, buffer, len);
  }
  return len;
}

size_t USBVendor::write(uint8_t c) {
  return write(&c, 1);
}

int USBVendor::available(void) {
  if (rx_queue == NULL) {
    return -1;
  }
  return uxQueueMessagesWaiting(rx_queue);
}

int USBVendor::peek(void) {
  if (rx_queue == NULL) {
    return -1;
  }
  uint8_t c;
  if (xQueuePeek(rx_queue, &c, 0)) {
    return c;
  }
  return -1;
}

int USBVendor::read(void) {
  if (rx_queue == NULL) {
    return -1;
  }
  uint8_t c = 0;
  if (xQueueReceive(rx_queue, &c, 0)) {
    return c;
  }
  return -1;
}

size_t USBVendor::read(uint8_t *buffer, size_t size) {
  if (rx_queue == NULL) {
    return -1;
  }
  uint8_t c = 0;
  size_t count = 0;
  while (count < size && xQueueReceive(rx_queue, &c, 0)) {
    buffer[count++] = c;
  }
  return count;
}

void USBVendor::flush(void) {
  tud_vendor_n_write_flush(itf);
}

#endif /* CONFIG_TINYUSB_VENDOR_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
