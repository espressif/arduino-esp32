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

extern "C" void tuh_mount_cb(uint8_t daddr) {
  /* TinyUSB sets configured (tuh_mounted) at SET_CONFIGURATION, before remaining
   * HID interfaces finish GET_REPORT_DESCRIPTOR. Arm IN from this callback only. */
  arduino_usb_host_hid_device_mounted(daddr);
}

/* Do not #include USBHostHID.h here — library paths are often missing from core builds.
 * HID provides a strong arduino_usb_host_hid_service() instead. */
extern "C" void arduino_usb_host_hid_service(void) __attribute__((weak));
extern "C" void arduino_usb_host_hid_service(void) {}
extern "C" void arduino_usb_host_hid_dispatch(void) __attribute__((weak));
extern "C" void arduino_usb_host_hid_dispatch(void) {}
extern "C" void arduino_usb_host_hid_device_mounted(uint8_t daddr) __attribute__((weak));
extern "C" void arduino_usb_host_hid_device_mounted(uint8_t daddr) {
  (void)daddr;
}

static TaskHandle_t s_tuh_worker = NULL;

/* A wedged host is otherwise silent. Publish progress for loop() to watch.
 * tuh_task() would block on the event queue forever, so an idle bus is
 * indistinguishable from a wedge — poll with a bounded timeout instead. This
 * also keeps the HID re-arm service running when no USB event arrives. */
enum {
  TUH_TASK_TIMEOUT_MS = 10,
  TUH_PHASE_TASK = 1,
  TUH_PHASE_HID = 2,
  TUH_PHASE_IDLE = 3,
};
static volatile uint32_t s_tuh_iter = 0;
static volatile uint8_t s_tuh_phase = TUH_PHASE_IDLE;

static void arduino_usb_host_tuh_worker(void *arg) {
  (void)arg;
  for (;;) {
    s_tuh_phase = TUH_PHASE_TASK;
    tuh_task_ext(TUH_TASK_TIMEOUT_MS, false);
    /* HID IN arm/re-arm only after tuh_task — never from loop() (DWC2 race). */
    s_tuh_phase = TUH_PHASE_HID;
    arduino_usb_host_hid_service();
    s_tuh_phase = TUH_PHASE_IDLE;
    s_tuh_iter = s_tuh_iter + 1; /* ++ on a volatile is deprecated in C++20 */
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

/* Loop-side only: never touches TinyUSB. Reports once per stall episode. */
static void arduino_usb_host_worker_watch(void) {
  static uint32_t last_iter = 0;
  static uint32_t last_change_ms = 0;
  static bool stalled = false;

  const uint32_t iter = s_tuh_iter;
  const uint32_t now = millis();

  if (iter != last_iter || last_change_ms == 0) {
    if (stalled) {
      stalled = false;
      log_w("[USBHost] worker resumed after stall (iter=%u)", (unsigned)iter);
    }
    last_iter = iter;
    last_change_ms = now;
    return;
  }
  if (!stalled && (now - last_change_ms) > 1000u) {
    stalled = true;
    log_e(
      "[USBHost] worker stalled %ums in %s (iter=%u)", (unsigned)(now - last_change_ms),
      (s_tuh_phase == TUH_PHASE_TASK) ? "tuh_task" : (s_tuh_phase == TUH_PHASE_HID) ? "hid service" : "idle", (unsigned)iter
    );
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
    const BaseType_t ok = xTaskCreateUniversal(arduino_usb_host_tuh_worker, "usbhTuh", 8192, NULL, 17, &s_tuh_worker, _core);
    if (ok != pdPASS) {
      s_tuh_worker = NULL;
    }
  }

  return true;
}

void USBHostClass::task() {
  /* Always drain HID callbacks on the Arduino loop task — never from usbhTuh.
   * Serial/FS in TinyUSB callbacks stalls DWC2 and can Store-fault on P4. */
  arduino_usb_host_hid_dispatch();
  if (!_started) {
    return;
  }
  if (s_tuh_worker == NULL) {
    /* Loop fallback must not block: tuh_task() waits on the event queue forever. */
    tuh_task_ext(0, false);
    arduino_usb_host_hid_service();
    arduino_usb_host_hid_dispatch();
  } else {
    arduino_usb_host_worker_watch();
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
