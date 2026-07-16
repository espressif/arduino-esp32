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
#include <MatterEndpoints/MatterGenericSwitch.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace chip::app::Clusters;

namespace {
void setCurrentPosition(uint16_t endpoint_id, uint8_t position) {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  val.type = ESP_MATTER_VAL_TYPE_UINT8;
  val.val.u8 = position;
  attribute::update(endpoint_id, Switch::Id, Switch::Attributes::CurrentPosition::Id, &val);
}
}  // namespace

MatterGenericSwitch::MatterGenericSwitch() {}

MatterGenericSwitch::~MatterGenericSwitch() {
  end();
}

bool MatterGenericSwitch::hasFeature(uint32_t feature) const {
  return (featureFlags & feature) == feature;
}

bool MatterGenericSwitch::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  if (!started) {
    log_e("Matter Generic Switch device has not begun.");
    return false;
  }

  log_d(
    "Generic Switch Attr update callback: endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32 ", val: %" PRIu32, endpoint_id, cluster_id, attribute_id,
    val->val.u32
  );
  return true;
}

bool MatterGenericSwitch::begin(uint32_t featureFlags, uint8_t multiPressMax) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Generic Switch with Endpoint Id %u device has already been created.", getEndPointId());
    return false;
  }

  if ((featureFlags & FEATURE_MOMENTARY) == 0) {
    log_e("Generic Switch requires FEATURE_MOMENTARY.");
    return false;
  }

  if ((featureFlags & FEATURE_LONG_PRESS) && !(featureFlags & FEATURE_RELEASE)) {
    log_e("FEATURE_LONG_PRESS requires FEATURE_RELEASE.");
    return false;
  }

  if (featureFlags & FEATURE_MULTI_PRESS) {
    if (!(featureFlags & FEATURE_RELEASE)) {
      log_e("FEATURE_MULTI_PRESS requires FEATURE_RELEASE.");
      return false;
    }
    if (multiPressMax < 2) {
      log_w("multiPressMax should be at least 2; using 2.");
      multiPressMax = 2;
    }
  }

  this->featureFlags = featureFlags;
  this->multiPressMax = multiPressMax;

  generic_switch::config_t switch_config;
  switch_config.switch_cluster.number_of_positions = 2;
  switch_config.switch_cluster.current_position = idlePosition;
  switch_config.switch_cluster.feature_flags = featureFlags;
  if (featureFlags & FEATURE_MULTI_PRESS) {
    switch_config.switch_cluster.features.momentary_switch_multi_press.multi_press_max = multiPressMax;
  }

  endpoint_t *endpoint = generic_switch::create(node::get(), &switch_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Generic Switch endpoint");
    return false;
  }

  cluster::groups::config_t groups_config;
  cluster::groups::create(endpoint, &groups_config, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_CLIENT);

  cluster_t *aCluster = cluster::get(endpoint, Descriptor::Id);
  esp_matter::cluster::descriptor::feature::tag_list::add(aCluster);

  cluster::fixed_label::config_t fl_config;
  cluster::fixed_label::create(endpoint, &fl_config, CLUSTER_FLAG_SERVER);

  cluster::user_label::config_t ul_config;
  cluster::user_label::create(endpoint, &ul_config, CLUSTER_FLAG_SERVER);

  setEndPointId(endpoint::get_id(endpoint));
  log_i("Generic Switch created with endpoint_id %u (feature_flags=0x%02" PRIX32 ")", getEndPointId(), featureFlags);

  started = true;
  return true;
}

void MatterGenericSwitch::end() {
  started = false;
}

void MatterGenericSwitch::press() {
  if (!started) {
    log_e("Matter Generic Switch device has not begun.");
    return;
  }
  if (!hasFeature(FEATURE_MOMENTARY)) {
    log_w("InitialPress not enabled in feature flags.");
    return;
  }

  int switch_endpoint_id = getEndPointId();
  chip::DeviceLayer::SystemLayer().ScheduleLambda([switch_endpoint_id]() {
    setCurrentPosition(static_cast<uint16_t>(switch_endpoint_id), pressPosition);
    switch_cluster::event::send_initial_press(switch_endpoint_id, pressPosition);
  });
}

void MatterGenericSwitch::release() {
  if (!started) {
    log_e("Matter Generic Switch device has not begun.");
    return;
  }
  if (!hasFeature(FEATURE_RELEASE)) {
    log_w("ShortRelease not enabled in feature flags.");
    return;
  }

  int switch_endpoint_id = getEndPointId();
  chip::DeviceLayer::SystemLayer().ScheduleLambda([switch_endpoint_id]() {
    setCurrentPosition(static_cast<uint16_t>(switch_endpoint_id), idlePosition);
    switch_cluster::event::send_short_release(switch_endpoint_id, pressPosition);
  });
}

void MatterGenericSwitch::longPress() {
  if (!started) {
    log_e("Matter Generic Switch device has not begun.");
    return;
  }
  if (!hasFeature(FEATURE_LONG_PRESS)) {
    log_w("LongPress not enabled in feature flags.");
    return;
  }

  int switch_endpoint_id = getEndPointId();
  chip::DeviceLayer::SystemLayer().ScheduleLambda([switch_endpoint_id]() {
    switch_cluster::event::send_long_press(switch_endpoint_id, pressPosition);
  });
}

void MatterGenericSwitch::longRelease() {
  if (!started) {
    log_e("Matter Generic Switch device has not begun.");
    return;
  }
  if (!hasFeature(FEATURE_LONG_PRESS)) {
    log_w("LongRelease not enabled in feature flags.");
    return;
  }

  int switch_endpoint_id = getEndPointId();
  chip::DeviceLayer::SystemLayer().ScheduleLambda([switch_endpoint_id]() {
    setCurrentPosition(static_cast<uint16_t>(switch_endpoint_id), idlePosition);
    switch_cluster::event::send_long_release(switch_endpoint_id, pressPosition);
  });
}

void MatterGenericSwitch::multiPressOngoing(uint8_t count) {
  if (!started) {
    log_e("Matter Generic Switch device has not begun.");
    return;
  }
  if (!hasFeature(FEATURE_MULTI_PRESS)) {
    log_w("MultiPressOngoing not enabled in feature flags.");
    return;
  }

  int switch_endpoint_id = getEndPointId();
  chip::DeviceLayer::SystemLayer().ScheduleLambda([switch_endpoint_id, count]() {
    setCurrentPosition(static_cast<uint16_t>(switch_endpoint_id), pressPosition);
    switch_cluster::event::send_multi_press_ongoing(switch_endpoint_id, pressPosition, count);
  });
}

void MatterGenericSwitch::multiPressComplete(uint8_t count) {
  if (!started) {
    log_e("Matter Generic Switch device has not begun.");
    return;
  }
  if (!hasFeature(FEATURE_MULTI_PRESS)) {
    log_w("MultiPressComplete not enabled in feature flags.");
    return;
  }

  if (count > multiPressMax) {
    count = 0;
  }

  int switch_endpoint_id = getEndPointId();
  chip::DeviceLayer::SystemLayer().ScheduleLambda([switch_endpoint_id, count]() {
    setCurrentPosition(static_cast<uint16_t>(switch_endpoint_id), idlePosition);
    switch_cluster::event::send_multi_press_complete(switch_endpoint_id, pressPosition, count);
  });
}

void MatterGenericSwitch::click() {
  press();
  if (hasFeature(FEATURE_RELEASE)) {
    release();
  }
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
