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

#ifndef ADAFRUIT_USBD_HID_H_
#define ADAFRUIT_USBD_HID_H_

#include "arduino/Adafruit_USBD_Device.h"

class Adafruit_USBD_HID : public Adafruit_USBD_Interface {
public:
  typedef uint16_t (*get_report_callback_t)(uint8_t report_id,
                                            hid_report_type_t report_type,
                                            uint8_t *buffer, uint16_t reqlen);
  typedef void (*set_report_callback_t)(uint8_t report_id,
                                        hid_report_type_t report_type,
                                        uint8_t const *buffer,
                                        uint16_t bufsize);

  Adafruit_USBD_HID(void);

  void setPollInterval(uint8_t interval_ms);
  void setBootProtocol(uint8_t protocol); // 0: None, 1: Keyboard, 2:Mouse
  void enableOutEndpoint(bool enable);
  void setReportDescriptor(uint8_t const *desc_report, uint16_t len);
  void setReportCallback(get_report_callback_t get_report,
                         set_report_callback_t set_report);

  bool begin(void);

  bool ready(void);
  bool sendReport(uint8_t report_id, void const *report, uint8_t len);

  // Report helpers
  bool sendReport8(uint8_t report_id, uint8_t num);
  bool sendReport16(uint8_t report_id, uint16_t num);
  bool sendReport32(uint8_t report_id, uint32_t num);

  //------------- Keyboard API -------------//
  bool keyboardReport(uint8_t report_id, uint8_t modifier, uint8_t keycode[6]);
  bool keyboardPress(uint8_t report_id, char ch);
  bool keyboardRelease(uint8_t report_id);

  //------------- Mouse API -------------//
  bool mouseReport(uint8_t report_id, uint8_t buttons, int8_t x, int8_t y,
                   int8_t vertical, int8_t horizontal);
  bool mouseMove(uint8_t report_id, int8_t x, int8_t y);
  bool mouseScroll(uint8_t report_id, int8_t scroll, int8_t pan);
  bool mouseButtonPress(uint8_t report_id, uint8_t buttons);
  bool mouseButtonRelease(uint8_t report_id);

  // from Adafruit_USBD_Interface
  virtual uint16_t getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf,
                                          uint16_t bufsize);

private:
  uint8_t _interval_ms;
  uint8_t _protocol;
  bool _out_endpoint;
  uint8_t _mouse_button;

  uint16_t _desc_report_len;
  uint8_t const *_desc_report;

  get_report_callback_t _get_report_cb;
  set_report_callback_t _set_report_cb;

  friend uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                                        hid_report_type_t report_type,
                                        uint8_t *buffer, uint16_t reqlen);
  friend void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                                    hid_report_type_t report_type,
                                    uint8_t const *buffer, uint16_t bufsize);
  friend uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf);
};

#endif /* ADAFRUIT_USBD_HID_H_ */
