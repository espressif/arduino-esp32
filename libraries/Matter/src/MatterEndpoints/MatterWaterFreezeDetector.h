// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndPoint.h>

class MatterWaterFreezeDetector : public MatterEndPoint {
public:
  MatterWaterFreezeDetector();
  ~MatterWaterFreezeDetector();
  // begin Matter Water Freeze Detector endpoint with initial freeze state
  bool begin(bool _freezeState = false);
  // this will just stop processing Water Freeze Detector Matter events
  void end();

  // set the freeze state
  bool setFreeze(bool _freezeState);
  // returns the freeze state
  bool getFreeze() {
    return freezeState;
  }

  // bool conversion operator
  void operator=(bool _freezeState) {
    setFreeze(_freezeState);
  }
  // bool conversion operator
  operator bool() {
    return getFreeze();
  }

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  bool freezeState = false;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */

