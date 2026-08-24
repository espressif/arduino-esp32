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

#include "USBHost.h"

#if SOC_USB_OTG_SUPPORTED
#if CONFIG_TINYUSB_ENABLED

#include "esp32-hal-tinyusb.h"
#include "Arduino.h"

#if CFG_TUH_ENABLED

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Do not #include USBHostHID.h here — library paths are often missing from core builds.
 * HID provides a strong arduino_usb_host_hid_service() instead. */
extern "C" void arduino_usb_host_hid_service(void) __attribute__((weak));
extern "C" void arduino_usb_host_hid_service(void) {}

static TaskHandle_t s_tuh_worker = NULL;

static void arduino_usb_host_tuh_worker(void *arg) {
  (void)arg;
  for (;;) {
    tuh_task();
    /* HID IN arm/re-arm only after tuh_task — never from loop() (DWC2 race). */
    arduino_usb_host_hid_service();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

bool USBHostClass::tuhBackgroundActive() const {
  return s_tuh_worker != NULL;
}

bool USBHostClass::begin() {
  if (_started) {
    return true;
  }

  USBHostBoardInit();

  esp_err_t err = tinyusb_host_init();
  if (err != ESP_OK) {
    log_e("[USBHost] begin() failed: 0x%x (%s)", (unsigned)err, esp_err_to_name(err));
    return false;
  }
  _started = true;

  if (s_tuh_worker == NULL) {
    const BaseType_t ok =
      xTaskCreateUniversal(arduino_usb_host_tuh_worker, "usbhTuh", 8192, NULL, 17, &s_tuh_worker, _core);
    if (ok != pdPASS) {
      s_tuh_worker = NULL;
    }
  }

  return true;
}

void USBHostClass::task() {
  if (!_started) {
    return;
  }
  if (s_tuh_worker == NULL) {
    tuh_task();
    arduino_usb_host_hid_service();
  }
}

#else

bool USBHostClass::tuhBackgroundActive() const {
  return false;
}

bool USBHostClass::begin() {
  return false;
}

void USBHostClass::task() {}

#endif /* CFG_TUH_ENABLED */

USBHostClass USBHost;

#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
