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

#include <stdint.h>
#include <stdbool.h>
#include <cstddef>

#if __has_include("tusb_config.h")
#include "tusb_config.h"
#endif
#ifndef CFG_TUH_HID
#define CFG_TUH_HID 0
#endif

class USBHostHIDClass;

/**
 * @brief Base class for USB Host HID handlers (mouse, keyboard, gamepad, …).
 *
 * Implement claim() / onUnmount() / onReport(), then register with USBHostHID.addDevice()
 * (or call registerWithHost() on the concrete handler) before USBHost.begin().
 */
class USBHostHIDDevice {
public:
  USBHostHIDDevice()
    : _dev_addr(0), _idx(0), _mounted(false), _itf_binding_valid(false), _itf_number(0) {}
  virtual ~USBHostHIDDevice() {}

  /** Return true to claim this HID interface (first matching handler wins). */
  virtual bool claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol,
                     const uint8_t *report_desc, uint16_t desc_len) = 0;

  virtual void onUnmount(uint8_t dev_addr, uint8_t idx) = 0;

  /**
   * INPUT report for this interface.
   * Do not call tuh_hid_receive_report() here — USBHostHID resubmits after return.
   */
  virtual void onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) = 0;

  /** Arduino-side mount cache (safe from loop(); does not call TinyUSB). */
  bool mounted() const;

  uint8_t devAddr() const {
    return _dev_addr;
  }
  uint8_t interfaceIndex() const {
    return _idx;
  }
  bool matches(uint8_t dev_addr, uint8_t idx) const {
    return _dev_addr == dev_addr && _idx == idx;
  }

protected:
  friend class USBHostHIDClass;

  /** Bind after claim() succeeds. Fails if this handler already owns another iface. */
  bool bindHidInterface(uint8_t dev_addr, uint8_t idx);
  void clearHidInterfaceBinding();
  void syncHostMountState();

  uint8_t _dev_addr;
  uint8_t _idx;
  bool _mounted;
  bool _itf_binding_valid;
  uint8_t _itf_number;
};

/**
 * @brief USB Host HID dispatcher (like device-side USBHID).
 *
 * TinyUSB callbacks land here and are forwarded to registered USBHostHIDDevice handlers.
 */
class USBHostHIDClass {
public:
  USBHostHIDClass();

  /** Register a handler. Prefer calling from setup() before USBHost.begin(). */
  bool addDevice(USBHostHIDDevice *dev);

  /**
   * From loop() / available(): no-op while the host worker owns TinyUSB.
   * Without a worker, arms interrupt IN on the caller.
   */
  void serviceReceives();

private:
  friend void tuh_hid_mount_cb(uint8_t, uint8_t, uint8_t const *, uint16_t);
  friend void tuh_hid_umount_cb(uint8_t, uint8_t);
  friend void tuh_hid_report_received_cb(uint8_t, uint8_t, uint8_t const *, uint16_t);
  friend void tuh_umount_cb(uint8_t);
  friend void arduino_usb_host_hid_service(void);

  void _onMount(uint8_t dev_addr, uint8_t idx, uint8_t const *report_desc, uint16_t desc_len);
  void _onUnmount(uint8_t dev_addr, uint8_t idx);
  void _onDeviceUnmount(uint8_t dev_addr);
  void _onReport(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len);

  void serviceReceivesFromHostTask();
  void startReceiveIfPending();
  void requestPeerRearm(uint8_t new_dev_addr, uint8_t new_idx);
  bool hasOtherMounted(uint8_t dev_addr, uint8_t idx) const;
  void clearPendingIf(uint8_t dev_addr, uint8_t idx);
  void releaseClaimedInterface(USBHostHIDDevice *dev, uint8_t dev_addr, uint8_t idx);

  static const size_t MAX_DEVICES = 8;
  USBHostHIDDevice *_devices[MAX_DEVICES];
  size_t _num_devices;
  bool _start_receive_pending;
  uint8_t _pending_dev_addr;
  uint8_t _pending_idx;
  bool _rearm_peers;
  uint32_t _rearm_after_ms;
  uint8_t _rearm_skip_addr;
  uint8_t _rearm_skip_idx;
};

extern USBHostHIDClass USBHostHID;
