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
#include <MatterEndpoints/MatterWaterValve.h>
#include <app/clusters/valve-configuration-and-control-server/valve-configuration-and-control-cluster.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;
namespace wv_endpoint = esp_matter::endpoint::water_valve;

// Bridges the CHIP ValveConfigurationAndControl::Delegate interface to the MatterWaterValve user callbacks.
// Its methods are always invoked with the CHIP stack lock held - either implicitly (remote commands are
// dispatched on the Matter/CHIP event loop thread) or explicitly (local open()/close() calls take the lock
// via lock::ScopedChipStackLock before calling into the free functions below) - so it's safe for them to
// call back into e.g. ValveConfigurationAndControl::UpdateCurrentState() synchronously.
class MatterWaterValve::ValveDelegate : public ValveConfigurationAndControl::Delegate {
public:
  explicit ValveDelegate(MatterWaterValve *owner) : owner(owner) {}

  chip::app::DataModel::Nullable<chip::Percent> HandleOpenValve(chip::app::DataModel::Nullable<chip::Percent> level) override {
    (void)level;  // the Level feature is not enabled on this endpoint
    bool ok = true;
    if (owner->_onOpenCB != NULL) {
      ok = owner->_onOpenCB();
    }
    owner->targetState = MatterWaterValve::VALVE_STATE_OPEN;
    if (ok) {
      owner->currentState = MatterWaterValve::VALVE_STATE_OPEN;
      ValveConfigurationAndControl::UpdateCurrentState(owner->getEndPointId(), ValveConfigurationAndControl::ValveStateEnum::kOpen);
    } else {
      log_e("Water Valve onOpen() callback reported failure.");
    }
    return chip::app::DataModel::Nullable<chip::Percent>();
  }

  CHIP_ERROR HandleCloseValve() override {
    if (owner->_onCloseCB != NULL) {
      owner->_onCloseCB();
    }
    owner->currentState = MatterWaterValve::VALVE_STATE_CLOSED;
    owner->targetState = MatterWaterValve::VALVE_STATE_CLOSED;
    owner->openDuration = 0;
    owner->remainingDuration = 0;

    ValveConfigurationAndControl::UpdateCurrentState(owner->getEndPointId(), ValveConfigurationAndControl::ValveStateEnum::kClosed);
    return CHIP_NO_ERROR;
  }

  void HandleRemainingDurationTick(uint32_t duration) override {
    // The first tick of a countdown reports the full requested duration - capture it as openDuration too,
    // since a remotely-requested Open command doesn't otherwise tell us what duration was assigned.
    if (owner->remainingDuration == 0 && duration > 0) {
      owner->openDuration = duration;
    }
    owner->remainingDuration = duration;
  }

private:
  MatterWaterValve *owner;
};

MatterWaterValve::MatterWaterValve() {}

MatterWaterValve::~MatterWaterValve() {
  end();
}

bool MatterWaterValve::begin(uint32_t defaultOpenDurationSeconds) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Water Valve with Endpoint Id %u device has already been created.", getEndPointId());
    return false;
  }

  delegate = new (std::nothrow) ValveDelegate(this);
  if (delegate == nullptr) {
    log_e("Failed to allocate Water Valve delegate");
    return false;
  }

  defaultOpenDuration = defaultOpenDurationSeconds;

  wv_endpoint::config_t water_valve_config;
  water_valve_config.valve_configuration_and_control.current_state = nullable<uint8_t>((uint8_t)VALVE_STATE_CLOSED);
  water_valve_config.valve_configuration_and_control.target_state = nullable<uint8_t>();
  water_valve_config.valve_configuration_and_control.open_duration = nullable<uint32_t>();
  water_valve_config.valve_configuration_and_control.default_open_duration =
    (defaultOpenDurationSeconds == 0) ? nullable<uint32_t>() : nullable<uint32_t>(defaultOpenDurationSeconds);
  water_valve_config.valve_configuration_and_control.delegate = (void *)delegate;

  endpoint_t *endpoint = wv_endpoint::create(node::get(), &water_valve_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Water Valve endpoint");
    delete delegate;
    delegate = nullptr;
    return false;
  }

  setEndPointId(endpoint::get_id(endpoint));
  log_i("Water Valve created with endpoint_id %u", getEndPointId());

  currentState = VALVE_STATE_CLOSED;
  targetState = VALVE_STATE_CLOSED;
  openDuration = 0;
  remainingDuration = 0;
  valveFault = 0;
  started = true;

  return true;
}

void MatterWaterValve::end() {
  started = false;
}

bool MatterWaterValve::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  // CurrentState/TargetState/OpenDuration/RemainingDuration are still plain ember attributes on this cluster,
  // but they're only ever written by the free functions below (in response to the ValveDelegate callbacks
  // above), which already keep currentState/targetState/openDuration/remainingDuration in sync directly.
  // Nothing extra to do here.
  return true;
}

bool MatterWaterValve::open() {
  return open(defaultOpenDuration);
}

bool MatterWaterValve::open(uint32_t durationSeconds) {
  if (!started) {
    log_e("Matter Water Valve device has not begun.");
    return false;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);

  chip::app::DataModel::Nullable<uint32_t> duration =
    (durationSeconds == 0) ? chip::app::DataModel::Nullable<uint32_t>() : chip::app::DataModel::Nullable<uint32_t>(durationSeconds);
  // SetValveLevel() is the Open command's implementation: it synchronously invokes the ValveDelegate above,
  // which commits currentState/targetState (and, via HandleRemainingDurationTick, openDuration/remainingDuration)
  // once the onOpen() callback confirms. The Level feature is not enabled on this endpoint, so level is always null.
  if (ValveConfigurationAndControl::SetValveLevel(getEndPointId(), chip::app::DataModel::Nullable<chip::Percent>(), duration) != CHIP_NO_ERROR) {
    log_e("Failed to open Water Valve.");
    return false;
  }
  return true;
}

bool MatterWaterValve::close() {
  if (!started) {
    log_e("Matter Water Valve device has not begun.");
    return false;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);

  // CloseValve() synchronously invokes the ValveDelegate above, which commits currentState/targetState.
  if (ValveConfigurationAndControl::CloseValve(getEndPointId()) != CHIP_NO_ERROR) {
    log_e("Failed to close Water Valve.");
    return false;
  }
  return true;
}

bool MatterWaterValve::setValveFault(uint16_t fault) {
  if (!started) {
    log_e("Matter Water Valve device has not begun.");
    return false;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);

  chip::BitMask<ValveConfigurationAndControl::ValveFaultBitmap> faultBitmap(fault);
  // Persist the fault in the ValveFault attribute too (not just the event below): the cluster's own Open
  // command handler checks this attribute and rejects Open with FailureDueToFault while any bit is set.
  ValveConfigurationAndControl::Attributes::ValveFault::Set(getEndPointId(), faultBitmap);
  ValveConfigurationAndControl::EmitValveFault(getEndPointId(), faultBitmap);
  valveFault = fault;
  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
