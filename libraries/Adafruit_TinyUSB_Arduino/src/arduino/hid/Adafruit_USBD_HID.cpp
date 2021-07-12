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

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUD_HID

#include "Adafruit_USBD_HID.h"

#define EPOUT 0x00
#define EPIN 0x80

uint8_t const _ascii2keycode[128][2] = {HID_ASCII_TO_KEYCODE};

static Adafruit_USBD_HID *_hid_dev = NULL;

//------------- IMPLEMENTATION -------------//
Adafruit_USBD_HID::Adafruit_USBD_HID(void) {
  _interval_ms = 10;
  _protocol = HID_ITF_PROTOCOL_NONE;

  _out_endpoint = false;
  _mouse_button = 0;

  _desc_report = NULL;
  _desc_report_len = 0;

  _get_report_cb = NULL;
  _set_report_cb = NULL;
}

void Adafruit_USBD_HID::setPollInterval(uint8_t interval_ms) {
  _interval_ms = interval_ms;
}

void Adafruit_USBD_HID::setBootProtocol(uint8_t protocol) {
  _protocol = protocol;
}

void Adafruit_USBD_HID::enableOutEndpoint(bool enable) {
  _out_endpoint = enable;
}

void Adafruit_USBD_HID::setReportDescriptor(uint8_t const *desc_report,
                                            uint16_t len) {
  _desc_report = desc_report;
  _desc_report_len = len;
}

void Adafruit_USBD_HID::setReportCallback(get_report_callback_t get_report,
                                          set_report_callback_t set_report) {
  _get_report_cb = get_report;
  _set_report_cb = set_report;
}

uint16_t Adafruit_USBD_HID::getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf,
                                                   uint16_t bufsize) {
  if (!_desc_report_len) {
    return 0;
  }

  // usb core will automatically update endpoint number
  uint8_t const desc_inout[] = {
      TUD_HID_INOUT_DESCRIPTOR(itfnum, 0, _protocol, _desc_report_len, EPIN,
                               EPOUT, CFG_TUD_HID_EP_BUFSIZE, _interval_ms)};
  uint8_t const desc_in_only[] = {
      TUD_HID_DESCRIPTOR(itfnum, 0, _protocol, _desc_report_len, EPIN,
                         CFG_TUD_HID_EP_BUFSIZE, _interval_ms)};

  uint8_t const *desc;
  uint16_t len;

  if (_out_endpoint) {
    desc = desc_inout;
    len = sizeof(desc_inout);
  } else {
    desc = desc_in_only;
    len = sizeof(desc_in_only);
  }

  if (bufsize < len) {
    return 0;
  }

  memcpy(buf, desc, len);
  return len;
}

bool Adafruit_USBD_HID::begin(void) {
  if (!USBDevice.addInterface(*this)) {
    return false;
  }

  _hid_dev = this;
  return true;
}

bool Adafruit_USBD_HID::ready(void) { return tud_hid_ready(); }

bool Adafruit_USBD_HID::sendReport(uint8_t report_id, void const *report,
                                   uint8_t len) {
  return tud_hid_report(report_id, report, len);
}

bool Adafruit_USBD_HID::sendReport8(uint8_t report_id, uint8_t num) {
  return tud_hid_report(report_id, &num, sizeof(num));
}

bool Adafruit_USBD_HID::sendReport16(uint8_t report_id, uint16_t num) {
  return tud_hid_report(report_id, &num, sizeof(num));
}

bool Adafruit_USBD_HID::sendReport32(uint8_t report_id, uint32_t num) {
  return tud_hid_report(report_id, &num, sizeof(num));
}

//------------- TinyUSB callbacks -------------//
extern "C" {

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor, whose contents must exist long
// enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf) {
  (void)itf;

  if (!_hid_dev) {
    return NULL;
  }

  return _hid_dev->_desc_report;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
  (void)itf;

  if (!(_hid_dev && _hid_dev->_get_report_cb)) {
    return 0;
  }

  return _hid_dev->_get_report_cb(report_id, report_type, buffer, reqlen);
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
  (void)itf;

  if (!(_hid_dev && _hid_dev->_set_report_cb)) {
    return;
  }

  _hid_dev->_set_report_cb(report_id, report_type, buffer, bufsize);
}

} // extern "C"

//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+

bool Adafruit_USBD_HID::keyboardReport(uint8_t report_id, uint8_t modifier,
                                       uint8_t keycode[6]) {
  return tud_hid_keyboard_report(report_id, modifier, keycode);
}

bool Adafruit_USBD_HID::keyboardPress(uint8_t report_id, char ch) {
  uint8_t keycode[6] = {0};
  uint8_t modifier = 0;
  uint8_t uch = (uint8_t)ch;

  if (_ascii2keycode[uch][0]) {
    modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
  }
  keycode[0] = _ascii2keycode[uch][1];

  return tud_hid_keyboard_report(report_id, modifier, keycode);
}

bool Adafruit_USBD_HID::keyboardRelease(uint8_t report_id) {
  return tud_hid_keyboard_report(report_id, 0, NULL);
}

//--------------------------------------------------------------------+
// Mouse
//--------------------------------------------------------------------+

bool Adafruit_USBD_HID::mouseReport(uint8_t report_id, uint8_t buttons,
                                    int8_t x, int8_t y, int8_t vertical,
                                    int8_t horizontal) {
  // cache mouse button for other API such as move, scroll
  _mouse_button = buttons;

  return tud_hid_mouse_report(report_id, buttons, x, y, vertical, horizontal);
}

bool Adafruit_USBD_HID::mouseMove(uint8_t report_id, int8_t x, int8_t y) {
  return tud_hid_mouse_report(report_id, _mouse_button, x, y, 0, 0);
}

bool Adafruit_USBD_HID::mouseScroll(uint8_t report_id, int8_t scroll,
                                    int8_t pan) {
  return tud_hid_mouse_report(report_id, _mouse_button, 0, 0, scroll, pan);
}

bool Adafruit_USBD_HID::mouseButtonPress(uint8_t report_id, uint8_t buttons) {
  return tud_hid_mouse_report(report_id, buttons, 0, 0, 0, 0);
}

bool Adafruit_USBD_HID::mouseButtonRelease(uint8_t report_id) {
  return tud_hid_mouse_report(report_id, 0, 0, 0, 0, 0);
}

#endif // TUSB_OPT_DEVICE_ENABLED
