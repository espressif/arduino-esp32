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

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <app/server/Server.h>
#include <MatterEndpoints/MatterGenericSwitch.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace chip::app::Clusters;

MatterGenericSwitch::MatterGenericSwitch() {}

MatterGenericSwitch::~MatterGenericSwitch() {
  end();
}

bool MatterGenericSwitch::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  if (!started) {
    log_e("Matter Generic Switch device has not begun.");
    return false;
  }

  log_d("Generic Switch Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);
  return true;
}

bool MatterGenericSwitch::begin() {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Generic Switch with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  generic_switch::config_t switch_config;
  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = generic_switch::create(node::get(), &switch_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Generic Switch endpoint");
    return false;
  }
  // Add group cluster to the switch endpoint
  cluster::groups::config_t groups_config;
  cluster::groups::create(endpoint, &groups_config, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_CLIENT);

  cluster_t *aCluster = cluster::get(endpoint, Descriptor::Id);
  esp_matter::cluster::descriptor::feature::taglist::add(aCluster);

  cluster::fixed_label::config_t fl_config;
  cluster::fixed_label::create(endpoint, &fl_config, CLUSTER_FLAG_SERVER);

  cluster::user_label::config_t ul_config;
  cluster::user_label::create(endpoint, &ul_config, CLUSTER_FLAG_SERVER);

  aCluster = cluster::get(endpoint, Switch::Id);
  switch_cluster::feature::momentary_switch::add(aCluster);
  switch_cluster::event::create_initial_press(aCluster);

  switch_cluster::feature::momentary_switch::add(aCluster);

  switch_cluster::attribute::create_current_position(aCluster, 0);
  switch_cluster::attribute::create_number_of_positions(aCluster, 2);

  setEndPointId(endpoint::get_id(endpoint));
  log_i("Generic Switch created with endpoint_id %d", getEndPointId());
  started = true;
  return true;
}

void MatterGenericSwitch::end() {
  started = false;
}

void MatterGenericSwitch::click() {
  if (!started) {
    log_e("Matter Generic Switch device has not begun.");
    return;
  }

  int switch_endpoint_id = getEndPointId();
  uint8_t newPosition = 1;
  // Press moves Position from 0 (off) to 1 (on)
  chip::DeviceLayer::SystemLayer().ScheduleLambda([switch_endpoint_id, newPosition]() {
    // InitialPress event takes newPosition as event data
    switch_cluster::event::send_initial_press(switch_endpoint_id, newPosition);
  });
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
