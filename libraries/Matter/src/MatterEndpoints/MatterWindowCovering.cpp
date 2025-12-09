// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndpoints/MatterWindowCovering.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace esp_matter::cluster::window_covering;
using namespace esp_matter::cluster::window_covering::command;
using namespace esp_matter::cluster::window_covering::feature;
using namespace chip::app::Clusters;

MatterWindowCovering::MatterWindowCovering() {}

MatterWindowCovering::~MatterWindowCovering() {
  end();
}

bool MatterWindowCovering::begin(uint8_t liftPercent, uint8_t tiltPercent, WindowCoveringType_t _coveringType) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Window Covering with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  coveringType = (_coveringType == 0) ? ROLLERSHADE : _coveringType;

  window_covering_device::config_t window_covering_config(0);
  window_covering_config.window_covering.type = (uint8_t)coveringType;
  window_covering_config.window_covering.config_status = 0;
  window_covering_config.window_covering.operational_status = 0;

  currentLiftPercent = liftPercent;
  currentTiltPercent = tiltPercent;
  currentLiftPosition = 0;
  currentTiltPosition = 0;

  endpoint_t *endpoint = window_covering_device::create(node::get(), &window_covering_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create window covering endpoint");
    return false;
  }

  setEndPointId(endpoint::get_id(endpoint));
  log_i("Window Covering created with endpoint_id %d", getEndPointId());

  // Get the Window Covering cluster and add features and commands
  cluster_t *window_covering_cluster = cluster::get(endpoint, WindowCovering::Id);
  if (window_covering_cluster != nullptr) {
    // Add Lift feature
    feature::lift::config_t lift_config;
    lift_config.number_of_actuations_lift = 0;
    if (feature::lift::add(window_covering_cluster, &lift_config) != ESP_OK) {
      log_e("Failed to add Lift feature");
    }

    // Add Position Aware Lift feature
    feature::position_aware_lift::config_t position_aware_lift_config;
    position_aware_lift_config.current_position_lift_percentage = nullable<uint8_t>(0);
    position_aware_lift_config.target_position_lift_percent_100ths = nullable<uint16_t>(liftPercent * 100);
    position_aware_lift_config.current_position_lift_percent_100ths = nullable<uint16_t>(liftPercent * 100);
    if (feature::position_aware_lift::add(window_covering_cluster, &position_aware_lift_config) != ESP_OK) {
      log_e("Failed to add Position Aware Lift feature");
    }

    // Add Tilt feature if the covering type supports it
    bool supportsTilt = (coveringType == SHUTTER || coveringType == BLIND_TILT_ONLY || coveringType == BLIND_LIFT_AND_TILT);
    if (supportsTilt) {
      feature::tilt::config_t tilt_config;
      tilt_config.number_of_actuations_tilt = 0;
      if (feature::tilt::add(window_covering_cluster, &tilt_config) != ESP_OK) {
        log_e("Failed to add Tilt feature");
      }

      // Add Position Aware Tilt feature
      feature::position_aware_tilt::config_t position_aware_tilt_config;
      position_aware_tilt_config.current_position_tilt_percentage = nullable<uint8_t>(0);
      position_aware_tilt_config.target_position_tilt_percent_100ths = nullable<uint16_t>(tiltPercent * 100);
      position_aware_tilt_config.current_position_tilt_percent_100ths = nullable<uint16_t>(tiltPercent * 100);
      if (feature::position_aware_tilt::add(window_covering_cluster, &position_aware_tilt_config) != ESP_OK) {
        log_e("Failed to add Position Aware Tilt feature");
      }
    }

    // Add Absolute Position feature (creates InstalledOpenLimitLift/ClosedLimitLift/Tilt attributes)
    // Must be added AFTER all lift and tilt features for all attributes to be created
    feature::absolute_position::config_t absolute_position_config;
    absolute_position_config.installed_open_limit_lift = 0;
    absolute_position_config.installed_closed_limit_lift = 65534;
    absolute_position_config.installed_open_limit_tilt = 0;
    absolute_position_config.installed_closed_limit_tilt = 65534;
    if (feature::absolute_position::add(window_covering_cluster, &absolute_position_config) != ESP_OK) {
      log_e("Failed to add Absolute Position feature");
    }

    // Create Window Covering commands
    create_up_or_open(window_covering_cluster);
    create_down_or_close(window_covering_cluster);
    create_stop_motion(window_covering_cluster);
    create_go_to_lift_value(window_covering_cluster);
    create_go_to_lift_percentage(window_covering_cluster);
    if (supportsTilt) {
      create_go_to_tilt_value(window_covering_cluster);
      create_go_to_tilt_percentage(window_covering_cluster);
    }
  } else {
    log_e("Failed to get Window Covering cluster for feature and command creation");
  }

  started = true;

  // Set initial lift and tilt percentages
  if (liftPercent > 0) {
    setLiftPercentage(liftPercent);
  }
  if (tiltPercent > 0) {
    setTiltPercentage(tiltPercent);
  }

  return true;
}

void MatterWindowCovering::end() {
  started = false;
}

bool MatterWindowCovering::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  log_d("Window Covering Attr update callback: endpoint: %u, cluster: %u, attribute: %u", endpoint_id, cluster_id, attribute_id);

  if (endpoint_id == getEndPointId() && cluster_id == WindowCovering::Id) {
    switch (attribute_id) {
      // Current position attributes (read-only to external Matter controllers; updated internally by device)
      case WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id:
      {
        uint16_t liftPercent100ths = val->val.u16;
        uint8_t liftPercent = (uint8_t)(liftPercent100ths / 100);
        log_d("Window Covering Lift Percentage changed to %d%%", liftPercent);
        if (currentLiftPercent != liftPercent) {
          if (_onChangeCB != NULL) {
            ret &= _onChangeCB(liftPercent, currentTiltPercent);
          }
          if (ret == true) {
            currentLiftPercent = liftPercent;
          }
        }
        break;
      }
      case WindowCovering::Attributes::CurrentPositionTiltPercent100ths::Id:
      {
        uint16_t tiltPercent100ths = val->val.u16;
        uint8_t tiltPercent = (uint8_t)(tiltPercent100ths / 100);
        log_d("Window Covering Tilt Percentage changed to %d%%", tiltPercent);
        if (currentTiltPercent != tiltPercent) {
          if (_onChangeCB != NULL) {
            ret &= _onChangeCB(currentLiftPercent, tiltPercent);
          }
          if (ret == true) {
            currentTiltPercent = tiltPercent;
          }
        }
        break;
      }
      case WindowCovering::Attributes::CurrentPositionLift::Id:
        log_d("Window Covering Lift Position changed to %d", val->val.u16);
        currentLiftPosition = val->val.u16;
        break;
      case WindowCovering::Attributes::CurrentPositionTilt::Id:
        log_d("Window Covering Tilt Position changed to %d", val->val.u16);
        currentTiltPosition = val->val.u16;
        break;
      case WindowCovering::Attributes::CurrentPositionLiftPercentage::Id: log_d("Window Covering Lift Percentage (legacy) changed to %d%%", val->val.u8); break;
      case WindowCovering::Attributes::CurrentPositionTiltPercentage::Id: log_d("Window Covering Tilt Percentage (legacy) changed to %d%%", val->val.u8); break;

      // Target position attributes (writable, trigger movement)
      // Note: TargetPosition is where the device SHOULD go, not where it is.
      // CurrentPosition should only be updated when the physical device actually moves.
      case WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id:
      {
        if (!chip::app::NumericAttributeTraits<uint16_t>::IsNullValue(val->val.u16)) {
          uint16_t targetLiftPercent100ths = val->val.u16;
          uint8_t targetLiftPercent = (uint8_t)(targetLiftPercent100ths / 100);
          log_d("Window Covering Target Lift Percentage changed to %d%%", targetLiftPercent);
          // Call callback to trigger movement - do NOT update currentLiftPercent here
          // `CurrentPosition` will be updated by the application when the device actually moves
          if (_onGoToLiftPercentageCB != NULL) {
            ret &= _onGoToLiftPercentageCB(targetLiftPercent);
          }
        } else {
          log_d("Window Covering Target Lift Percentage set to NULL");
        }
        break;
      }
      case WindowCovering::Attributes::TargetPositionTiltPercent100ths::Id:
      {
        if (!chip::app::NumericAttributeTraits<uint16_t>::IsNullValue(val->val.u16)) {
          uint16_t targetTiltPercent100ths = val->val.u16;
          uint8_t targetTiltPercent = (uint8_t)(targetTiltPercent100ths / 100);
          log_d("Window Covering Target Tilt Percentage changed to %d%%", targetTiltPercent);
          // Call callback to trigger movement - do NOT update currentTiltPercent here
          // CurrentPosition will be updated by the application when the device actually moves
          if (_onGoToTiltPercentageCB != NULL) {
            ret &= _onGoToTiltPercentageCB(targetTiltPercent);
          }
        } else {
          log_d("Window Covering Target Tilt Percentage set to NULL");
        }
        break;
      }

      // Configuration attributes
      case WindowCovering::Attributes::Type::Id:
        log_d("Window Covering Type changed to %d", val->val.u8);
        coveringType = (WindowCoveringType_t)val->val.u8;
        break;
      case WindowCovering::Attributes::EndProductType::Id: log_d("Window Covering End Product Type changed to %d", val->val.u8); break;
      case WindowCovering::Attributes::ConfigStatus::Id:   log_d("Window Covering Config Status changed to 0x%02X", val->val.u8); break;
      case WindowCovering::Attributes::Mode::Id:           log_d("Window Covering Mode changed to 0x%02X", val->val.u8); break;

      // Operational status attributes
      case WindowCovering::Attributes::OperationalStatus::Id: log_d("Window Covering Operational Status changed to 0x%02X", val->val.u8); break;
      case WindowCovering::Attributes::SafetyStatus::Id:      log_d("Window Covering Safety Status changed to 0x%04X", val->val.u16); break;

      // Limit attributes
      case WindowCovering::Attributes::PhysicalClosedLimitLift::Id:  log_d("Window Covering Physical Closed Limit Lift changed to %d", val->val.u16); break;
      case WindowCovering::Attributes::PhysicalClosedLimitTilt::Id:  log_d("Window Covering Physical Closed Limit Tilt changed to %d", val->val.u16); break;
      case WindowCovering::Attributes::InstalledOpenLimitLift::Id:   log_d("Window Covering Installed Open Limit Lift changed to %d", val->val.u16); break;
      case WindowCovering::Attributes::InstalledClosedLimitLift::Id: log_d("Window Covering Installed Closed Limit Lift changed to %d", val->val.u16); break;
      case WindowCovering::Attributes::InstalledOpenLimitTilt::Id:   log_d("Window Covering Installed Open Limit Tilt changed to %d", val->val.u16); break;
      case WindowCovering::Attributes::InstalledClosedLimitTilt::Id: log_d("Window Covering Installed Closed Limit Tilt changed to %d", val->val.u16); break;

      // Actuation count attributes
      case WindowCovering::Attributes::NumberOfActuationsLift::Id: log_d("Window Covering Number of Actuations Lift changed to %d", val->val.u16); break;
      case WindowCovering::Attributes::NumberOfActuationsTilt::Id: log_d("Window Covering Number of Actuations Tilt changed to %d", val->val.u16); break;

      default: log_d("Window Covering Unknown attribute %u changed", attribute_id); break;
    }
  }
  return ret;
}

bool MatterWindowCovering::setLiftPosition(uint16_t liftPosition) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  if (currentLiftPosition == liftPosition) {
    return true;
  }

  // Get InstalledOpenLimitLift and InstalledClosedLimitLift for conversion
  uint16_t openLimit = 0;
  uint16_t closedLimit = 0;
  esp_matter_attr_val_t limitVal = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledOpenLimitLift::Id, &limitVal)) {
    openLimit = limitVal.val.u16;
  }
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledClosedLimitLift::Id, &limitVal)) {
    closedLimit = limitVal.val.u16;
  }

  // Convert absolute position to percent100ths
  // Using the same logic as ESP-Matter's LiftToPercent100ths
  uint16_t liftPercent100ths = 0;
  if (openLimit != closedLimit) {
    // Linear interpolation between open (0% = 0) and closed (100% = 10000)
    if (openLimit < closedLimit) {
      // Normal: open < closed
      if (liftPosition <= openLimit) {
        liftPercent100ths = 0;  // Fully open
      } else if (liftPosition >= closedLimit) {
        liftPercent100ths = 10000;  // Fully closed
      } else {
        liftPercent100ths = ((liftPosition - openLimit) * 10000) / (closedLimit - openLimit);
      }
    } else {
      // Inverted: open > closed
      if (liftPosition >= openLimit) {
        liftPercent100ths = 0;  // Fully open
      } else if (liftPosition <= closedLimit) {
        liftPercent100ths = 10000;  // Fully closed
      } else {
        liftPercent100ths = ((openLimit - liftPosition) * 10000) / (openLimit - closedLimit);
      }
    }
  }

  // Update CurrentPositionLift (absolute)
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionLift::Id, &val)) {
    log_e("Failed to get Lift Position Attribute.");
    return false;
  }

  if (val.val.u16 != liftPosition) {
    val.val.u16 = liftPosition;
    bool ret = updateAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionLift::Id, &val);
    if (ret) {
      currentLiftPosition = liftPosition;
      // Also update CurrentPositionLiftPercent100ths to keep attributes in sync
      // This matches ESP-Matter's LiftPositionSet() behavior
      setLiftPercentage((uint8_t)(liftPercent100ths / 100));
    }
    return ret;
  }
  return true;
}

uint16_t MatterWindowCovering::getLiftPosition() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionLift::Id, &val)) {
    currentLiftPosition = val.val.u16;
  }
  return currentLiftPosition;
}

bool MatterWindowCovering::setLiftPercentage(uint8_t liftPercent) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  if (liftPercent > 100) {
    log_e("Lift percentage must be between 0 and 100");
    return false;
  }

  if (currentLiftPercent == liftPercent) {
    return true;
  }

  // Matter uses percent100ths (0-10000 for 0-100%)
  uint16_t liftPercent100ths = liftPercent * 100;

  // Update only CurrentPosition, not TargetPosition
  // TargetPosition is set by Matter commands/apps, CurrentPosition reflects actual position
  esp_matter_attr_val_t currentVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id, &currentVal)) {
    log_e("Failed to get Current Lift Percentage Attribute.");
    return false;
  }

  if (currentVal.val.u16 != liftPercent100ths) {
    currentVal.val.u16 = liftPercent100ths;
    bool ret = updateAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id, &currentVal);
    if (ret) {
      currentLiftPercent = liftPercent;
    }
    return ret;
  }
  return true;
}

uint8_t MatterWindowCovering::getLiftPercentage() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id, &val)) {
    currentLiftPercent = (uint8_t)(val.val.u16 / 100);
  }
  return currentLiftPercent;
}

bool MatterWindowCovering::setTargetLiftPercent100ths(uint16_t liftPercent100ths) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  if (liftPercent100ths > 10000) {
    log_e("Lift percent100ths must be between 0 and 10000");
    return false;
  }

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id, &val)) {
    log_e("Failed to get Target Lift Percentage Attribute.");
    return false;
  }

  if (val.val.u16 != liftPercent100ths) {
    val.val.u16 = liftPercent100ths;
    return updateAttributeVal(WindowCovering::Id, WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id, &val);
  }
  return true;
}

uint16_t MatterWindowCovering::getTargetLiftPercent100ths() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id, &val)) {
    if (!chip::app::NumericAttributeTraits<uint16_t>::IsNullValue(val.val.u16)) {
      return val.val.u16;
    }
  }
  return 0;
}

bool MatterWindowCovering::setTiltPosition(uint16_t tiltPosition) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  if (currentTiltPosition == tiltPosition) {
    return true;
  }

  // Get InstalledOpenLimitTilt and InstalledClosedLimitTilt for conversion
  uint16_t openLimit = 0;
  uint16_t closedLimit = 0;
  esp_matter_attr_val_t limitVal = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledOpenLimitTilt::Id, &limitVal)) {
    openLimit = limitVal.val.u16;
  }
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledClosedLimitTilt::Id, &limitVal)) {
    closedLimit = limitVal.val.u16;
  }

  // Convert absolute position to percent100ths
  // Using the same logic as ESP-Matter's TiltToPercent100ths
  uint16_t tiltPercent100ths = 0;
  if (openLimit != closedLimit) {
    // Linear interpolation between open (0% = 0) and closed (100% = 10000)
    if (openLimit < closedLimit) {
      // Normal: open < closed
      if (tiltPosition <= openLimit) {
        tiltPercent100ths = 0;  // Fully open
      } else if (tiltPosition >= closedLimit) {
        tiltPercent100ths = 10000;  // Fully closed
      } else {
        tiltPercent100ths = ((tiltPosition - openLimit) * 10000) / (closedLimit - openLimit);
      }
    } else {
      // Inverted: open > closed
      if (tiltPosition >= openLimit) {
        tiltPercent100ths = 0;  // Fully open
      } else if (tiltPosition <= closedLimit) {
        tiltPercent100ths = 10000;  // Fully closed
      } else {
        tiltPercent100ths = ((openLimit - tiltPosition) * 10000) / (openLimit - closedLimit);
      }
    }
  }

  // Update CurrentPositionTilt (absolute)
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionTilt::Id, &val)) {
    log_e("Failed to get Tilt Position Attribute.");
    return false;
  }

  if (val.val.u16 != tiltPosition) {
    val.val.u16 = tiltPosition;
    bool ret = updateAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionTilt::Id, &val);
    if (ret) {
      currentTiltPosition = tiltPosition;
      // Also update CurrentPositionTiltPercent100ths to keep attributes in sync
      // This matches ESP-Matter's TiltPositionSet() behavior
      setTiltPercentage((uint8_t)(tiltPercent100ths / 100));
    }
    return ret;
  }
  return true;
}

uint16_t MatterWindowCovering::getTiltPosition() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionTilt::Id, &val)) {
    currentTiltPosition = val.val.u16;
  }
  return currentTiltPosition;
}

bool MatterWindowCovering::setTiltPercentage(uint8_t tiltPercent) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  if (tiltPercent > 100) {
    log_e("Tilt percentage must be between 0 and 100");
    return false;
  }

  if (currentTiltPercent == tiltPercent) {
    return true;
  }

  // Matter uses percent100ths (0-10000 for 0-100%)
  uint16_t tiltPercent100ths = tiltPercent * 100;

  // Update only CurrentPosition, not TargetPosition
  // TargetPosition is set by Matter commands/apps, CurrentPosition reflects actual position
  esp_matter_attr_val_t currentVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionTiltPercent100ths::Id, &currentVal)) {
    log_e("Failed to get Current Tilt Percentage Attribute.");
    return false;
  }

  if (currentVal.val.u16 != tiltPercent100ths) {
    currentVal.val.u16 = tiltPercent100ths;
    bool ret = updateAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionTiltPercent100ths::Id, &currentVal);
    if (ret) {
      currentTiltPercent = tiltPercent;
    }
    return ret;
  }
  return true;
}

uint8_t MatterWindowCovering::getTiltPercentage() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::CurrentPositionTiltPercent100ths::Id, &val)) {
    currentTiltPercent = (uint8_t)(val.val.u16 / 100);
  }
  return currentTiltPercent;
}

bool MatterWindowCovering::setTargetTiltPercent100ths(uint16_t tiltPercent100ths) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  if (tiltPercent100ths > 10000) {
    log_e("Tilt percent100ths must be between 0 and 10000");
    return false;
  }

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::TargetPositionTiltPercent100ths::Id, &val)) {
    log_e("Failed to get Target Tilt Percentage Attribute.");
    return false;
  }

  if (val.val.u16 != tiltPercent100ths) {
    val.val.u16 = tiltPercent100ths;
    return updateAttributeVal(WindowCovering::Id, WindowCovering::Attributes::TargetPositionTiltPercent100ths::Id, &val);
  }
  return true;
}

uint16_t MatterWindowCovering::getTargetTiltPercent100ths() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::TargetPositionTiltPercent100ths::Id, &val)) {
    if (!chip::app::NumericAttributeTraits<uint16_t>::IsNullValue(val.val.u16)) {
      return val.val.u16;
    }
  }
  return 0;
}

bool MatterWindowCovering::setInstalledOpenLimitLift(uint16_t openLimit) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledOpenLimitLift::Id, &val)) {
    log_e("Failed to get Installed Open Limit Lift Attribute");
    return false;
  }

  if (val.val.u16 != openLimit) {
    val.val.u16 = openLimit;
    return setAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledOpenLimitLift::Id, &val);
  }
  return true;
}

uint16_t MatterWindowCovering::getInstalledOpenLimitLift() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledOpenLimitLift::Id, &val)) {
    return val.val.u16;
  }
  return 0;
}

bool MatterWindowCovering::setInstalledClosedLimitLift(uint16_t closedLimit) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledClosedLimitLift::Id, &val)) {
    log_e("Failed to get Installed Closed Limit Lift Attribute.");
    return false;
  }

  if (val.val.u16 != closedLimit) {
    val.val.u16 = closedLimit;
    return setAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledClosedLimitLift::Id, &val);
  }
  return true;
}

uint16_t MatterWindowCovering::getInstalledClosedLimitLift() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledClosedLimitLift::Id, &val)) {
    return val.val.u16;
  }
  return 0;
}

bool MatterWindowCovering::setInstalledOpenLimitTilt(uint16_t openLimit) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledOpenLimitTilt::Id, &val)) {
    log_e("Failed to get Installed Open Limit Tilt Attribute.");
    return false;
  }

  if (val.val.u16 != openLimit) {
    val.val.u16 = openLimit;
    return setAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledOpenLimitTilt::Id, &val);
  }
  return true;
}

uint16_t MatterWindowCovering::getInstalledOpenLimitTilt() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledOpenLimitTilt::Id, &val)) {
    return val.val.u16;
  }
  return 0;
}

bool MatterWindowCovering::setInstalledClosedLimitTilt(uint16_t closedLimit) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledClosedLimitTilt::Id, &val)) {
    log_e("Failed to get Installed Closed Limit Tilt Attribute.");
    return false;
  }

  if (val.val.u16 != closedLimit) {
    val.val.u16 = closedLimit;
    return setAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledClosedLimitTilt::Id, &val);
  }
  return true;
}

uint16_t MatterWindowCovering::getInstalledClosedLimitTilt() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::InstalledClosedLimitTilt::Id, &val)) {
    return val.val.u16;
  }
  return 0;
}

bool MatterWindowCovering::setCoveringType(WindowCoveringType_t coveringType) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::Type::Id, &val)) {
    log_e("Failed to get Window Covering Type Attribute.");
    return false;
  }

  if (val.val.u8 != (uint8_t)coveringType) {
    val.val.u8 = (uint8_t)coveringType;
    bool ret = updateAttributeVal(WindowCovering::Id, WindowCovering::Attributes::Type::Id, &val);
    if (ret) {
      this->coveringType = coveringType;
    }
    return ret;
  }
  return true;
}

MatterWindowCovering::WindowCoveringType_t MatterWindowCovering::getCoveringType() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::Type::Id, &val)) {
    coveringType = (WindowCoveringType_t)val.val.u8;
  }
  return coveringType;
}

bool MatterWindowCovering::setOperationalStatus(uint8_t operationalStatus) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::OperationalStatus::Id, &val)) {
    log_e("Failed to get Operational Status Attribute.");
    return false;
  }

  if (val.val.u8 != operationalStatus) {
    val.val.u8 = operationalStatus;
    return updateAttributeVal(WindowCovering::Id, WindowCovering::Attributes::OperationalStatus::Id, &val);
  }
  return true;
}

uint8_t MatterWindowCovering::getOperationalStatus() {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::OperationalStatus::Id, &val)) {
    return val.val.u8;
  }
  return 0;
}

bool MatterWindowCovering::setOperationalState(OperationalStatusField_t field, OperationalState_t state) {
  if (!started) {
    log_e("Matter Window Covering device has not begun.");
    return false;
  }

  // ESP-Matter only allows setting Lift or Tilt, not Global directly
  // Global is automatically updated based on Lift (priority) or Tilt
  if (field == GLOBAL) {
    log_e("Cannot set Global operational state directly. Set Lift or Tilt instead.");
    return false;
  }

  if (field != LIFT && field != TILT) {
    log_e("Invalid Operational Status Field. Only LIFT or TILT are allowed.");
    return false;
  }

  // Get current operational status
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::OperationalStatus::Id, &val)) {
    log_e("Failed to get Operational Status Attribute.");
    return false;
  }

  uint8_t currentStatus = val.val.u8;
  uint8_t fieldMask = (uint8_t)field;
  // For clarity: LIFT uses shift 2 (bits 2-3), TILT uses shift 4 (bits 4-5)
  uint8_t fieldShift = (field == LIFT) ? 2 : 4;

  // Extract current state for this field
  uint8_t currentFieldState = (currentStatus & fieldMask) >> fieldShift;

  // Only update if state changed
  if (currentFieldState != (uint8_t)state) {
    // Clear the field and set new state
    currentStatus = (currentStatus & ~fieldMask) | (((uint8_t)state << fieldShift) & fieldMask);

    // Following ESP-Matter behavior:
    // 1. Set the field (Lift or Tilt) to the new state
    // 2. Temporarily set Global to the same state
    // 3. Recalculate Global based on priority: Lift (if not Stall) > Tilt
    uint8_t liftState = (currentStatus & LIFT) >> 2;
    uint8_t tiltState = (currentStatus & TILT) >> 4;

    // Global follows Lift by priority, or Tilt if Lift is not active (Stall = 0)
    uint8_t globalState = (liftState != STALL) ? liftState : tiltState;

    // Update Global field (bits 0-1)
    currentStatus = (currentStatus & ~GLOBAL) | (globalState << 0);

    val.val.u8 = currentStatus;
    return updateAttributeVal(WindowCovering::Id, WindowCovering::Attributes::OperationalStatus::Id, &val);
  }
  return true;
}

MatterWindowCovering::OperationalState_t MatterWindowCovering::getOperationalState(OperationalStatusField_t field) {
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(WindowCovering::Id, WindowCovering::Attributes::OperationalStatus::Id, &val)) {
    return STALL;
  }

  uint8_t operationalStatus = val.val.u8;
  uint8_t fieldMask = (uint8_t)field;
  uint8_t fieldShift = 0;

  // Determine shift based on field
  if (field == GLOBAL) {
    fieldShift = 0;  // Bits 0-1
  } else if (field == LIFT) {
    fieldShift = 2;  // Bits 2-3
  } else if (field == TILT) {
    fieldShift = 4;  // Bits 4-5
  } else {
    return STALL;
  }

  // Extract state for this field
  uint8_t fieldState = (operationalStatus & fieldMask) >> fieldShift;
  return (OperationalState_t)fieldState;
}

void MatterWindowCovering::updateAccessory() {
  if (_onChangeCB != NULL) {
    _onChangeCB(currentLiftPercent, currentTiltPercent);
  }
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
