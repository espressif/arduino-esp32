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
#include <MatterEndpoints/MatterOccupancySensor.h>
#include <esp_matter_cluster.h>
#include <esp_matter_attribute.h>
#include <esp_matter_core.h>
#include <app/clusters/occupancy-sensor-server/occupancy-sensor-server.h>
#include <platform/CHIPDeviceLayer.h>
#include <app/AttributeAccessInterface.h>
#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/AttributeValueDecoder.h>
#include <app/AttributeValueEncoder.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace esp_matter::cluster::occupancy_sensing::attribute;
using namespace chip::app::Clusters;

// Custom AttributeAccessInterface wrapper that intercepts HoldTime writes to call user callbacks
// This wraps the standard OccupancySensing::Instance to add callback support
class OccupancySensingAttrAccessWrapper : public chip::app::AttributeAccessInterface {
public:
  OccupancySensingAttrAccessWrapper() 
    : chip::app::AttributeAccessInterface(chip::Optional<chip::EndpointId>::Missing(), OccupancySensing::Id),
      mInstance(chip::BitMask<OccupancySensing::Feature>(0)) {
  }

  CHIP_ERROR Init() {
    // Register THIS wrapper instance, not the standard instance
    // The wrapper will delegate reads/writes to mInstance internally
    bool registered = chip::app::AttributeAccessInterfaceRegistry::Instance().Register(this);
    if (!registered) {
      log_e("Failed to register OccupancySensing AttributeAccessInterface (duplicate?)");
      return CHIP_ERROR_INCORRECT_STATE;
    }
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR Read(const chip::app::ConcreteReadAttributePath & aPath, chip::app::AttributeValueEncoder & aEncoder) override {
    // Delegate reads to the standard instance
    return mInstance.Read(aPath, aEncoder);
  }

  CHIP_ERROR Write(const chip::app::ConcreteDataAttributePath & aPath, chip::app::AttributeValueDecoder & aDecoder) override {
    // Intercept HoldTime writes to call user callbacks
    if (aPath.mAttributeId == OccupancySensing::Attributes::HoldTime::Id) {
      uint16_t newHoldTime;
      CHIP_ERROR err = aDecoder.Decode(newHoldTime);
      if (err != CHIP_NO_ERROR) {
        return err;
      }

      // Validate against HoldTimeLimits first (same as Instance::Write does)
      OccupancySensing::Structs::HoldTimeLimitsStruct::Type * currHoldTimeLimits = 
        OccupancySensing::GetHoldTimeLimitsForEndpoint(aPath.mEndpointId);
      if (currHoldTimeLimits == nullptr) {
        return CHIP_ERROR_INVALID_ARGUMENT;
      }
      if (newHoldTime < currHoldTimeLimits->holdTimeMin || newHoldTime > currHoldTimeLimits->holdTimeMax) {
        return CHIP_IM_GLOBAL_STATUS(ConstraintError);
      }

      // Find the MatterOccupancySensor instance for this endpoint and call its callback
      MatterOccupancySensor* sensor = FindOccupancySensorForEndpoint(aPath.mEndpointId);
      if (sensor != nullptr) {
        // Call the user callback if set (this allows rejection of the change)
        if (sensor->_onHoldTimeChangeCB) {
          if (!sensor->_onHoldTimeChangeCB(newHoldTime)) {
            // User callback rejected the change
            return CHIP_IM_GLOBAL_STATUS(ConstraintError);
          }
        }
      }

      // Call SetHoldTime directly (same as Instance::Write does)
      err = OccupancySensing::SetHoldTime(aPath.mEndpointId, newHoldTime);
      if (err == CHIP_NO_ERROR && sensor != nullptr) {
        // Update the internal value to keep it in sync
        sensor->holdTime_seconds = newHoldTime;
      }
      return err;
    }

    // For other attributes, delegate to the standard instance
    return mInstance.Write(aPath, aDecoder);
  }

private:
  OccupancySensing::Instance mInstance;

  // Helper to find MatterOccupancySensor instance for an endpoint
  static MatterOccupancySensor* FindOccupancySensorForEndpoint(chip::EndpointId endpointId) {
    // Get the endpoint's private data (set when creating the endpoint)
    void* priv_data = esp_matter::endpoint::get_priv_data(endpointId);
    if (priv_data == nullptr) {
      return nullptr;
    }
    
    MatterOccupancySensor* sensor = static_cast<MatterOccupancySensor*>(priv_data);
    // Verify it's actually a MatterOccupancySensor by checking if it's started
    if (sensor != nullptr && sensor->started) {
      return sensor;
    }
    
    return nullptr;
  }
};

// Static AttributeAccessInterface wrapper instance
// Registered once globally to handle all OccupancySensing endpoints
namespace {
  static OccupancySensingAttrAccessWrapper sOccupancySensingAttrAccess;
  static bool sOccupancySensingAttrAccessRegistered = false;
}

// Static helper functions for Matter event loop operations
namespace {
  void SetHoldTimeInEventLoop(uint16_t endpoint_id, uint16_t holdTime_seconds) {
    CHIP_ERROR err = OccupancySensing::SetHoldTime(endpoint_id, holdTime_seconds);
    if (err != CHIP_NO_ERROR) {
      ChipLogError(NotSpecified, "Failed to set HoldTime: %" CHIP_ERROR_FORMAT, err.Format());
    } else {
      ChipLogDetail(NotSpecified, "HoldTime set to %u seconds", holdTime_seconds);
    }
  }

  void SetHoldTimeLimitsInEventLoop(uint16_t endpoint_id, uint16_t min_seconds, uint16_t max_seconds, uint16_t default_seconds) {
    OccupancySensing::Structs::HoldTimeLimitsStruct::Type holdTimeLimits;
    holdTimeLimits.holdTimeMin = min_seconds;
    holdTimeLimits.holdTimeMax = max_seconds;
    holdTimeLimits.holdTimeDefault = default_seconds;
    
    CHIP_ERROR err = OccupancySensing::SetHoldTimeLimits(endpoint_id, holdTimeLimits);
    if (err != CHIP_NO_ERROR) {
      ChipLogError(NotSpecified, "Failed to set HoldTimeLimits: %" CHIP_ERROR_FORMAT, err.Format());
    } else {
      ChipLogDetail(NotSpecified, "HoldTimeLimits set: Min=%u, Max=%u, Default=%u seconds", 
                    min_seconds, max_seconds, default_seconds);
    }
  }

  void SetHoldTimeLimitsAndHoldTimeInEventLoop(uint16_t endpoint_id, uint16_t min_seconds, uint16_t max_seconds, 
                                                uint16_t default_seconds, uint16_t holdTime_seconds) {
    // Set HoldTimeLimits first
    SetHoldTimeLimitsInEventLoop(endpoint_id, min_seconds, max_seconds, default_seconds);
    
    // Then adjust HoldTime to be within the new limits
    SetHoldTimeInEventLoop(endpoint_id, holdTime_seconds);
  }
}

// clang-format off
const uint8_t MatterOccupancySensor::occupancySensorTypeBitmap[4] = {
  MatterOccupancySensor::occupancySensorTypePir,
  MatterOccupancySensor::occupancySensorTypePir | MatterOccupancySensor::occupancySensorTypeUltrasonic,
  MatterOccupancySensor::occupancySensorTypeUltrasonic,
  MatterOccupancySensor::occupancySensorTypePhysicalContact
};
// clang-format on

bool MatterOccupancySensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  if (!started) {
    log_e("Matter Occupancy Sensor device has not begun.");
    return false;
  }

  log_d("Occupancy Sensor Attr update callback: endpoint: %u, cluster: %u, attribute: %u", endpoint_id, cluster_id, attribute_id);

  // Note: HoldTime writes are handled by OccupancySensingAttrAccessWrapper::Write() 
  // since HoldTime is MANAGED_INTERNALLY and doesn't go through the normal esp-matter callback path

  return true;
}

void MatterOccupancySensor::onHoldTimeChange(HoldTimeChangeCB onHoldTimeChangeCB) {
  _onHoldTimeChangeCB = onHoldTimeChangeCB;
}

MatterOccupancySensor::MatterOccupancySensor() {
  // HoldTimeLimits must be set explicitly via setHoldTimeLimits() after Matter.begin()
}

MatterOccupancySensor::~MatterOccupancySensor() {
  end();
}

bool MatterOccupancySensor::begin(bool _occupancyState, OccupancySensorType_t _occupancySensorType) {
  ArduinoMatter::_init();

  // Initial HoldTime value is 0 (can be set later via setHoldTime() or setHoldTimeLimits())
  holdTime_seconds = 0;
  if (getEndPointId() != 0) {
    log_e("Matter Occupancy Sensor with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }
  occupancy_sensor::config_t occupancy_sensor_config;
  occupancy_sensor_config.occupancy_sensing.occupancy = _occupancyState;
  occupancy_sensor_config.occupancy_sensing.occupancy_sensor_type = _occupancySensorType;
  occupancy_sensor_config.occupancy_sensing.occupancy_sensor_type_bitmap = occupancySensorTypeBitmap[_occupancySensorType];
  
  // Set features based on sensor type
  // Available features: other, passive_infrared, ultrasonic, physical_contact, active_infrared, radar, rf_sensing, vision
  using namespace esp_matter::cluster::occupancy_sensing::feature;
  
  switch (_occupancySensorType) {
    case OCCUPANCY_SENSOR_TYPE_PIR:
      occupancy_sensor_config.occupancy_sensing.features = passive_infrared::get_id();
      break;
    case OCCUPANCY_SENSOR_TYPE_ULTRASONIC:
      occupancy_sensor_config.occupancy_sensing.features = ultrasonic::get_id();
      break;
    case OCCUPANCY_SENSOR_TYPE_PIR_AND_ULTRASONIC:
      occupancy_sensor_config.occupancy_sensing.features = passive_infrared::get_id() | ultrasonic::get_id();
      break;
    case OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT:
      occupancy_sensor_config.occupancy_sensing.features = physical_contact::get_id();
      break;
    default:
      // For unknown types, use "other" feature
      occupancy_sensor_config.occupancy_sensing.features = other::get_id();
      break;
  }
  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = occupancy_sensor::create(node::get(), &occupancy_sensor_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Occupancy Sensor endpoint");
    return false;
  }
  setEndPointId(endpoint::get_id(endpoint));
  occupancyState = _occupancyState;
  
  // Register AttributeAccessInterface for OccupancySensing cluster if not already registered
  // This enables HoldTime and HoldTimeLimits (MANAGED_INTERNALLY attributes) to be read
  // via the server implementation instead of falling back to esp-matter's placeholder storage
  if (!sOccupancySensingAttrAccessRegistered) {
    CHIP_ERROR err = sOccupancySensingAttrAccess.Init();
    if (err == CHIP_NO_ERROR) {
      sOccupancySensingAttrAccessRegistered = true;
    } else {
      log_e("Failed to register OccupancySensing AttributeAccessInterface: %" CHIP_ERROR_FORMAT, err.Format());
    }
  }
  
  // Add HoldTime and HoldTimeLimits attributes to the occupancy sensing cluster
  cluster_t *cluster = cluster::get(endpoint, OccupancySensing::Id);
  if (cluster != nullptr) {
    // Create HoldTime attribute first (HoldTimeLimits may depend on it)
    attribute_t *hold_time_attr = create_hold_time(cluster, holdTime_seconds);
    if (hold_time_attr == nullptr) {
      log_e("Failed to create HoldTime attribute");
      // Continue anyway, as HoldTime is optional
    } else {
      log_d("HoldTime attribute created with value %u seconds", holdTime_seconds);
    }

    // Create the HoldTimeLimits attribute
    // Since this attribute is MANAGED_INTERNALLY, we pass NULL/0/0 and let the CHIP server manage the value
    // The server will handle TLV encoding/decoding automatically via AttributeAccessInterface
    // Note: HoldTimeLimits should only be created if HoldTime was successfully created
    if (hold_time_attr != nullptr) {
      attribute_t *hold_time_limits_attr = create_hold_time_limits(cluster, NULL, 0, 0);
      if (hold_time_limits_attr == nullptr) {
        log_e("Failed to create HoldTimeLimits attribute");
      } else {
        log_d("HoldTimeLimits attribute created");
      }
    } else {
      log_w("Skipping HoldTimeLimits creation because HoldTime attribute creation failed");
    }
  } else {
    log_e("Failed to get Occupancy Sensing cluster");
  }

  log_i("Occupancy Sensor created with endpoint_id %d", getEndPointId());

  started = true;
  return true;
}

void MatterOccupancySensor::end() {
  started = false;
}

bool MatterOccupancySensor::setOccupancy(bool _occupancyState) {
  if (!started) {
    log_e("Matter Occupancy Sensor device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (occupancyState == _occupancyState) {
    return true;
  }

  esp_matter_attr_val_t occupancyVal = esp_matter_invalid(NULL);

  if (!getAttributeVal(OccupancySensing::Id, OccupancySensing::Attributes::Occupancy::Id, &occupancyVal)) {
    log_e("Failed to get Occupancy Sensor Attribute.");
    return false;
  }
  if (occupancyVal.val.u8 != _occupancyState) {
    occupancyVal.val.u8 = _occupancyState;
    if (!updateAttributeVal(OccupancySensing::Id, OccupancySensing::Attributes::Occupancy::Id, &occupancyVal)) {
      log_e("Failed to update Occupancy Sensor Attribute.");
      return false;
    }
    occupancyState = _occupancyState;
  }
  log_v("Occupancy Sensor set to %s", _occupancyState ? "Occupied" : "Vacant");

  return true;
}

bool MatterOccupancySensor::setHoldTime(uint16_t _holdTime_seconds) {
  if (!started) {
    log_e("Matter Occupancy Sensor device has not begun.");
    return false;
  }

  if (getEndPointId() == 0) {
    log_e("Endpoint ID is not set");
    return false;
  }

  // avoid processing if there was no change
  if (holdTime_seconds == _holdTime_seconds) {
    return true;
  }

  // Validate against HoldTimeLimits if they are set (using member variables)
  if (holdTimeMax_seconds > 0) {
    // Limits are set, validate the new value
    if (_holdTime_seconds < holdTimeMin_seconds) {
      log_e("HoldTime (%u) is below minimum (%u seconds)", _holdTime_seconds, holdTimeMin_seconds);
      return false;
    }
    if (_holdTime_seconds > holdTimeMax_seconds) {
      log_e("HoldTime (%u) exceeds maximum (%u seconds)", _holdTime_seconds, holdTimeMax_seconds);
      return false;
    }
  }

  // SetHoldTime() calls MatterReportingAttributeChangeCallback() which must be called
  // from the Matter event loop context to avoid stack locking errors.
  // Schedule the call on the Matter event loop using ScheduleLambda.
  if (!chip::DeviceLayer::SystemLayer().IsInitialized()) {
    log_e("SystemLayer is not initialized. Matter.begin() must be called before setHoldTime().");
    return false;
  }

  uint16_t endpoint_id = getEndPointId();
  
  CHIP_ERROR schedule_err = chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, holdTime = _holdTime_seconds]() {
    SetHoldTimeInEventLoop(endpoint_id, holdTime);
  });

  if (schedule_err != CHIP_NO_ERROR) {
    log_e("Failed to schedule HoldTime update: %" CHIP_ERROR_FORMAT, schedule_err.Format());
    return false;
  }

  // Update member variable immediately
  holdTime_seconds = _holdTime_seconds;
  log_v("HoldTime scheduled for update with value %u seconds", _holdTime_seconds);

  return true;
}

bool MatterOccupancySensor::setHoldTimeLimits(uint16_t _holdTimeMin_seconds, uint16_t _holdTimeMax_seconds, uint16_t _holdTimeDefault_seconds) {
  if (!started) {
    log_e("Matter Occupancy Sensor device has not begun.");
    return false;
  }

  if (getEndPointId() == 0) {
    log_e("Endpoint ID is not set");
    return false;
  }

  // Validate limits
  if (_holdTimeMin_seconds > _holdTimeMax_seconds) {
    log_e("HoldTimeMin (%u) must be <= HoldTimeMax (%u)", _holdTimeMin_seconds, _holdTimeMax_seconds);
    return false;
  }

  if (_holdTimeDefault_seconds < _holdTimeMin_seconds || _holdTimeDefault_seconds > _holdTimeMax_seconds) {
    log_e("HoldTimeDefault (%u) must be between HoldTimeMin (%u) and HoldTimeMax (%u)", 
          _holdTimeDefault_seconds, _holdTimeMin_seconds, _holdTimeMax_seconds);
    return false;
  }

  // SetHoldTimeLimits() calls MatterReportingAttributeChangeCallback() which must be called
  // from the Matter event loop context to avoid stack locking errors.
  // Schedule the call on the Matter event loop using ScheduleLambda.
  // First check if the scheduler is available (Matter.begin() must have been called)
  if (!chip::DeviceLayer::SystemLayer().IsInitialized()) {
    log_e("SystemLayer is not initialized. Matter.begin() must be called before setHoldTimeLimits().");
    return false;
  }

  // Update member variables immediately
  holdTimeMin_seconds = _holdTimeMin_seconds;
  holdTimeMax_seconds = _holdTimeMax_seconds;
  holdTimeDefault_seconds = _holdTimeDefault_seconds;

  // Check if current HoldTime is outside the new limits and adjust if necessary
  uint16_t adjustedHoldTime = holdTime_seconds;
  bool holdTimeAdjusted = false;
  
  if (holdTime_seconds < _holdTimeMin_seconds) {
    adjustedHoldTime = _holdTimeMin_seconds;
    holdTimeAdjusted = true;
    log_i("Current HoldTime (%u) is below new minimum (%u), adjusting to minimum", 
          holdTime_seconds, _holdTimeMin_seconds);
  } else if (holdTime_seconds > _holdTimeMax_seconds) {
    adjustedHoldTime = _holdTimeMax_seconds;
    holdTimeAdjusted = true;
    log_i("Current HoldTime (%u) exceeds new maximum (%u), adjusting to maximum", 
          holdTime_seconds, _holdTimeMax_seconds);
  }

  uint16_t endpoint_id = getEndPointId();
  CHIP_ERROR schedule_err;
  
  if (holdTimeAdjusted) {
    // Schedule both limits and HoldTime updates together
    schedule_err = chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, min = _holdTimeMin_seconds, max = _holdTimeMax_seconds, 
                                                                     def = _holdTimeDefault_seconds, holdTime = adjustedHoldTime]() {
      SetHoldTimeLimitsAndHoldTimeInEventLoop(endpoint_id, min, max, def, holdTime);
    });
    holdTime_seconds = adjustedHoldTime;
  } else {
    // No adjustment needed, just schedule the limits update
    schedule_err = chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, min = _holdTimeMin_seconds, max = _holdTimeMax_seconds, 
                                                                     def = _holdTimeDefault_seconds]() {
      SetHoldTimeLimitsInEventLoop(endpoint_id, min, max, def);
    });
  }

  if (schedule_err != CHIP_NO_ERROR) {
    log_e("Failed to schedule HoldTimeLimits update: %" CHIP_ERROR_FORMAT, schedule_err.Format());
    return false;
  }

  log_v("HoldTimeLimits scheduled for update: Min=%u, Max=%u, Default=%u seconds", 
        _holdTimeMin_seconds, _holdTimeMax_seconds, _holdTimeDefault_seconds);

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */

