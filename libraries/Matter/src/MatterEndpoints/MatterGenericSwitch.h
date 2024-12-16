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

// Matter Generic Switch Endpoint that works as a single click smart button
class MatterGenericSwitch : public MatterEndPoint {
public:
  MatterGenericSwitch();
  ~MatterGenericSwitch();
  virtual bool begin();
  void end();  // this will just stop processing Matter events

  // send a simple click event to the Matter Controller
  void click();

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);
  // this function is invoked when clients interact with the Identify Cluster of an specific endpoint
  bool endpointIdentifyCB(uint16_t endpoint_id, bool identifyIsEnabled, uint8_t identifyCounter) {
    if (_onEndPointIdentifyCB) {
      return _onEndPointIdentifyCB(identifyIsEnabled, identifyCounter);
    }
    return true;
  }
  // User callaback for the Identify Cluster functionality
  using EndPointIdentifyCB = std::function<bool(bool, uint8_t)>;
  void onIdentify(EndPointIdentifyCB onEndPointIdentifyCB) {
    _onEndPointIdentifyCB = onEndPointIdentifyCB;
  }

protected:
  bool started = false;
  EndPointIdentifyCB _onEndPointIdentifyCB = NULL;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
