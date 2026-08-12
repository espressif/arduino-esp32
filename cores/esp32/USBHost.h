// Copyright 2015-2024 Espressif Systems (Shanghai) PTE LTD
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
/**
 * @brief Optional HID host service hook (weak in core; USBHostHID provides strong).
 * Called on the TinyUSB host task after tuh_task() — arm interrupt IN transfers there.
 */
extern "C" void arduino_usb_host_hid_service(void);
#endif

/**
 * @brief USB Host controller (Arduino API).
 * Call begin() once in setup(), then task() in loop() so that
 * TinyUSB host stack can enumerate and communicate with devices.
 */
class USBHostClass {
public:
  /**
   * @brief Start USB in host mode.
   * @return true on success.
   */
  bool begin();

  /**
   * @brief Process USB host events. Must be called repeatedly (e.g. from loop()).
   * If tuhBackgroundActive() is true, tuh_task() and HID receive arming run from an
   * internal FreeRTOS task — this call must not touch TinyUSB host transfer APIs.
   */
  void task();

  /**
   * @brief True when TinyUSB host is serviced by a background task (after begin()).
   * Class drivers (MSC, HID) must not call tuh_task() / submit transfers from loop() then.
   */
  bool tuhBackgroundActive() const;

  /**
   * @brief Check if host stack is running.
   */
  bool started() const {
    return _started;
  }

  operator bool() const {
    return _started;
  }

private:
  bool _started = false;
};

extern USBHostClass USBHost;

#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
