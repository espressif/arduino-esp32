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

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <app/server/Server.h>
#include <MatterEndpoints/MatterWaterLeakDetector.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

bool MatterWaterLeakDetector::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Water Leak Detector device has not begun.");
    return false;
  }

  log_d("Water Leak Detector Attr update callback: endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32 ", val: %" PRIu32, endpoint_id, cluster_id, attribute_id, val->val.u32);
  return ret;
}

MatterWaterLeakDetector::MatterWaterLeakDetector() {}

MatterWaterLeakDetector::~MatterWaterLeakDetector() {
  end();
}

bool MatterWaterLeakDetector::begin(bool _leakState) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Water Leak Detector with Endpoint Id %u device has already been created.", getEndPointId());
    return false;
  }

  water_leak_detector::config_t water_leak_detector_config;
  water_leak_detector_config.boolean_state.state_value = _leakState;

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = water_leak_detector::create(node::get(), &water_leak_detector_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Water Leak Detector endpoint");
    return false;
  }
  leakState = _leakState;
  setEndPointId(endpoint::get_id(endpoint));
  log_i("Water Leak Detector created with endpoint_id %u", getEndPointId());

  started = true;
  return true;
}

void MatterWaterLeakDetector::end() {
  started = false;
}

bool MatterWaterLeakDetector::setLeak(bool _leakState) {
  if (!started) {
    log_e("Matter Water Leak Detector device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (leakState == _leakState) {
    return true;
  }

  esp_matter_attr_val_t leakVal = esp_matter_invalid(NULL);

  if (!getAttributeVal(BooleanState::Id, BooleanState::Attributes::StateValue::Id, &leakVal)) {
    log_e("Failed to get Water Leak Detector Attribute.");
    return false;
  }
  if (leakVal.val.u8 != _leakState) {
    leakVal.val.u8 = _leakState;
    bool ret;
    ret = updateAttributeVal(BooleanState::Id, BooleanState::Attributes::StateValue::Id, &leakVal);
    if (!ret) {
      log_e("Failed to update Water Leak Detector Attribute.");
      return false;
    }
    leakState = _leakState;
  }
  log_v("Water Leak Detector set to %s", _leakState ? "Detected" : "Not Detected");

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
