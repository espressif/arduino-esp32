// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

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

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"

#if CONFIG_TINYUSB_MSC_ENABLED

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
typedef bool (*msc_start_stop_cb)(uint8_t power_condition, bool start, bool load_eject);

// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
typedef int32_t (*msc_read_cb)(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);

// Process data in buffer to disk's storage and return number of written bytes
typedef int32_t (*msc_write_cb)(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize);

class USBMSC {
public:
  USBMSC();
  ~USBMSC();
  bool begin(uint32_t block_count, uint16_t block_size);
  void end();
  void vendorID(const char* vid);         //max 8 chars
  void productID(const char* pid);        //max 16 chars
  void productRevision(const char* ver);  //max 4 chars
  void mediaPresent(bool media_present);
  void onStartStop(msc_start_stop_cb cb);
  void onRead(msc_read_cb cb);
  void onWrite(msc_write_cb cb);
private:
  uint8_t _lun;
};

#endif /* CONFIG_TINYUSB_MSC_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
