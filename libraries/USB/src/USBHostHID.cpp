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

#if __has_include("tusb_config.h")
#include "tusb_config.h"
#endif

#include "USBHostHID.h"

#if CFG_TUH_HID

#include "tusb.h"
#include "class/hid/hid.h"
#include "Arduino.h"
#include "USBHost.h"
#include <string.h>

USBHostHIDClass::USBHostHIDClass()
  : _num_devices(0), _start_receive_pending(false), _pending_wait_enum(false), _pending_dev_addr(0), _pending_idx(0), _rearm_peers(false), _rearm_after_ms(0),
    _rearm_skip_addr(0), _rearm_skip_idx(0), _rearm_tries(0),
#if USBHOST_HID_DUMP_DESC
    _desc_dump_w(0), _desc_dump_r(0),
#endif
    _host_note_w(0), _host_note_r(0) {
  for (size_t i = 0; i < MAX_DEVICES; i++) {
    _devices[i] = nullptr;
  }
}

/* P4/S31 = baseline Y (no interrupt-IN abort; arm IN after tuh_mount_cb).
 * S2/S3 = baseline Q (peer rearm + abort-on-leave; arm IN after hid_mount_cb). */
static bool hidBaselineY(void) {
#if CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31
  return true;
#else
  return false;
#endif
}

/* Aborting a peer's interrupt IN is the only way back for one that a neighbour's enumeration
 * left stuck: without it a mouse that boots next to a keyboard stays silent until replugged.
 * Set to 0 to bisect a device that goes quiet right after an unrelated mount or unmount. */
#ifndef USBHOST_HID_XFER_ABORT
#define USBHOST_HID_XFER_ABORT 1
#endif

static bool hidXferAbortOk(void) {
  return USBHOST_HID_XFER_ABORT && !hidBaselineY();
}

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
static const char *hidSpeedStr(uint8_t speed) {
  switch (speed) {
    case TUSB_SPEED_LOW:  return "LS";
    case TUSB_SPEED_FULL: return "FS";
    case TUSB_SPEED_HIGH: return "HS";
    default:              return "?";
  }
}
#endif

bool USBHostHIDClass::hasOtherMounted(uint8_t dev_addr, uint8_t idx) const {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->mounted() && !dev->matches(dev_addr, idx)) {
      return true;
    }
  }
  return false;
}

void USBHostHIDClass::requestPeerRearm(uint8_t new_dev_addr, uint8_t new_idx) {
  if (!hidXferAbortOk()) {
    return; /* Baseline Y: never abort. */
  }
  if (!hasOtherMounted(new_dev_addr, new_idx)) {
    return; /* Sole device: startReceiveIfPending is enough; aborting it crashes DWC2. */
  }
  /* Defer the abort so it does not race hub enumeration in the same tuh_task window. */
  if (!_rearm_peers) {
    /* Keep the exemption of a request already in flight: it names the interface that claimed most
     * recently, which is the one that must not be aborted. An unmount passes the departing
     * interface, so losing that value costs nothing. */
    _rearm_skip_addr = new_dev_addr;
    _rearm_skip_idx = new_idx;
  }
  _rearm_peers = true;
  _rearm_after_ms = millis() + 150;
  _rearm_tries = 0;
}

bool USBHostHIDClass::addDevice(USBHostHIDDevice *dev) {
  if (dev == nullptr || _num_devices >= MAX_DEVICES) {
    return false;
  }
  _devices[_num_devices++] = dev;
  return true;
}

void USBHostHIDClass::startReceiveIfPending() {
  if (!_start_receive_pending || _pending_dev_addr == 0 || _pending_wait_enum) {
    return;
  }
  const uint8_t a = _pending_dev_addr;
  const uint8_t x = _pending_idx;
  if (!tuh_mounted(a) || !tuh_hid_mounted(a, x)) {
    _start_receive_pending = false;
    _pending_wait_enum = false;
    _pending_dev_addr = 0;
    _pending_idx = 0;
    return;
  }
  _start_receive_pending = false;
  _pending_dev_addr = 0;
  _pending_idx = 0;
  queueHostNote(NOTE_START_RX, a, x, 0, 0);
  (void)tuh_hid_receive_report(a, x);
}

void USBHostHIDClass::_onMount(uint8_t dev_addr, uint8_t idx, uint8_t const *report_desc, uint16_t desc_len) {
  const uint8_t protocol = tuh_hid_interface_protocol(dev_addr, idx);
#if USBHOST_HID_DUMP_DESC
  /* Copy only — log_buf_v here stalls usbhTuh (UART) and wedges DWC2 mid-enum. */
  queueDescDump(dev_addr, idx, protocol, report_desc, desc_len);
#endif

  bool claimed = false;
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->claim(dev_addr, idx, protocol, report_desc, desc_len)) {
      _start_receive_pending = true;
      /* Baseline Y only: on S2/S3 waiting for tuh_mount_cb deadlocks devices behind a hub. */
      _pending_wait_enum = hidBaselineY();
      _pending_dev_addr = dev_addr;
      _pending_idx = idx;
      claimed = true;
      queueHostNote(NOTE_MOUNT_CLAIMED, dev_addr, idx, protocol, desc_len);
      requestPeerRearm(dev_addr, idx);
      break;
    }
  }
  if (!claimed) {
    queueHostNote(NOTE_MOUNT_NONE, dev_addr, idx, protocol, desc_len);
  }
}

#if USBHOST_HID_DUMP_DESC
void USBHostHIDClass::queueDescDump(uint8_t dev_addr, uint8_t idx, uint8_t protocol, const uint8_t *desc, uint16_t len) {
  const uint8_t w = _desc_dump_w;
  const uint8_t n = (uint8_t)((w + 1u) % DESC_DUMP_SLOTS);
  if (n == _desc_dump_r) {
    return;
  }
  DescDumpSlot *s = &_desc_dump[w];
  s->dev_addr = dev_addr;
  s->idx = idx;
  s->protocol = protocol;
  uint16_t ncopy = len;
  if (ncopy > DESC_DUMP_CAP) {
    ncopy = DESC_DUMP_CAP;
  }
  s->len = ncopy;
  if (desc != nullptr && ncopy > 0) {
    memcpy(s->buf, desc, ncopy);
  } else {
    s->len = 0;
  }
  _desc_dump_w = n;
}

void USBHostHIDClass::flushDescDumps() {
  while (_desc_dump_r != _desc_dump_w) {
    const uint8_t r = _desc_dump_r;
    const DescDumpSlot *s = &_desc_dump[r];
    log_v(
      "[USBHostHID] report descriptor (deferred) dev=%u idx=%u protocol=%u len=%u", (unsigned)s->dev_addr, (unsigned)s->idx, (unsigned)s->protocol,
      (unsigned)s->len
    );
    if (s->len > 0) {
      log_buf_v(s->buf, s->len);
    }
    _desc_dump_r = (uint8_t)((r + 1u) % DESC_DUMP_SLOTS);
  }
}
#endif /* USBHOST_HID_DUMP_DESC */

void USBHostHIDClass::queueHostNote(uint8_t kind, uint8_t dev_addr, uint8_t idx, uint8_t protocol, uint16_t extra, uint16_t extra2) {
  const uint8_t w = _host_note_w;
  const uint8_t n = (uint8_t)((w + 1u) % HOST_NOTE_SLOTS);
  if (n == _host_note_r) {
    return;
  }
  HostNote *s = &_host_note[w];
  s->kind = kind;
  s->dev_addr = dev_addr;
  s->idx = idx;
  s->protocol = protocol;
  s->extra = extra;
  s->extra2 = extra2;
  _host_note_w = n;
}

void USBHostHIDClass::flushHostNotes() {
  while (_host_note_r != _host_note_w) {
    const uint8_t r = _host_note_r;
    const HostNote *s = &_host_note[r];
    switch (s->kind) {
      case NOTE_MOUNT_CLAIMED:
        log_v(
          "[USBHostHID] mount dev=%u idx=%u protocol=%u claimed len=%u", (unsigned)s->dev_addr, (unsigned)s->idx, (unsigned)s->protocol, (unsigned)s->extra
        );
        break;
      case NOTE_MOUNT_NONE:
        log_v(
          "[USBHostHID] mount dev=%u idx=%u protocol=%u (no handler) len=%u", (unsigned)s->dev_addr, (unsigned)s->idx, (unsigned)s->protocol, (unsigned)s->extra
        );
        break;
      case NOTE_UMOUNT_IF:  log_v("[USBHostHID] interface freed dev=%u idx=%u", (unsigned)s->dev_addr, (unsigned)s->idx); break;
      case NOTE_UMOUNT_DEV: log_v("[USBHost] device unmounted daddr=%u", (unsigned)s->dev_addr); break;
      case NOTE_START_RX:   log_v("[USBHostHID] start receive dev=%u idx=%u", (unsigned)s->dev_addr, (unsigned)s->idx); break;
      case NOTE_PEER_REARM: log_v("[USBHostHID] peer rearm abort+receive dev=%u idx=%u", (unsigned)s->dev_addr, (unsigned)s->idx); break;
      case NOTE_DEV_MOUNT:
        log_v(
          "[USBHost] device mounted daddr=%u vid=%04x pid=%04x speed=%s", (unsigned)s->dev_addr, (unsigned)s->extra, (unsigned)s->extra2,
          hidSpeedStr(s->protocol)
        );
        break;
      default: break;
    }
    _host_note_r = (uint8_t)((r + 1u) % HOST_NOTE_SLOTS);
  }
}

void USBHostHIDClass::clearPendingIf(uint8_t dev_addr, uint8_t idx) {
  if (_pending_dev_addr == dev_addr && _pending_idx == idx) {
    _start_receive_pending = false;
    _pending_wait_enum = false;
    _pending_dev_addr = 0;
    _pending_idx = 0;
  }
}

void USBHostHIDClass::releaseClaimedInterface(USBHostHIDDevice *dev, uint8_t dev_addr, uint8_t idx) {
  if (dev == nullptr) {
    return;
  }
  /* Free the DWC2 channel for the replug. Never on baseline Y: the abort faults on the P4 HS FIFO. */
  if (hidXferAbortOk() && tuh_hid_mounted(dev_addr, idx) && !tuh_hid_receive_ready(dev_addr, idx)) {
    (void)tuh_hid_receive_abort(dev_addr, idx);
  }
  if (_rearm_skip_addr == dev_addr && _rearm_skip_idx == idx) {
    _rearm_peers = false;
    _rearm_skip_addr = 0;
    _rearm_skip_idx = 0;
  }
  clearPendingIf(dev_addr, idx);
  queueHostNote(NOTE_UMOUNT_IF, dev_addr, idx, 0, 0);
  dev->onUnmount(dev_addr, idx);
  /* A removal strands a peer's interrupt IN just like a new claim does, and nothing else would
   * ever re-arm it — a keyboard left alone by an unplugged mouse stays silent until the next
   * device happens to arrive and sweep it. */
  requestPeerRearm(dev_addr, idx);
}

void USBHostHIDClass::_onUnmount(uint8_t dev_addr, uint8_t idx) {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->matches(dev_addr, idx)) {
      releaseClaimedInterface(dev, dev_addr, idx);
      return;
    }
  }
}

void USBHostHIDClass::_onDeviceUnmount(uint8_t dev_addr) {
  if (_pending_dev_addr == dev_addr) {
    _start_receive_pending = false;
    _pending_wait_enum = false;
    _pending_dev_addr = 0;
    _pending_idx = 0;
  }
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev == nullptr || dev->devAddr() != dev_addr) {
      continue;
    }
    releaseClaimedInterface(dev, dev_addr, dev->interfaceIndex());
  }
}

void USBHostHIDClass::_onTuhMount(uint8_t daddr) {
  uint16_t vid = 0;
  uint16_t pid = 0;
  (void)tuh_vid_pid_get(daddr, &vid, &pid);
  queueHostNote(NOTE_DEV_MOUNT, daddr, 0, (uint8_t)tuh_speed_get(daddr), vid, pid);
  if (_pending_dev_addr == daddr) {
    _pending_wait_enum = false;
  }
}

void USBHostHIDClass::_onReport(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len) {
  /* May see a short/failed IN during remove — never resubmit then. */
  if (!tuh_hid_mounted(dev_addr, idx)) {
    return;
  }
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->matches(dev_addr, idx)) {
      dev->onReport(dev_addr, idx, report, len);
      if (tuh_hid_mounted(dev_addr, idx) && tuh_hid_receive_ready(dev_addr, idx)) {
        (void)tuh_hid_receive_report(dev_addr, idx);
      }
      return;
    }
  }
}

extern "C" {

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *report_desc, uint16_t desc_len) {
  USBHostHID._onMount(dev_addr, idx, report_desc, desc_len);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx) {
  USBHostHID._onUnmount(dev_addr, idx);
}

void tuh_umount_cb(uint8_t daddr) {
  /* TinyUSB invokes this before class close / hcd_device_close: printing from here keeps the
   * interrupt-IN channels armed after the device is gone, which Store-faults the P4 FIFO. */
  USBHostHID.queueHostNote(USBHostHIDClass::NOTE_UMOUNT_DEV, daddr, 0, 0, 0);
  USBHostHID._onDeviceUnmount(daddr);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len) {
  USBHostHID._onReport(dev_addr, idx, report, len);
}

void arduino_usb_host_hid_service(void) {
  USBHostHID.serviceReceivesFromHostTask();
}

void arduino_usb_host_hid_dispatch(void) {
  USBHostHID.dispatchReportCallbacks();
}

void arduino_usb_host_hid_device_mounted(uint8_t daddr) {
  USBHostHID._onTuhMount(daddr);
}

}  // extern "C"

void USBHostHIDDevice::clearHidInterfaceBinding() {
  _itf_binding_valid = false;
  _itf_number = 0;
  _mounted = false;
  _dev_addr = 0;
  _idx = 0;
}

bool USBHostHIDDevice::bindHidInterface(uint8_t dev_addr, uint8_t idx) {
  if (_mounted && _itf_binding_valid && (_dev_addr != dev_addr || _idx != idx)) {
    return false;
  }
  tuh_itf_info_t info;
  if (!tuh_hid_itf_get_info(dev_addr, idx, &info)) {
    return false;
  }
  _dev_addr = dev_addr;
  _idx = idx;
  _itf_number = info.desc.bInterfaceNumber;
  _itf_binding_valid = true;
  _mounted = true;
  return true;
}

void USBHostHIDDevice::syncHostMountState() {
  if (!_mounted || !_itf_binding_valid || _dev_addr == 0) {
    return;
  }
  if (!tuh_mounted(_dev_addr) || !tuh_hid_mounted(_dev_addr, _idx)) {
    onUnmount(_dev_addr, _idx);
    return;
  }
  const uint8_t idx_now = tuh_hid_itf_get_index(_dev_addr, _itf_number);
  if (idx_now == TUSB_INDEX_INVALID_8 || idx_now != _idx) {
    onUnmount(_dev_addr, _idx);
  }
}

bool USBHostHIDDevice::mounted() const {
  return _mounted && _itf_binding_valid && _dev_addr != 0;
}

void USBHostHIDClass::serviceReceives() {
  if (USBHost.tuhBackgroundActive()) {
    return;
  }
  serviceReceivesFromHostTask();
}

void USBHostHIDClass::dispatchReportCallbacks() {
  flushHostNotes();
#if USBHOST_HID_DUMP_DESC
  flushDescDumps();
#endif
  for (size_t i = 0; i < _num_devices; i++) {
    if (_devices[i] != nullptr) {
      _devices[i]->dispatchReportCallback();
    }
  }
}

void USBHostHIDClass::serviceReceivesFromHostTask() {
  for (size_t i = 0; i < _num_devices; i++) {
    if (_devices[i] != nullptr) {
      _devices[i]->syncHostMountState();
    }
  }
  if (_pending_dev_addr != 0 && (!tuh_mounted(_pending_dev_addr) || !tuh_hid_mounted(_pending_dev_addr, _pending_idx))) {
    _start_receive_pending = false;
    _pending_wait_enum = false;
    _pending_dev_addr = 0;
    _pending_idx = 0;
  }
  startReceiveIfPending();

  bool rearm_peers = false;
  if (_rearm_peers && (int32_t)(millis() - _rearm_after_ms) >= 0) {
    rearm_peers = true;
    _rearm_peers = false;
  }
  bool rearm_unfinished = false;

  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev == nullptr || !dev->mounted()) {
      continue;
    }
    const uint8_t a = dev->devAddr();
    const uint8_t x = dev->interfaceIndex();
    if (!tuh_mounted(a) || !tuh_hid_mounted(a, x)) {
      continue;
    }
    /* Do not arm the new iface until tuh_mount_cb (Y / P4/S31 only). */
    if (_pending_wait_enum && a == _pending_dev_addr) {
      continue;
    }
    const bool is_new = (a == _rearm_skip_addr && x == _rearm_skip_idx);
    if (!tuh_hid_receive_ready(a, x)) {
      /* Abort only stuck peers — never the interface that just claimed/started IN. */
      if (!rearm_peers || is_new || !hidXferAbortOk()) {
        continue;
      }
      queueHostNote(NOTE_PEER_REARM, a, x, 0, 0);
      (void)tuh_hid_receive_abort(a, x);
      if (!tuh_mounted(a) || !tuh_hid_mounted(a, x) || !tuh_hid_receive_ready(a, x)) {
        /* The abort is asynchronous, so "still busy" here means nothing yet. Come back a few times
         * before giving up, otherwise a slow abort leaves the endpoint dead for good. */
        rearm_unfinished = true;
        continue;
      }
    }
    (void)tuh_hid_receive_report(a, x);
  }

  if (rearm_peers && rearm_unfinished && _rearm_tries < REARM_MAX_TRIES) {
    _rearm_tries++;
    _rearm_peers = true;
    _rearm_after_ms = millis() + REARM_RETRY_MS;
  }
}

USBHostHIDClass USBHostHID;

#endif /* CFG_TUH_HID */
