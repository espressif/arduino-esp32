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
   * If tuhBackgroundActive() is true, tuh_task() runs from an internal FreeRTOS task
   * and this call is optional (still safe to call).
   */
  void task();

  /**
   * @brief True when TinyUSB host is serviced by a background task (after begin()).
   * Needed so MSC sync SCSI wait can yield without calling tuh_task() on the same stack
   * (avoids DWC2 bulk-OUT deadlocks during FatFS writes).
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
