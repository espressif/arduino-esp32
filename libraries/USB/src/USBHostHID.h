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

#include <stdint.h>
#include <stdbool.h>
#include <cstddef>

#if __has_include("tusb_config.h")
#include "tusb_config.h"
#endif
#ifndef CFG_TUH_HID
#define CFG_TUH_HID 0
#endif

/**
 * @brief Base class for a USB Host HID device handler (mouse, keyboard, etc.).
 * Implement claim(), onUnmount(), and onReport(); register with USBHostHID via addDevice().
 */
class USBHostHIDDevice {
public:
  USBHostHIDDevice()
    : _dev_addr(0), _idx(0), _mounted(false), _itf_binding_valid(false), _itf_number(0) {}
  virtual ~USBHostHIDDevice() {}

  /**
   * @brief Return true if this handler claims the given HID interface (e.g. protocol == MOUSE).
   * Called from mount callback; first handler to return true gets the interface.
   */
  virtual bool claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol,
                     const uint8_t *report_desc, uint16_t desc_len) = 0;

  /**
   * @brief Called when the interface is unmounted (device unplugged or interface released).
   */
  virtual void onUnmount(uint8_t dev_addr, uint8_t idx) = 0;

  /**
   * @brief Called when an INPUT report is received for this interface.
   * Do not call tuh_hid_receive_report() here — USBHostHID resubmits on the host task
   * after this returns (avoids re-queueing IN during disconnect).
   */
  virtual void onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) = 0;

  /**
   * @brief True if this handler has a claimed interface (Arduino-side cache).
   * Safe from loop(): does not call into TinyUSB (host stack is not thread-safe with usbhTuh).
   */
  bool mounted() const;

  /** @brief Device address of the claimed interface (0 if not mounted). */
  uint8_t devAddr() const { return _dev_addr; }

  /** @brief HID interface index of the claimed interface. */
  uint8_t interfaceIndex() const { return _idx; }

  /** @brief Return true if this handler owns the given (dev_addr, idx). */
  bool matches(uint8_t dev_addr, uint8_t idx) const {
    return _dev_addr == dev_addr && _idx == idx;
  }

  /** Drop cached mount if the host stack no longer has this (dev, idx). */
  void syncHostMountState();

protected:
  /**
   * After claim() heuristics pass, record USB bInterfaceNumber and bind (dev_addr, idx).
   * Fails if this handler already owns a different interface (one device per singleton).
   */
  bool bindHidInterface(uint8_t dev_addr, uint8_t idx);
  void clearHidInterfaceBinding();

  uint8_t _dev_addr;
  uint8_t _idx;
  bool _mounted;
  bool _itf_binding_valid;
  uint8_t _itf_number;
};

/**
 * @brief Central USB Host HID class (mirrors device-side USBHID role).
 * Owns TinyUSB HID host callbacks; dispatches mount/unmount/report to registered
 * USBHostHIDDevice handlers. Call addDevice() for each handler (e.g. mouse, keyboard).
 */
class USBHostHID {
public:
  USBHostHID();

  /**
   * @brief Register a device handler. Call from setup() or from the handler's constructor.
   */
  bool addDevice(USBHostHIDDevice *dev);

  /**
   * @brief Start the first interrupt IN if a handler just claimed an interface.
   * Called from the host task; sketches normally use serviceReceives() / available().
   */
  void startReceiveIfPending();

  /**
   * @brief Safe from loop / available(): no-ops when the USB host background worker owns
   * TinyUSB. Without a worker, arms interrupt IN on the caller.
   */
  void serviceReceives();

  /**
   * @brief Run only on the TinyUSB host task (usbhTuh worker or USBHost.task without worker).
   * Syncs mount state, starts pending receives, and re-arms idle/stuck interrupt INs.
   */
  void serviceReceivesFromHostTask();

  // Called from TinyUSB HID callbacks (same translation unit as .cpp); do not use from user code.
  void _onMount(uint8_t dev_addr, uint8_t idx, uint8_t const *report_desc, uint16_t desc_len);
  void _onUnmount(uint8_t dev_addr, uint8_t idx);
  /** Drop every handler bound to this device address (tuh_umount_cb / missed iface umount). */
  void _onDeviceUnmount(uint8_t dev_addr);
  void _onReport(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len);

private:
  /**
   * After a new HID claim, optionally schedule abort+rearm of *peer* interfaces only
   * (hub enum can leave them busy-forever). Never aborts the newly claimed iface.
   */
  void requestPeerRearm(uint8_t new_dev_addr, uint8_t new_idx);
  bool hasOtherMounted(uint8_t dev_addr, uint8_t idx) const;
  void clearPendingIf(uint8_t dev_addr, uint8_t idx);
  /** Abort busy IN (if any) then notify handler — call while TinyUSB iface still open. */
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

/** Single global instance. Defined in USBHostHID.cpp when CFG_TUH_HID. */
extern USBHostHID USBHostHIDInstance;

/** Backward-compat alias for the global instance. */
#define USBHostHID USBHostHIDInstance
