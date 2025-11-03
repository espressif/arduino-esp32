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

#include <Arduino.h>
#include <esp_matter.h>
#include <ColorFormat.h>
#include <MatterEndpoints/MatterGenericSwitch.h>
#include <MatterEndpoints/MatterOnOffLight.h>
#include <MatterEndpoints/MatterDimmableLight.h>
#include <MatterEndpoints/MatterColorTemperatureLight.h>
#include <MatterEndpoints/MatterColorLight.h>
#include <MatterEndpoints/MatterEnhancedColorLight.h>
#include <MatterEndpoints/MatterFan.h>
#include <MatterEndpoints/MatterTemperatureSensor.h>
#include <MatterEndpoints/MatterHumiditySensor.h>
#include <MatterEndpoints/MatterContactSensor.h>
#include <MatterEndpoints/MatterPressureSensor.h>
#include <MatterEndpoints/MatterOccupancySensor.h>
#include <MatterEndpoints/MatterOnOffPlugin.h>
#include <MatterEndpoints/MatterThermostat.h>

// Matter Event types used when there is a user callback for Matter Events
enum matterEvent_t {
  // Starting from 0x8000, these events are public and can be used by applications.
  // Defined in CHIPDeviceEvent.h

  // WiFi Connectivity Change: Signals a change in connectivity of the device's WiFi station interface.
  MATTER_WIFI_CONNECTIVITY_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kWiFiConnectivityChange,

  // Thread Connectivity Change: Signals a change in connectivity of the device's Thread interface.
  MATTER_THREAD_CONNECTIVITY_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kThreadConnectivityChange,

  // Internet Connectivity Change: Signals a change in the device's ability to communicate via the Internet.
  MATTER_INTERNET_CONNECTIVITY_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kInternetConnectivityChange,

  // Service Connectivity Change: Signals a change in the device's ability to communicate with a chip-enabled service.
  MATTER_SERVICE_CONNECTIVITY_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kServiceConnectivityChange,

  // Service Provisioning Change: Signals a change to the device's service provisioning state.
  MATTER_SERVICE_PROVISIONING_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kServiceProvisioningChange,

  // Time Sync Change: Signals a change to the device's real time clock synchronization state.
  MATTER_TIME_SYNC_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kTimeSyncChange,

  // CHIPoBLE Connection Established: Signals that an external entity has established a new
  // CHIPoBLE connection with the device.
  MATTER_CHIPOBLE_CONNECTION_ESTABLISHED = (uint16_t)chip::DeviceLayer::DeviceEventType::kCHIPoBLEConnectionEstablished,

  // CHIPoBLE Connection Closed: Signals that an external entity has closed existing CHIPoBLE
  // connection with the device.
  MATTER_CHIPOBLE_CONNECTION_CLOSED = (uint16_t)chip::DeviceLayer::DeviceEventType::kCHIPoBLEConnectionClosed,

  // Request BLE connections to be closed. This is used in the supportsConcurrentConnection = False case.
  MATTER_CLOSE_ALL_BLE_CONNECTIONS = (uint16_t)chip::DeviceLayer::DeviceEventType::kCloseAllBleConnections,

  // WiFi Device Available: When supportsConcurrentConnection = False, the ConnectNetwork
  // command cannot start until the BLE device is closed and the Operation Network device (e.g. WiFi) has been started.
  MATTER_WIFI_DEVICE_AVAILABLE = (uint16_t)chip::DeviceLayer::DeviceEventType::kWiFiDeviceAvailable,

  MATTER_OPERATIONAL_NETWORK_STARTED = (uint16_t)chip::DeviceLayer::DeviceEventType::kOperationalNetworkStarted,

  // Thread State Change: Signals that a state change has occurred in the Thread stack.
  MATTER_THREAD_STATE_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kThreadStateChange,

  // Thread Interface State Change: Signals that the state of the Thread network interface has changed.
  MATTER_THREAD_INTERFACE_STATE_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kThreadInterfaceStateChange,

  // CHIPoBLE Advertising Change: Signals that the state of CHIPoBLE advertising has changed.
  MATTER_CHIPOBLE_ADVERTISING_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kCHIPoBLEAdvertisingChange,

  // Interface IP Address Changed: IP address availability - either ipv4 or ipv6
  // addresses assigned to the underlying wifi/ethernet interface.
  MATTER_INTERFACE_IP_ADDRESS_CHANGED = (uint16_t)chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged,

  // Commissioning Complete: Commissioning has completed by a call to the general
  // commissioning cluster command.
  MATTER_COMMISSIONING_COMPLETE = (uint16_t)chip::DeviceLayer::DeviceEventType::kCommissioningComplete,

  // Fail Safe Timer Expired: Signals that the fail-safe timer expired before
  // the CommissioningComplete command was successfully invoked.
  MATTER_FAIL_SAFE_TIMER_EXPIRED = (uint16_t)chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired,

  // Operational Network Enabled.
  MATTER_OPERATIONAL_NETWORK_ENABLED = (uint16_t)chip::DeviceLayer::DeviceEventType::kOperationalNetworkEnabled,

  // DNS-SD Initialized: Signals that DNS-SD has been initialized and is ready to operate.
  MATTER_DNSSD_INITIALIZED = (uint16_t)chip::DeviceLayer::DeviceEventType::kDnssdInitialized,

  // DNS-SD Restart Needed: Signals that DNS-SD backend was restarted and services must be published again.
  MATTER_DNSSD_RESTART_NEEDED = (uint16_t)chip::DeviceLayer::DeviceEventType::kDnssdRestartNeeded,

  // Bindings Changed Via Cluster: Signals that bindings were updated.
  MATTER_BINDINGS_CHANGED_VIA_CLUSTER = (uint16_t)chip::DeviceLayer::DeviceEventType::kBindingsChangedViaCluster,

  // OTA State Changed: Signals that the state of the OTA engine changed.
  MATTER_OTA_STATE_CHANGED = (uint16_t)chip::DeviceLayer::DeviceEventType::kOtaStateChanged,

  // Server Ready: Server initialization has completed. Signals that all server components have been initialized
  // and the node is ready to establish connections with other nodes. This event can be used to trigger on-boot actions
  // that require sending messages to other nodes.
  MATTER_SERVER_READY = (uint16_t)chip::DeviceLayer::DeviceEventType::kServerReady,

  // BLE Deinitialized: Signals that BLE stack is deinitialized and memory reclaimed
  MATTER_BLE_DEINITIALIZED = (uint16_t)chip::DeviceLayer::DeviceEventType::kBLEDeinitialized,

  // Starting ESP32 Platform Specific Events from 0x9000
  MATTER_ESP32_SPECIFIC_EVENT,  // value is previous + 1

  // Commissioning Session Started: Signals that Commissioning session has started
  MATTER_COMMISSIONING_SESSION_STARTED = (uint16_t)chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted,

  // Commissioning Session Stopped: Signals that Commissioning session has stopped
  MATTER_COMMISSIONING_SESSION_STOPPED = (uint16_t)chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped,

  // Commissioning Window Opened: Signals that Commissioning window is now opened
  MATTER_COMMISSIONING_WINDOW_OPEN = (uint16_t)chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened,

  // Commissioning Window Closed: Signals that Commissioning window is now closed
  MATTER_COMMISSIONING_WINDOW_CLOSED = (uint16_t)chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed,

  // Fabric Will Be Removed: Signals that a fabric is about to be deleted. This allows actions to be taken that need the
  // fabric to still be around before we delete it
  MATTER_FABRIC_WILL_BE_REMOVED = (uint16_t)chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved,

  // Fabric Has Been Removed: Signals that a fabric is effectively deleted
  MATTER_FABRIC_REMOVED = (uint16_t)chip::DeviceLayer::DeviceEventType::kFabricRemoved,

  // Fabric Has Been Committed: Signals that a fabric in Fabric Table is persisted to storage, by CommitPendingFabricData
  MATTER_FABRIC_COMMITTED = (uint16_t)chip::DeviceLayer::DeviceEventType::kFabricCommitted,

  // Fabric Has Been Updated: Signals that operational credentials are changed, which may not be persistent.
  // Can be used to affect what is needed for UpdateNOC prior to commit
  MATTER_FABRIC_UPDATED = (uint16_t)chip::DeviceLayer::DeviceEventType::kFabricUpdated,

  // ESP32 Matter Events: These are custom ESP32 Matter events as defined in CHIPDevicePlatformEvent.h.
  MATTER_ESP32_PUBLIC_SPECIFIC_EVENT = (uint16_t)chip::DeviceLayer::DeviceEventType::kRange_PublicPlatformSpecific,  // ESPSystemEvent
};

using namespace esp_matter;

class ArduinoMatter {
public:
  // Matter Event Callback type
  using matterEventCB = std::function<void(matterEvent_t, const chip::DeviceLayer::ChipDeviceEvent *)>;
  // Matter Event Callback
  static matterEventCB _matterEventCB;
  // set the Matter Event Callback
  static void onEvent(matterEventCB cb) {
    _matterEventCB = cb;
  }

  static inline String getManualPairingCode() {
    // return the pairing code for manual pairing
    return String("34970112332");
  }
  static inline String getOnboardingQRCodeUrl() {
    // return the URL for the QR code for onboarding
    return String("https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT:Y.K9042C00KA0648G00");
  }
  static void begin();
  static bool isDeviceCommissioned();
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
  static bool isWiFiConnected();
#endif
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
  static bool isThreadConnected();
#endif
  static bool isDeviceConnected();
  static void decommission();

  // list of Matter EndPoints Friend Classes
  friend class MatterGenericSwitch;
  friend class MatterOnOffLight;
  friend class MatterDimmableLight;
  friend class MatterColorTemperatureLight;
  friend class MatterColorLight;
  friend class MatterEnhancedColorLight;
  friend class MatterFan;
  friend class MatterTemperatureSensor;
  friend class MatterHumiditySensor;
  friend class MatterContactSensor;
  friend class MatterPressureSensor;
  friend class MatterOccupancySensor;
  friend class MatterOnOffPlugin;
  friend class MatterThermostat;

protected:
  static void _init();
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_MATTER)
extern ArduinoMatter Matter;
#endif

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
