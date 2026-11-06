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

#include "tusb.h"
#include "tusb_config.h"

#if CFG_TUH_ENABLED
/** Weak in core; USBHostHID provides the strong definition (HID arm after tuh_task). */
extern "C" void arduino_usb_host_hid_service(void);
/** Weak in core; USBHostHID dispatches deferred report callbacks (loop-safe). */
extern "C" void arduino_usb_host_hid_dispatch(void);
/** Weak in core; USBHostHID arms HID IN only after the whole device has enumerated. */
extern "C" void arduino_usb_host_hid_device_mounted(uint8_t daddr);

/**
 * Board mux / VBUS hook. Weak empty default is in esp32-hal-misc.c;
 * override in the variant (see variants/esp32s3usbotg).
 */
extern "C" void USBHostBoardInit(void);
#endif

#ifndef ARDUINO_USB_HOST_CORE
#define ARDUINO_USB_HOST_CORE 0
#endif

/**
 * @brief USB Host controller.
 *
 * Call begin() once in setup(), then task() in loop().
 * After begin(), TinyUSB runs on a background worker — task() still dispatches HID
 * report callbacks (do not call tuh_* from those callbacks).
 */
class USBHostClass {
public:
  /** Start USB host mode. @return true on success. */
  bool begin();

  /**
   * Pin the TinyUSB host worker. Call before begin().
   * Default is ARDUINO_USB_HOST_CORE (0). Use -1 to leave the task unpinned.
   */
  void setCore(int coreId) {
    _core = coreId;
  }
  int core() const {
    return _core;
  }

  /** Dispatch HID callbacks; also runs TinyUSB when no background worker. */
  void task();

  /** True when TinyUSB is serviced by the background worker (do not call tuh_* from loop()). */
  bool tuhBackgroundActive() const;

  bool started() const {
    return _started;
  }
  operator bool() const {
    return _started;
  }

private:
  bool _started = false;
  int _core = ARDUINO_USB_HOST_CORE;
};

extern USBHostClass USBHost;

#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
