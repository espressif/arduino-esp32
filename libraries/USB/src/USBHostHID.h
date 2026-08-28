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

#if __has_include("tusb_config.h")
#include "tusb_config.h"
#endif
#ifndef CFG_TUH_HID
#define CFG_TUH_HID 0
#endif

#if CFG_TUH_ENABLED && CFG_TUH_HID

#include <stdint.h>
#include <stdbool.h>
#include <cstddef>

/* Report descriptor hex dump, off by default: -DUSBHOST_HID_DUMP_DESC=1. The pointer TinyUSB
 * hands to the mount callback is only valid there and printing from that context wedges DWC2,
 * so enabling it costs a copy of every descriptor (DESC_DUMP_SLOTS x DESC_DUMP_CAP). */
#ifndef USBHOST_HID_DUMP_DESC
#define USBHOST_HID_DUMP_DESC 0
#endif

/* TinyUSB / host-worker hooks are C linkage — declare before friend so we do not
 * introduce conflicting C++ linkage names in this header. */
extern "C" {
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *report_desc, uint16_t desc_len);
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx);
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len);
void tuh_umount_cb(uint8_t daddr);
void arduino_usb_host_hid_service(void);
void arduino_usb_host_hid_dispatch(void);
void arduino_usb_host_hid_device_mounted(uint8_t daddr);
}

class USBHostHIDClass;

/**
 * @brief Base class for USB Host HID handlers (mouse, keyboard, gamepad, …).
 *
 * Implement claim() / onUnmount() / onReport(), then register with USBHostHID.addDevice()
 * (or call registerWithHost() on the concrete handler) before USBHost.begin().
 */
class USBHostHIDDevice {
public:
  USBHostHIDDevice() : _dev_addr(0), _idx(0), _mounted(false), _itf_binding_valid(false), _itf_number(0) {}
  virtual ~USBHostHIDDevice() {}

  /** Return true to claim this HID interface (first matching handler wins). */
  virtual bool claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol, const uint8_t *report_desc, uint16_t desc_len) = 0;

  virtual void onUnmount(uint8_t dev_addr, uint8_t idx) = 0;

  /**
   * INPUT report for this interface.
   * Do not call tuh_hid_receive_report() here — USBHostHID resubmits after return.
   * Keep this fast: no Serial / FS. User report callbacks run from USBHost.task().
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
  /** Invoke queued setReportCallback() from USBHost.task() (loop context). */
  virtual void dispatchReportCallback() {}

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
  friend void arduino_usb_host_hid_dispatch(void);
  friend void arduino_usb_host_hid_device_mounted(uint8_t);

  void _onMount(uint8_t dev_addr, uint8_t idx, uint8_t const *report_desc, uint16_t desc_len);
  void _onUnmount(uint8_t dev_addr, uint8_t idx);
  void _onDeviceUnmount(uint8_t dev_addr);
  void _onTuhMount(uint8_t daddr);
  void _onReport(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len);
  void dispatchReportCallbacks();

  void serviceReceivesFromHostTask();
  void startReceiveIfPending();
  void requestPeerRearm(uint8_t new_dev_addr, uint8_t new_idx);
  bool hasOtherMounted(uint8_t dev_addr, uint8_t idx) const;
  void clearPendingIf(uint8_t dev_addr, uint8_t idx);
  void releaseClaimedInterface(USBHostHIDDevice *dev, uint8_t dev_addr, uint8_t idx);
#if USBHOST_HID_DUMP_DESC
  void queueDescDump(uint8_t dev_addr, uint8_t idx, uint8_t protocol, const uint8_t *desc, uint16_t len);
  void flushDescDumps();
#endif
  void queueHostNote(uint8_t kind, uint8_t dev_addr, uint8_t idx, uint8_t protocol, uint16_t extra, uint16_t extra2 = 0);
  void flushHostNotes();

  static const size_t MAX_DEVICES = 8;
  static const uint8_t REARM_MAX_TRIES = 5;
  static const uint32_t REARM_RETRY_MS = 50;
#if USBHOST_HID_DUMP_DESC
  static const uint8_t DESC_DUMP_SLOTS = 4;
  static const uint16_t DESC_DUMP_CAP = 256;
#endif
  static const uint8_t HOST_NOTE_SLOTS = 8;
  enum HostNoteKind : uint8_t {
    NOTE_MOUNT_CLAIMED = 1,
    NOTE_MOUNT_NONE = 2,
    NOTE_UMOUNT_IF = 3,
    NOTE_UMOUNT_DEV = 4,
    NOTE_START_RX = 5,
    NOTE_PEER_REARM = 6,
    NOTE_DEV_MOUNT = 7,
  };
#if USBHOST_HID_DUMP_DESC
  struct DescDumpSlot {
    uint8_t dev_addr;
    uint8_t idx;
    uint8_t protocol;
    uint16_t len;
    uint8_t buf[DESC_DUMP_CAP];
  };
#endif
  /** Deferred log line. NOTE_DEV_MOUNT reuses protocol/extra/extra2 as speed/vid/pid. */
  struct HostNote {
    uint8_t kind;
    uint8_t dev_addr;
    uint8_t idx;
    uint8_t protocol;
    uint16_t extra;
    uint16_t extra2;
  };
  USBHostHIDDevice *_devices[MAX_DEVICES];
  size_t _num_devices;
  bool _start_receive_pending;
  bool _pending_wait_enum;
  uint8_t _pending_dev_addr;
  uint8_t _pending_idx;
  bool _rearm_peers;
  uint32_t _rearm_after_ms;
  uint8_t _rearm_skip_addr;
  uint8_t _rearm_skip_idx;
  uint8_t _rearm_tries;
#if USBHOST_HID_DUMP_DESC
  DescDumpSlot _desc_dump[DESC_DUMP_SLOTS];
  volatile uint8_t _desc_dump_w;
  volatile uint8_t _desc_dump_r;
#endif
  HostNote _host_note[HOST_NOTE_SLOTS];
  volatile uint8_t _host_note_w;
  volatile uint8_t _host_note_r;
};

extern USBHostHIDClass USBHostHID;

#endif /* CFG_TUH_ENABLED && CFG_TUH_HID */
#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
