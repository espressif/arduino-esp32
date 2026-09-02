// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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
#include <MatterEndPoint.h>
#include <MatterTags.h>
#include <MatterEndpoints/MatterGenericSwitch.h>
#include <MatterEndpoints/MatterOnOffLight.h>
#include <MatterEndpoints/MatterDimmableLight.h>
#include <MatterEndpoints/MatterColorTemperatureLight.h>
#include <MatterEndpoints/MatterColorLight.h>
#include <MatterEndpoints/MatterEnhancedColorLight.h>
#include <MatterEndpoints/MatterFan.h>
#include <MatterEndpoints/MatterTemperatureSensor.h>
#include <MatterEndpoints/MatterTemperatureControlledCabinet.h>
#include <MatterEndpoints/MatterHumiditySensor.h>
#include <MatterEndpoints/MatterContactSensor.h>
#include <MatterEndpoints/MatterWaterLeakDetector.h>
#include <MatterEndpoints/MatterWaterFreezeDetector.h>
#include <MatterEndpoints/MatterRainSensor.h>
#include <MatterEndpoints/MatterPressureSensor.h>
#include <MatterEndpoints/MatterOccupancySensor.h>
#include <MatterEndpoints/MatterOnOffPlugin.h>
#include <MatterEndpoints/MatterDimmablePlugin.h>
#include <MatterEndpoints/MatterThermostat.h>
#include <MatterEndpoints/MatterWindowCovering.h>
#include <MatterEndpoints/MatterLightSensor.h>
#include "matter_closure_patch.h"

// Matter Event types used when there is a user callback for Matter Events
enum matterEvent_t {
  // Starting from 0x8000, these events are public and can be used by applications.
  // Defined in CHIPDeviceEvent.h

  // Wi-Fi Connectivity Change: Signals a change in connectivity of the device's Wi-Fi station interface.
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

  // Wi-Fi Device Available: When supportsConcurrentConnection = False, the ConnectNetwork
  // command cannot start until the BLE device is closed and the Operation Network device (e.g. Wi-Fi) has been started.
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

  // Secure Session Established: Signals that a secure session (PASE or CASE) is established.
  MATTER_SECURE_SESSION_ESTABLISHED = (uint16_t)chip::DeviceLayer::DeviceEventType::kSecureSessionEstablished,

  // Factory Reset: Signals that factory reset has started.
  MATTER_FACTORY_RESET = (uint16_t)chip::DeviceLayer::DeviceEventType::kFactoryReset,

  // ESP-Matter platform specific events, from kRange_PublicPlatformSpecific + 0x1000.
  // Defined in esp_matter.h

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
  MATTER_ESP32_SPECIFIC_EVENT = MATTER_ESP32_PUBLIC_SPECIFIC_EVENT,
};

// Runtime network selection. NONE is the default: existing sketches are unchanged.
enum matterNetwork_t {
  MATTER_NETWORK_NONE = 0,
  MATTER_NETWORK_WIFI,
  MATTER_NETWORK_THREAD,
  MATTER_NETWORK_ETHERNET
};

using namespace esp_matter;

class ArduinoMatter {
public:
  // Matter Event Callback type
  using matterEventCB = std::function<void(matterEvent_t, const chip::DeviceLayer::ChipDeviceEvent *)>;
  // Matter Event Callback
  static matterEventCB _matterEventCB;
  // set the Matter Event Callback. The ChipDeviceEvent pointer is only valid
  // during this call; do not store it.
  static void onEvent(matterEventCB cb) {
    _matterEventCB = cb;
  }

  // Called after CHIPoBLE BLE RAM has been returned to the heap (CHIP kBLEDeinitialized).
  // Register before Matter.begin(). Runs on the CHIP task: do not block; set a flag and
  // allocate large buffers from loop(). May never run if CHIPoBLE is off or release is disabled.
  using bleMemoryReleasedCB = std::function<void()>;
  static bleMemoryReleasedCB _bleMemoryReleasedCB;
  static void onBLEMemoryReleased(bleMemoryReleasedCB cb) {
    _bleMemoryReleasedCB = cb;
  }

  // Generated after Matter.begin() from CommissionableDataProvider.
  // Before begin() these log a warning and return an empty String.
  static String getManualPairingCode();
  static String getOnboardingQRCodeUrl();
  // Starts the Matter stack. On Wi-Fi station builds with no Thread/Ethernet
  // selection, initializes Wi-Fi first with reduced RX/TX buffers so CHIP
  // inherits those counts (first esp_wifi_init wins).
  static void begin();

  // Node identity (Basic Information on endpoint 0). Call before Matter.begin().
  // String setters copy into internal storage; the argument need not outlive the call.
  static bool setVendorName(const char *name);
  static bool setProductName(const char *name);
  static bool setDeviceName(const char *name);  // writes NodeLabel
  static bool setSerialNumber(const char *value);
  static bool setHardwareVersion(uint16_t version);
  static bool setHardwareVersionString(const char *value);

  // Commissioning codes. Call before Matter.begin(). Test defaults are 0xF00 / 20202021.
  static bool setSetupDiscriminator(uint16_t discriminator);
  static bool setSetupPasscode(uint32_t passcode);

  // CHIPoBLE on/off. Call before Matter.begin(). Default is true only when CONFIG_ENABLE_CHIPOBLE.
  // false forces on-network commissioning (Wi-Fi/Ethernet first) and releases BLE RAM.
  // Do not use the Arduino BLE library (BLE.h / BLEDevice) in a Matter sketch.
  static bool setBLECommissioningEnabled(bool enabled);

  // After CHIPoBLE commissioning, release BLE RAM. Call before Matter.begin(). Default true.
  // Only takes effect when CONFIG_ENABLE_CHIPOBLE is set and CHIPoBLE commissioning is enabled.
  // No effect if CHIPoBLE is compiled out (no BT, ESP32-S2, or Arduino IDE ESP32 prebuild
  // which uses Bluedroid without CHIPoBLE). Arduino-as-IDF-component ESP32 can enable NimBLE
  // and CONFIG_ENABLE_CHIPOBLE; then this API applies. To keep BLE after commission, those
  // builds also need CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING=n.
  static bool setBLEMemoryReleaseEnabled(bool enabled);

  // Compile-time capability (Kconfig / SOC). Not "the interface is up".
  static bool isWiFiStationEnabled();       // CONFIG_ENABLE_WIFI_STATION (false on H2)
  static bool isWiFiAccessPointEnabled();   // CONFIG_ENABLE_WIFI_AP
  static bool isThreadEnabled();            // CONFIG_ENABLE_MATTER_OVER_THREAD (C6/H2; C5 when Matter Network is Thread)
  static bool isEthernetEnabled();          // CONFIG_ETH_ENABLED: ETH library builds; not "cable present"
  static bool isBLECommissioningEnabled();  // CHIPoBLE compiled in and still enabled
  static bool isBLEMemoryReleaseEnabled();  // CHIPoBLE on and BLE RAM will be released after commission

  // Runtime network selection. Call selectNetwork() before any accessory begin().
  // Records intent only: does not start Wi-Fi, Thread, or Ethernet, and does not
  // apply a Thread dataset. Ethernet: sketch must ETH.begin() + enableIPv6().
  // Dual-stack C6: selectNetwork(THREAD) puts Thread Network Commissioning on endpoint 0
  // (replaces the prebuild Wi-Fi driver) so hubs that only talk to the root see Thread.
  // Matter.begin() skips CHIP's Wi-Fi init for Thread/Ethernet.
  static bool isNetworkSupported(matterNetwork_t network);  // same as the is*Enabled() helpers
  // BLE default: Ethernet disables CHIPoBLE; Wi-Fi and Thread leave it on.
  // MATTER_NETWORK_NONE clears the selection and does not change BLE.
  static bool selectNetwork(matterNetwork_t network);
  // disableBLECommissioning true: setBLECommissioningEnabled(false).
  // false: does not turn CHIPoBLE back on if the sketch already disabled it.
  static bool selectNetwork(matterNetwork_t network, bool disableBLECommissioning);
  static matterNetwork_t getSelectedNetwork();  // last selectNetwork(), or NONE
  // Which netif has IPv6 (link-local or global). Prefers the selection if that
  // interface is up; else Wi-Fi, Thread, Ethernet. Not isWiFiConnected().
  static matterNetwork_t getActiveNetwork();
  // Expected Network Commissioning endpoint, even before it is created.
  // Wi-Fi: 0, or 0xFFFF when Thread replaced the root driver (C6).
  // Thread: 0 when Thread is selected (C6 replaces root Wi-Fi; H2 is already 0).
  // Ethernet / unsupported: 0xFFFF.
  static uint16_t getNetworkEndPointId(matterNetwork_t network);
  // Block (delay) until the selected interface has IPv6. NONE waits for any.
  // Does not start hardware. timeoutMs 0 is a single check.
  static bool waitForNetwork(uint32_t timeoutMs);

  static bool isDeviceCommissioned();
  static bool isWiFiConnected();
  static bool isThreadConnected();
  static bool isDeviceConnected();
  static bool isOnline();  // active CASE session with a controller
  static void decommission();

  // list of Matter EndPoints Friend Classes
  friend class MatterGenericSwitch;
  friend class MatterOnOffLight;
  friend class MatterDimmableLight;
  friend class MatterDimmablePlugin;
  friend class MatterColorTemperatureLight;
  friend class MatterColorLight;
  friend class MatterEnhancedColorLight;
  friend class MatterFan;
  friend class MatterTemperatureSensor;
  friend class MatterTemperatureControlledCabinet;
  friend class MatterHumiditySensor;
  friend class MatterContactSensor;
  friend class MatterWaterLeakDetector;
  friend class MatterWaterFreezeDetector;
  friend class MatterRainSensor;
  friend class MatterPressureSensor;
  friend class MatterOccupancySensor;
  friend class MatterOnOffPlugin;
  friend class MatterThermostat;
  friend class MatterWindowCovering;
  friend class MatterLightSensor;

protected:
  static void _init();
  static bool isStackStarted();  // true only after a successful Matter.begin()
  static bool ensureSetBeforeBegin(const char *apiName);
  static bool storeIdentityString(char *dst, size_t dstSize, const char *src, const char *apiName);
  static void applyIdentityBeforeStart();
  static void applyIdentityAfterStart();
  static void applyBlePolicyAfterStart();
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_MATTER)
extern ArduinoMatter Matter;
#endif

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
