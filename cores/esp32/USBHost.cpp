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

#include "USBHost.h"

#if SOC_USB_OTG_SUPPORTED
#if CONFIG_TINYUSB_ENABLED

#include "esp32-hal-tinyusb.h"
#include "Arduino.h"

#if CFG_TUH_ENABLED

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CFG_TUH_HID && __has_include("USBHostHID.h")
#include "USBHostHID.h"
#undef USBHostHID
#define ARDUINO_USBHOST_HAS_HID_SERVICE 1
#else
#define ARDUINO_USBHOST_HAS_HID_SERVICE 0
#endif

static TaskHandle_t s_tuh_worker = nullptr;

static void arduino_usb_host_tuh_worker(void *arg) {
  (void)arg;
  for (;;) {
    tuh_task();
    /*
     * Do not call USBHostHID serviceReceives() here every tick: it submits HID IN transfers
     * and can saturate the DWC2 host controller, starving control transfers used for hub
     * port power / reset and downstream enumeration (symptom: nothing powers on hub ports).
     * HID servicing runs from USBHost.task() in loop() and from each handler's available().
     */
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

bool USBHostClass::tuhBackgroundActive() const {
  return s_tuh_worker != nullptr;
}

bool USBHostClass::begin() {
  if (_started) {
    return true;
  }
  /*
   * ESP32-S3-USB-OTG: host mux + VBUS before host controller init (same as device_info).
   * Drive GPIO here — do not call usbHostEnable/usbHostPower from core: those live in
   * variant.cpp and may not link into the same archive as USBHost.cpp in some builds.
   */
#if defined(USB_HOST_EN) && defined(DEV_VBUS_EN) && defined(LIMIT_EN)
#  if defined(BOOST_EN)
  pinMode(BOOST_EN, OUTPUT);
  digitalWrite(BOOST_EN, LOW);
#  endif
  pinMode(USB_HOST_EN, OUTPUT);
  digitalWrite(USB_HOST_EN, HIGH);
  delay(10);
  pinMode(DEV_VBUS_EN, OUTPUT);
  digitalWrite(DEV_VBUS_EN, HIGH);
  pinMode(LIMIT_EN, OUTPUT);
  digitalWrite(LIMIT_EN, HIGH);
  delay(10);
#endif
  tinyusb_host_config_t host_config = {
    .rhport = 0,
  };
  esp_err_t err = tinyusb_host_init(&host_config);
  if (err != ESP_OK) {
    log_e("[USBHost] begin() failed: 0x%x (%s)", (unsigned)err, esp_err_to_name(err));
    return false;
  }
  _started = true;

  if (s_tuh_worker == nullptr) {
    /* Priority well above Arduino loop (~1) so bulk OUT / MSC completions make progress while FatFS waits. */
#if defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE
    const BaseType_t ok = xTaskCreate(arduino_usb_host_tuh_worker, "usbhTuh", 8192, nullptr, 17, &s_tuh_worker);
#else
    const BaseType_t ok = xTaskCreatePinnedToCore(arduino_usb_host_tuh_worker, "usbhTuh", 8192, nullptr, 17, &s_tuh_worker, 0);
#endif
    if (ok != pdPASS) {
      s_tuh_worker = nullptr;
    }
  }

  return true;
}

void USBHostClass::task() {
  if (_started) {
    /* When the worker exists, tuh_task runs there; calling here would race the same stack. */
    if (s_tuh_worker == nullptr) {
      tuh_task();
    }
#if ARDUINO_USBHOST_HAS_HID_SERVICE
    USBHostHIDInstance.serviceReceives();
#endif
  }
}

#else

bool USBHostClass::tuhBackgroundActive() const {
  return false;
}

bool USBHostClass::begin() {
  (void)0;
  return false;
}

void USBHostClass::task() {
}

#endif /* CFG_TUH_ENABLED */

USBHostClass USBHost;

#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
