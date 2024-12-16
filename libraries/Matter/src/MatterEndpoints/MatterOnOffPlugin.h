// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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

class MatterOnOffPlugin : public MatterEndPoint {
public:
  MatterOnOffPlugin();
  ~MatterOnOffPlugin();
  virtual bool begin(bool initialState = false);  // default initial state is off
  void end();                                     // this will just stop processing Plugin Matter events

  bool setOnOff(bool newState);  // returns true if successful
  bool getOnOff();               // returns current plugin state
  bool toggle();                 // returns true if successful

  // User Callback for whenever the Plugin state is changed by the Matter Controller
  using EndPointCB = std::function<bool(bool)>;
  void onChange(EndPointCB onChangeCB) {
    _onChangeCB = onChangeCB;
  }

  // User Callback for whenever the On/Off state is changed by the Matter Controller
  void onChangeOnOff(EndPointCB onChangeCB) {
    _onChangeOnOffCB = onChangeCB;
  }

  // used to update the state of the plugin using the current Matter Plugin internal state
  // It is necessary to set a user callback function using onChange() to handle the physical plugin state
  void updateAccessory();

  operator bool();             // returns current plugin state
  void operator=(bool state);  // turns plugin on or off

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  bool onOffState = false;  // default initial state is off, but it can be changed by begin(bool)
  EndPointCB _onChangeCB = NULL;
  EndPointCB _onChangeOnOffCB = NULL;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
