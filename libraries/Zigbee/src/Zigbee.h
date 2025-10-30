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

// Zigbee library header file for includes of all Zigbee library headers.

#pragma once

// Common types and functions
#include "ZigbeeTypes.h"

// Core
#include "ZigbeeCore.h"
#include "ZigbeeEP.h"

// Endpoints
//// Switches
#include "ep/ZigbeeColorDimmerSwitch.h"
#include "ep/ZigbeeSwitch.h"
//// Lights
#include "ep/ZigbeeColorDimmableLight.h"
#include "ep/ZigbeeDimmableLight.h"
#include "ep/ZigbeeLight.h"
//// Controllers
#include "ep/ZigbeeThermostat.h"
#include "ep/ZigbeeFanControl.h"
////Outlets
#include "ep/ZigbeePowerOutlet.h"
//// Sensors
#include "ep/ZigbeeAnalog.h"
#include "ep/ZigbeeBinary.h"
#include "ep/ZigbeeCarbonDioxideSensor.h"
#include "ep/ZigbeeContactSwitch.h"
#include "ep/ZigbeeDoorWindowHandle.h"
#include "ep/ZigbeeElectricalMeasurement.h"
#include "ep/ZigbeeFlowSensor.h"
#include "ep/ZigbeeIlluminanceSensor.h"
#include "ep/ZigbeeMultistate.h"
#include "ep/ZigbeeOccupancySensor.h"
#include "ep/ZigbeePM25Sensor.h"
#include "ep/ZigbeePressureSensor.h"
#include "ep/ZigbeeTempSensor.h"
#include "ep/ZigbeeVibrationSensor.h"
#include "ep/ZigbeeWindSpeedSensor.h"
#include "ep/ZigbeeWindowCovering.h"
//// Other
#include "ep/ZigbeeGateway.h"
#include "ep/ZigbeeRangeExtender.h"
