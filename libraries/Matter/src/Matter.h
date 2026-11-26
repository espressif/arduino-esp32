// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

#pragma once
#include <sdkconfig.h>
#include <MatterC5Network.h>
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
#include <MatterEndpoints/MatterSoilSensor.h>
#include <MatterEndpoints/MatterWaterHeater.h>
#include <MatterEndpoints/MatterWaterValve.h>

/**
 * @brief Matter device-layer events delivered to @ref ArduinoMatter::onEvent.
 *
 * Public CHIP events start at 0x8000 (CHIPDeviceEvent.h). ESP-Matter platform
 * events follow, then ESP32-specific events from CHIPDevicePlatformEvent.h.
 * The accompanying ChipDeviceEvent pointer is valid only during the callback.
 */
enum matterEvent_t {
  // Starting from 0x8000, these events are public and can be used by applications.
  // Defined in CHIPDeviceEvent.h

  //! Signals a change in connectivity of the device's Wi-Fi station interface.
  MATTER_WIFI_CONNECTIVITY_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kWiFiConnectivityChange,

  //! Signals a change in connectivity of the device's Thread interface.
  MATTER_THREAD_CONNECTIVITY_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kThreadConnectivityChange,

  //! Signals a change in the device's ability to communicate via the Internet.
  MATTER_INTERNET_CONNECTIVITY_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kInternetConnectivityChange,

  //! Signals a change in the device's ability to communicate with a chip-enabled service.
  MATTER_SERVICE_CONNECTIVITY_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kServiceConnectivityChange,

  //! Signals a change to the device's service provisioning state.
  MATTER_SERVICE_PROVISIONING_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kServiceProvisioningChange,

  //! Signals a change to the device's real time clock synchronization state.
  MATTER_TIME_SYNC_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kTimeSyncChange,

  //! Signals that an external entity has established a new CHIPoBLE connection with the device.
  MATTER_CHIPOBLE_CONNECTION_ESTABLISHED = (uint16_t)chip::DeviceLayer::DeviceEventType::kCHIPoBLEConnectionEstablished,

  //! Signals that an external entity has closed an existing CHIPoBLE connection with the device.
  MATTER_CHIPOBLE_CONNECTION_CLOSED = (uint16_t)chip::DeviceLayer::DeviceEventType::kCHIPoBLEConnectionClosed,

  //! Request BLE connections to be closed. Used when supportsConcurrentConnection is false.
  MATTER_CLOSE_ALL_BLE_CONNECTIONS = (uint16_t)chip::DeviceLayer::DeviceEventType::kCloseAllBleConnections,

  //! When supportsConcurrentConnection is false, ConnectNetwork waits until BLE is closed and the operational network (e.g. Wi-Fi) has started.
  MATTER_WIFI_DEVICE_AVAILABLE = (uint16_t)chip::DeviceLayer::DeviceEventType::kWiFiDeviceAvailable,

  //! Signals that the operational network has started.
  MATTER_OPERATIONAL_NETWORK_STARTED = (uint16_t)chip::DeviceLayer::DeviceEventType::kOperationalNetworkStarted,

  //! Signals that a state change has occurred in the Thread stack.
  MATTER_THREAD_STATE_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kThreadStateChange,

  //! Signals that the state of the Thread network interface has changed.
  MATTER_THREAD_INTERFACE_STATE_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kThreadInterfaceStateChange,

  //! Signals that the state of CHIPoBLE advertising has changed.
  MATTER_CHIPOBLE_ADVERTISING_CHANGE = (uint16_t)chip::DeviceLayer::DeviceEventType::kCHIPoBLEAdvertisingChange,

  //! IP address availability — IPv4 or IPv6 addresses assigned to the underlying Wi-Fi/Ethernet interface.
  MATTER_INTERFACE_IP_ADDRESS_CHANGED = (uint16_t)chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged,

  //! Commissioning has completed by a call to the general commissioning cluster command.
  MATTER_COMMISSIONING_COMPLETE = (uint16_t)chip::DeviceLayer::DeviceEventType::kCommissioningComplete,

  //! Signals that the fail-safe timer expired before the CommissioningComplete command was successfully invoked.
  MATTER_FAIL_SAFE_TIMER_EXPIRED = (uint16_t)chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired,

  //! Operational network enabled.
  MATTER_OPERATIONAL_NETWORK_ENABLED = (uint16_t)chip::DeviceLayer::DeviceEventType::kOperationalNetworkEnabled,

  //! Signals that DNS-SD has been initialized and is ready to operate.
  MATTER_DNSSD_INITIALIZED = (uint16_t)chip::DeviceLayer::DeviceEventType::kDnssdInitialized,

  //! Signals that the DNS-SD backend was restarted and services must be published again.
  MATTER_DNSSD_RESTART_NEEDED = (uint16_t)chip::DeviceLayer::DeviceEventType::kDnssdRestartNeeded,

  //! Signals that bindings were updated.
  MATTER_BINDINGS_CHANGED_VIA_CLUSTER = (uint16_t)chip::DeviceLayer::DeviceEventType::kBindingsChangedViaCluster,

  //! Signals that the state of the OTA engine changed.
  MATTER_OTA_STATE_CHANGED = (uint16_t)chip::DeviceLayer::DeviceEventType::kOtaStateChanged,

  //! Server initialization has completed. All server components are initialized and the node is ready to establish connections.
  MATTER_SERVER_READY = (uint16_t)chip::DeviceLayer::DeviceEventType::kServerReady,

  //! Signals that the BLE stack is deinitialized and memory has been reclaimed.
  MATTER_BLE_DEINITIALIZED = (uint16_t)chip::DeviceLayer::DeviceEventType::kBLEDeinitialized,

  //! Signals that a secure session (PASE or CASE) is established.  // codespell:ignore
  MATTER_SECURE_SESSION_ESTABLISHED = (uint16_t)chip::DeviceLayer::DeviceEventType::kSecureSessionEstablished,

  //! Signals that factory reset has started.
  MATTER_FACTORY_RESET = (uint16_t)chip::DeviceLayer::DeviceEventType::kFactoryReset,

  // ESP-Matter platform specific events, from kRange_PublicPlatformSpecific + 0x1000.
  // Defined in esp_matter.h

  //! Signals that a commissioning session has started.
  MATTER_COMMISSIONING_SESSION_STARTED = (uint16_t)chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted,

  //! Signals that a commissioning session has stopped.
  MATTER_COMMISSIONING_SESSION_STOPPED = (uint16_t)chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped,

  //! Signals that the commissioning window is now opened.
  MATTER_COMMISSIONING_WINDOW_OPEN = (uint16_t)chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened,

  //! Signals that the commissioning window is now closed.
  MATTER_COMMISSIONING_WINDOW_CLOSED = (uint16_t)chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed,

  //! Signals that a fabric is about to be deleted. Allows actions that still need the fabric present.
  MATTER_FABRIC_WILL_BE_REMOVED = (uint16_t)chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved,

  //! Signals that a fabric is effectively deleted.
  MATTER_FABRIC_REMOVED = (uint16_t)chip::DeviceLayer::DeviceEventType::kFabricRemoved,

  //! Signals that a fabric in the Fabric Table is persisted to storage by CommitPendingFabricData.
  MATTER_FABRIC_COMMITTED = (uint16_t)chip::DeviceLayer::DeviceEventType::kFabricCommitted,

  //! Signals that operational credentials changed, which may not be persistent.
  MATTER_FABRIC_UPDATED = (uint16_t)chip::DeviceLayer::DeviceEventType::kFabricUpdated,

  //! ESP32 Matter events as defined in CHIPDevicePlatformEvent.h.
  MATTER_ESP32_PUBLIC_SPECIFIC_EVENT = (uint16_t)chip::DeviceLayer::DeviceEventType::kRange_PublicPlatformSpecific,  // ESPSystemEvent
  MATTER_ESP32_SPECIFIC_EVENT = MATTER_ESP32_PUBLIC_SPECIFIC_EVENT,
};

/**
 * @brief Runtime network selection for Arduino Matter.
 *
 * @c MATTER_NETWORK_NONE is the default: existing sketches are unchanged.
 * Call @ref ArduinoMatter::selectNetwork before any accessory @c begin().
 * Records intent only; it does not start Wi-Fi, Thread, or Ethernet.
 */
enum matterNetwork_t {
  MATTER_NETWORK_NONE = 0,  ///< No selection; wait-for-network accepts any interface.
  MATTER_NETWORK_WIFI,      ///< Wi-Fi station (Network Commissioning on endpoint 0).
  MATTER_NETWORK_THREAD,    ///< Matter-over-Thread (Network Commissioning on endpoint 0).
  MATTER_NETWORK_ETHERNET   ///< Ethernet (no Network Commissioning cluster; CHIPoBLE off).
};

using namespace esp_matter;

/**
 * @brief Global Matter singleton — the entry point for commissioning, identity, and network APIs.
 *
 * There is only one instance; sketches use the global @c Matter object. It handles device
 * commissioning (QR / manual pairing code), network connectivity, event callbacks, identity
 * (Basic Information), CHIPoBLE policy, and factory reset.
 *
 * Call identity and network setters before @ref begin. Start accessory endpoints first, then
 * @ref begin. Pairing-code getters are empty until @ref isStackStarted is true.
 *
 * Sketch helpers such as @c matterWaitUntilReady() live in MatterHelpers.h (pulled in by
 * Matter.h) and are not members of this class.
 *
 * @note Do not use the Arduino BLE library (@c BLE.h) in the same sketch. Matter owns CHIPoBLE.
 */
class ArduinoMatter {
public:
  /**
   * @brief Callback invoked for Matter device-layer events.
   * @param event Event type.
   * @param eventData CHIP event payload; valid only during this call — do not store it.
   */
  using matterEventCB = std::function<void(matterEvent_t, const chip::DeviceLayer::ChipDeviceEvent *)>;
  static matterEventCB _matterEventCB;
  /**
   * @brief Register a callback for Matter events.
   *
   * The ChipDeviceEvent pointer is only valid during the callback; do not store it.
   *
   * @param cb Handler to invoke, or an empty function to clear.
   */
  static void onEvent(matterEventCB cb) {
    _matterEventCB = cb;
  }

  /**
   * @brief Callback invoked when CHIPoBLE BLE RAM has been returned to the heap.
   *
   * Fires at the same moment as @c MATTER_BLE_DEINITIALIZED. Register before @ref begin.
   * Runs on the CHIP task: do not block; set a flag and allocate large buffers from @c loop().
   * May never run if CHIPoBLE is off or BLE memory release is disabled.
   */
  using bleMemoryReleasedCB = std::function<void()>;
  static bleMemoryReleasedCB _bleMemoryReleasedCB;
  /**
   * @brief Register a callback for CHIPoBLE BLE RAM reclaim.
   *
   * Register before @ref begin. Runs on the CHIP task.
   *
   * @param cb Handler to invoke, or an empty function to clear.
   */
  static void onBLEMemoryReleased(bleMemoryReleasedCB cb) {
    _bleMemoryReleasedCB = cb;
  }

  /**
   * @brief Get the manual pairing code for commissioning.
   *
   * Generated after a successful @ref begin from CommissionableDataProvider.
   * Before @ref begin, or if @ref begin failed, logs a warning and returns an empty String.
   *
   * @return Manual pairing code, or an empty String if the stack is not started.
   */
  static String getManualPairingCode();
  /**
   * @brief Get the onboarding QR code URL for commissioning.
   *
   * Generated after a successful @ref begin. Empty and a warning before @ref begin
   * or if @ref begin failed.
   *
   * @return QR code URL, or an empty String if the stack is not started.
   */
  static String getOnboardingQRCodeUrl();
  /**
   * @brief Create the Matter node (root endpoint 0) if it does not already exist.
   *
   * Idempotent. Normally called from @c MatterEndPoint::ensureMatterNode() inside
   * endpoint @c begin(); sketches rarely call this directly.
   *
   * @return true if the node exists or was created, false otherwise.
   */
  static bool initNode();

  /**
   * @brief Start the Matter stack.
   *
   * Call after at least one accessory endpoint exists on the node (endpoint id ≠ 0).
   * On Wi-Fi station builds with no Thread/Ethernet selection, initializes Wi-Fi first
   * with reduced RX/TX buffers so CHIP inherits those counts (first @c esp_wifi_init wins),
   * unless the sketch already called @c matterConnectWiFi() / @c WiFi.begin() / @c WiFi.mode().
   */
  static void begin();

  /**
   * @brief Set Basic Information VendorName. Call before @ref begin.
   *
   * The string is copied; the argument need not outlive the call. Maximum 32 characters.
   *
   * @param name Vendor name.
   * @return true if stored, false after @ref begin or on invalid input.
   */
  static bool setVendorName(const char *name);
  /**
   * @brief Set Basic Information ProductName. Call before @ref begin.
   *
   * The string is copied; the argument need not outlive the call. Maximum 32 characters.
   *
   * @param name Product name.
   * @return true if stored, false after @ref begin or on invalid input.
   */
  static bool setProductName(const char *name);
  /**
   * @brief Set Basic Information NodeLabel (device name). Call before @ref begin.
   *
   * The string is copied; the argument need not outlive the call. Maximum 32 characters.
   * On a single-endpoint node, controllers often use this as the accessory title.
   *
   * @param name Device / node label.
   * @return true if stored, false after @ref begin or on invalid input.
   */
  static bool setDeviceName(const char *name);
  /**
   * @brief Set Basic Information SerialNumber. Call before @ref begin.
   *
   * The string is copied; the argument need not outlive the call. Maximum 32 characters.
   *
   * @param value Serial number.
   * @return true if stored, false after @ref begin or on invalid input.
   */
  static bool setSerialNumber(const char *value);
  /**
   * @brief Set Basic Information HardwareVersion. Call before @ref begin.
   * @param version Hardware version number.
   * @return true if stored, false after @ref begin.
   */
  static bool setHardwareVersion(uint16_t version);
  /**
   * @brief Set Basic Information HardwareVersionString. Call before @ref begin.
   *
   * The string is copied; the argument need not outlive the call. Maximum 64 characters.
   *
   * @param value Hardware version string.
   * @return true if stored, false after @ref begin or on invalid input.
   */
  static bool setHardwareVersionString(const char *value);
  /**
   * @brief Set Basic Information SoftwareVersion. Call before @ref begin.
   *
   * Firmware version shown by controllers. In-memory only; CHIP StoreSoftwareVersion
   * is unsupported on ESP32.
   *
   * @param version Software version number.
   * @return true if stored, false after @ref begin.
   */
  static bool setSoftwareVersion(uint32_t version);
  /**
   * @brief Set Basic Information SoftwareVersionString. Call before @ref begin.
   *
   * Maximum 64 characters. Default is the IDF app version.
   *
   * @param value Software version string.
   * @return true if stored, false after @ref begin or on invalid input.
   */
  static bool setSoftwareVersionString(const char *value);
  /**
   * @brief Get Basic Information SoftwareVersion.
   *
   * May be called after @ref begin. Without a setter, returns @c CONFIG_DEVICE_SOFTWARE_VERSION_NUMBER.
   *
   * @return Software version number.
   */
  static uint32_t getSoftwareVersion();
  /**
   * @brief Get Basic Information SoftwareVersionString.
   *
   * May be called after @ref begin. Without a setter, returns the IDF app version.
   *
   * @return Software version string.
   */
  static String getSoftwareVersionString();

  /**
   * @brief Set the commissioning setup discriminator. Call before @ref begin.
   *
   * Range 0–0xFFF. Arduino test default is 0xF00.
   *
   * @param discriminator Setup discriminator.
   * @return true if stored, false after @ref begin or on invalid input.
   */
  static bool setSetupDiscriminator(uint16_t discriminator);
  /**
   * @brief Set the commissioning setup passcode (PIN). Call before @ref begin.
   *
   * Arduino test default is 20202021. Changing the PIN regenerates the SPAKE2+ verifier.
   *
   * @param passcode Valid setup PIN.
   * @return true if stored, false after @ref begin or on invalid input.
   */
  static bool setSetupPasscode(uint32_t passcode);

  /**
   * @brief Enable or disable CHIPoBLE commissioning. Call before @ref begin.
   *
   * Default is true only when @c CONFIG_ENABLE_CHIPOBLE is set. @c false forces
   * on-network commissioning (connect Wi-Fi or Ethernet first). BLE RAM is released
   * after @ref begin, not at this call. Commissioning sketches typically use
   * @ref selectNetwork with @c disableBLECommissioning true instead of this setter.
   *
   * @param enabled true to leave CHIPoBLE on, false to disable it.
   * @return true if the flag was stored, false if CHIPoBLE is compiled out or called too late.
   * @note Do not use the Arduino BLE library (@c BLE.h) in a Matter sketch.
   */
  static bool setBLECommissioningEnabled(bool enabled);

  /**
   * @brief After CHIPoBLE commissioning, release BLE RAM. Call before @ref begin.
   *
   * Default true. Only takes effect when @c CONFIG_ENABLE_CHIPOBLE is set and CHIPoBLE
   * commissioning is enabled. No effect if CHIPoBLE is compiled out. Arduino-as-IDF-component
   * builds that keep BLE must also set @c CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING=n.
   *
   * @param enabled true to reclaim BLE RAM after commissioning, false to keep NimBLE.
   * @return true if the flag was stored, false otherwise.
   */
  static bool setBLEMemoryReleaseEnabled(bool enabled);

  /**
   * @brief Whether Wi-Fi Station mode is compiled in (@c CONFIG_ENABLE_WIFI_STATION).
   *
   * False on ESP32-H2. Not "the interface is up".
   *
   * @return true if Wi-Fi station is supported in this build.
   */
  static bool isWiFiStationEnabled();
  /**
   * @brief Whether Wi-Fi AP mode is compiled in (@c CONFIG_ENABLE_WIFI_AP).
   * @return true if Wi-Fi AP is supported in this build.
   */
  static bool isWiFiAccessPointEnabled();
  /**
   * @brief Whether Matter-over-Thread is compiled in (@c CONFIG_ENABLE_MATTER_OVER_THREAD).
   *
   * True on ESP32-C6 / ESP32-S31 / ESP32-H2, and on ESP32-C5 when Tools → Matter Network → Thread.
   * Not "OpenThread is compiled in".
   *
   * @return true if Matter-over-Thread is enabled in this build.
   */
  static bool isThreadEnabled();
  /**
   * @brief Whether the ETH library is compiled in (@c CONFIG_ETH_ENABLED).
   *
   * Not "cable present".
   *
   * @return true if Ethernet support is built.
   */
  static bool isEthernetEnabled();
  /**
   * @brief Whether CHIPoBLE is compiled in and still enabled.
   * @return true if BLE commissioning is available and has not been turned off.
   */
  static bool isBLECommissioningEnabled();
  /**
   * @brief Whether CHIPoBLE is on and BLE RAM will be released after commissioning.
   * @return true if post-commission BLE reclaim is enabled.
   */
  static bool isBLEMemoryReleaseEnabled();

  /**
   * @brief Whether @a network is supported in this build (same as the matching @c is*Enabled()).
   * @param network Network to query. @c MATTER_NETWORK_NONE is always false.
   * @return true if that transport is compiled in.
   */
  static bool isNetworkSupported(matterNetwork_t network);
  /**
   * @brief Record the intended operational network. Call before any accessory @c begin().
   *
   * Does not start Wi-Fi, Thread, or Ethernet, and does not apply a Thread dataset.
   * Ethernet disables CHIPoBLE; Wi-Fi and Thread leave it on. @c MATTER_NETWORK_NONE
   * clears the selection and does not change BLE.
   *
   * On ESP32-C6 / ESP32-S31, @c MATTER_NETWORK_THREAD replaces the root Wi-Fi Network
   * Commissioning cluster so hubs that only talk to endpoint 0 see Thread.
   *
   * @param network Network to select.
   * @return true if the selection is valid for this build.
   */
  static bool selectNetwork(matterNetwork_t network);
  /**
   * @brief Record the intended operational network and optionally disable CHIPoBLE.
   *
   * @a disableBLECommissioning true turns CHIPoBLE off. false does not turn it back on
   * if the sketch already disabled it. Do not also call @ref setBLECommissioningEnabled.
   *
   * @param network Network to select.
   * @param disableBLECommissioning true to force on-network commissioning (CHIPoBLE off).
   * @return true if the selection is valid for this build.
   */
  static bool selectNetwork(matterNetwork_t network, bool disableBLECommissioning);
  /**
   * @brief Last successful @ref selectNetwork value, or @c MATTER_NETWORK_NONE.
   * @return Selected network.
   */
  static matterNetwork_t getSelectedNetwork();
  /**
   * @brief Interface that currently has a usable IPv6 address.
   *
   * Prefers the selection if that netif is up; otherwise Wi-Fi, Thread, then Ethernet.
   * Not the same as @ref isWiFiConnected / @ref isThreadConnected.
   *
   * @return Active network, or @c MATTER_NETWORK_NONE if none has IPv6.
   */
  static matterNetwork_t getActiveNetwork();
  /**
   * @brief Expected Network Commissioning endpoint id, even before the endpoint is created.
   *
   * Wi-Fi: 0, or 0xFFFF when Thread replaced the root driver (ESP32-C6 / ESP32-S31).
   * Thread: 0 when Thread is on the root. Ethernet / not selected / unsupported: 0xFFFF.
   *
   * @param network Network to query.
   * @return Endpoint id, or 0xFFFF if none.
   */
  static uint16_t getNetworkEndPointId(matterNetwork_t network);
  /**
   * @brief Block until the selected interface has IPv6.
   *
   * Does not start hardware. @c MATTER_NETWORK_NONE waits for any interface.
   * @a timeoutMs 0 is a single check.
   *
   * @param timeoutMs Maximum wait in milliseconds; 0 polls once.
   * @return true if IPv6 is present, false on timeout.
   */
  static bool waitForNetwork(uint32_t timeoutMs);

  /**
   * @brief Whether @ref begin completed successfully.
   * @return true only after a successful @ref begin.
   */
  static bool isStackStarted();
  /**
   * @brief Whether the device is commissioned (a Matter fabric exists).
   * @return true if a fabric is present.
   */
  static bool isDeviceCommissioned();
  /**
   * @brief Whether the Wi-Fi station is associated.
   * @return true if Wi-Fi is connected.
   */
  static bool isWiFiConnected();
  /**
   * @brief Whether Thread is attached.
   * @return true if Thread is connected.
   */
  static bool isThreadConnected();
  /**
   * @brief Whether Wi-Fi, Thread, or Ethernet IPv6 is up.
   * @return true if the device has operational IP connectivity.
   */
  static bool isDeviceConnected();
  /**
   * @brief Whether a controller has an active CASE session with this node.
   *
   * Stays true until CHIP idle-evicts that session, not until the user closes a
   * controller app. Do not gate physical outputs on this; use it for hub-discovery logs.
   *
   * @return true if an active CASE session exists.
   */
  static bool isOnline();
  /**
   * @brief Factory-reset the device (erase fabrics and commissioning data).
   *
   * The device must be commissioned again afterwards.
   */
  static void decommission();

protected:
  static bool ensureSetBeforeBegin(const char *apiName);
  static bool storeIdentityString(char *dst, size_t dstSize, const char *src, const char *apiName);
  static void applyIdentityBeforeStart();
  static void applyIdentityAfterStart();
  static void applyBlePolicyAfterStart();
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_MATTER)
/**
 * @brief Global Arduino Matter singleton. Sketches use this object directly.
 */
extern ArduinoMatter Matter;
#endif

#include <MatterHelpers.h>

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
