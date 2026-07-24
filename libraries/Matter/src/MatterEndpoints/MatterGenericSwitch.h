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

// Matter Generic Switch Endpoint — momentary smart button with optional gesture features
class MatterGenericSwitch : public MatterEndPoint {
public:
  // Switch cluster FeatureMap bits (Matter Switch cluster specification)
  static constexpr uint32_t FEATURE_MOMENTARY = 0x02;
  static constexpr uint32_t FEATURE_RELEASE = 0x04;
  static constexpr uint32_t FEATURE_LONG_PRESS = 0x08;
  static constexpr uint32_t FEATURE_MULTI_PRESS = 0x10;

  // Short click: InitialPress + ShortRelease
  static constexpr uint32_t FEATURE_SIMPLE = FEATURE_MOMENTARY | FEATURE_RELEASE;
  // All momentary gestures supported by this class
  static constexpr uint32_t FEATURE_ALL = FEATURE_SIMPLE | FEATURE_LONG_PRESS | FEATURE_MULTI_PRESS;

  MatterGenericSwitch();
  ~MatterGenericSwitch();

  // featureFlags: FEATURE_SIMPLE (default) or FEATURE_ALL; multiPressMax used when FEATURE_MULTI_PRESS is set (2–255)
  virtual bool begin(uint32_t featureFlags = FEATURE_SIMPLE, uint8_t multiPressMax = 5);
  void end();  // this will just stop processing Matter events

  bool hasFeature(uint32_t feature) const;

  // Matter Switch cluster events — call from your button driver
  void press();                            // InitialPress
  void release();                          // ShortRelease
  void longPress();                        // LongPress
  void longRelease();                      // LongRelease
  void multiPressOngoing(uint8_t count);   // MultiPressOngoing
  void multiPressComplete(uint8_t count);  // MultiPressComplete

  // Convenience: sends InitialPress and ShortRelease when release feature is enabled
  void click();

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  uint32_t featureFlags = FEATURE_SIMPLE;
  uint8_t multiPressMax = 5;
  static constexpr uint8_t pressPosition = 1;
  static constexpr uint8_t idlePosition = 0;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
