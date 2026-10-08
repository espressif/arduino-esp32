# Arduino Matter Library

Arduino-friendly wrapper around [ESP-Matter](https://docs.espressif.com/projects/esp-matter/en/latest/) (Espressif's SDK for Matter), providing high-level endpoint classes for common Matter device types.

**Do not use the Arduino `BLE` library (`BLE.h`) in a Matter sketch.** When CHIPoBLE is compiled in (`CONFIG_ENABLE_CHIPOBLE`) Matter owns the BLE host (NimBLE if `CONFIG_BT_NIMBLE_ENABLED`). After `Matter.begin()`, `BLE.begin()` will fail or crash: CHIPoBLE is running, you turned it off with `setBLECommissioningEnabled(false)` / `selectNetwork(..., true)` (BLE RAM is released after `begin()`, not at that call), or CHIPoBLE commissioning finished with the default `setBLEMemoryReleaseEnabled(true)`. Turning CHIPoBLE off does not hand the radio to Arduino BLE.

On SoCs with PSRAM initialized (SPIRAM heap present), the data model (endpoints, clusters, attributes) is allocated in PSRAM so many-endpoint nodes leave internal/DMA RAM for CHIPoBLE and AES. `ESP.getFreeHeap()` can stay flat while `ESP.getFreePsram()` drops; that is expected. CHIP, AES bounce buffers, and NimBLE still use `malloc` / DMA and are not moved to PSRAM. Without a SPIRAM heap the same objects stay in internal RAM.

## Architecture

Each Matter device type is represented by a C++ class under `src/MatterEndpoints/` (e.g., `MatterOnOffLight`, `MatterTemperatureSensor`, `MatterFan`). These classes manage:

- **Internal state variables** — C++ members that cache the device's current state (e.g., `onOffState`, `brightnessLevel`, `rawTemperature`).
- **Matter attribute store** — The ESP-Matter SDK's attribute database, which is the protocol-level representation read by controllers and used for subscriptions/reporting.

### Custom endpoints (esp-matter device types not wrapped in-tree)

Subclass `MatterEndPoint`, implement `attributeChangeCB`, and in your subclass `begin()`:

1. Call `ensureMatterNode()` (protected; only valid inside the subclass) or, from sketch code that does not subclass `MatterEndPoint`, `Matter.initNode()`.
2. Create the endpoint with `esp_matter::endpoint::*::create(node::get(), …, ENDPOINT_FLAG_NONE, (void *)this)`.
3. `setEndPointId(endpoint::get_id(ep))`, or use `registerCreatedEndpoint(ep)` after a `create()` that already passed `(void *)this`.

Use the same Ember setter/getter patterns as stock endpoints. Override `onStackStarted()` when you need code-driven clusters after `Matter.begin()`. Sketch order is unchanged: accessory `begin()` / `setTagList()` → `Matter.begin()`. `Matter.begin()` requires at least one accessory endpoint (id ≠ 0) on the node.

Full walkthrough: [`MatterCustomEndpoint`](examples/Advanced/MatterCustomEndpoint) builds a **PM2.5 sensor** to illustrate creating and using user custom endpoints (Air Quality Sensor device type plus PM2.5 cluster). The sketch subclasses `MatterEndPoint`, calls your endpoint `begin()`, then `Matter.begin()`:

```cpp
MyCustomMatterEndpoint myEndpoint;

void setup() {
  myEndpoint.begin(/* initial state */);
  Matter.begin();
}
```

## Attribute Update Pattern

This library is built on ESP-Matter, which uses an Ember-based attribute store. The attribute store is the protocol-level source of truth: when a Matter controller reads an attribute, it reads from this store.

Most endpoint setters and getters **must** follow one of the two Ember store patterns below. Exceptions:

- Boolean State sensors: `StateValue` lives on the code-driven cluster. `updateAttributeVal()` only updates the shadow table; use `setBooleanStateValue()` / the endpoint setter. Values set before `Matter.begin()` are cached and applied when the stack starts.
- Color lights: `setColorHSV()` / `setColorRGB()` write several Color Control attributes plus CurrentLevel using `attribute::report()` so a local set fires a single `onChangeColorHSV` callback instead of one per attribute.

### Setter Pattern

When the device application (Arduino sketch) wants to change state, the setter **must** update the attribute store first and only commit to internal state after the store confirms success.

```cpp
bool MyEndpoint::setValue(int newValue) {
  if (internalValue == newValue) {
    return true;  // No change needed
  }

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  if (!getAttributeVal(ClusterId, AttributeId, &val)) {
    log_e("Failed to get attribute.");
    return false;
  }

  if (val.val.i16 != newValue) {
    val.val.i16 = newValue;
    bool ret = updateAttributeVal(ClusterId, AttributeId, &val);
    if (!ret) {
      log_e("Failed to update attribute.");
      return false;
    }
    // Internal state committed ONLY after attribute store update succeeds
    internalValue = newValue;
  }

  return true;
}
```

Key rules:
- **Never assign internal state before the attribute store update**, except for the color HSV/RGB setters described below. If `updateAttributeVal()` fails and the function returns `false`, the internal state must remain unchanged.
- **Internal state assignment goes inside the `if (value changed)` block**, after the successful update call.
- When `updateAttributeVal()` may fail for nullable or dynamic attributes, a `setAttributeVal()` fallback can be used (see `MatterWindowCovering`, `MatterTemperatureControlledCabinet`). `setAttributeVal()` writes to the store without triggering the `PRE_UPDATE` callback chain or subscriber reporting.
- For composite setters that update multiple attributes, attempt all sub-updates and return the combined result. Partial commits are possible (some attributes updated, others not) since true atomic rollback is not feasible with the current ESP-Matter API. Each sub-update independently follows the Ember pattern (internal state only after its own attribute store success).
- **Color HSV/RGB setters** (`MatterColorLight`, `MatterEnhancedColorLight`) write Hue, Saturation, CurrentX, CurrentY, ColorMode, and CurrentLevel with `attribute::report()` (no `PRE_UPDATE`). They update the local HSV cache first, then report, then invoke the user color callback once. CurrentLevel is nullable uint8: valid levels are 1-254; 255 is the null sentinel (`err: 258` if written as a plain uint8). <!-- codespell:ignore currenty -->

### Getter Pattern

Getters return the internal state variable directly, without reading from the attribute store:

```cpp
int MyEndpoint::getValue() {
  return internalValue;
}
```

This is consistent across all endpoints. Since setters only update internal state after the attribute store succeeds, the getter always reflects the last successfully committed value.

### Boolean State Sensors (ESP-Matter 1.5+)

`MatterContactSensor`, `MatterWaterLeakDetector`, `MatterWaterFreezeDetector`, and `MatterRainSensor` use the code-driven Boolean State cluster. `attribute::update()` on `StateValue` only writes the shadow table; the hub still reads the cluster (which starts `false`). Those setters call `MatterEndPoint::setBooleanStateValue()`, which looks up the live cluster and uses `BooleanStateCluster::SetStateValue()`.

```cpp
bool MatterWaterLeakDetector::setLeak(bool _leakState) {
  if (leakState == _leakState) {
    return true;
  }
  if (!setBooleanStateValue(_leakState)) {
    log_e("Failed to update Water Leak Detector Attribute.");
    return false;
  }
  leakState = _leakState;
  return true;
}
```

Key rules:
- **`begin()` takes no initial state.** Value-initialize the config and set `config.boolean_state.state_value = false` so Ember `create()` gets a deterministic default. CHIP's live `BooleanStateCluster` ignores that field and always starts at `false`. A real sensor that is not false must be applied with the setter after `Matter.begin()`.
- **Call the setter after the endpoint `begin()`.** Before `Matter.begin()` the value is cached and pushed when the cluster is created. After start it writes the live cluster. Typical sketches still do `begin()`, `Matter.begin()`, then the setter.
- Do not use `updateAttributeVal()` for Boolean State `StateValue`, and do not use CHIP's `BooleanState::FindClusterOnEndpoint()` (ESP-Matter does not link that helper).

### Controller-Originated Changes (attributeChangeCB)

When a Matter controller changes an attribute (e.g., turning a light on via an app), the flow is:

1. ESP-Matter receives the write and fires a `PRE_UPDATE` callback.
2. `app_attribute_update_cb()` in `Matter.cpp` calls the endpoint's virtual `attributeChangeCB()`.
3. The endpoint's `attributeChangeCB()` updates internal state and, for endpoints that expose user callbacks (e.g., `MatterOnOffLight`, `MatterDimmableLight`, `MatterFan`), invokes `_onChangeCB`.
4. If the callback returns `true` (or if no user callback is registered), internal state is updated and `ESP_OK` is returned, allowing the attribute store to commit the new value.
5. If the callback returns `false`, the change is rejected (`ESP_FAIL`) and both internal state and attribute store remain unchanged.

Note: Not all endpoints expose user callbacks. For example, `MatterTemperatureControlledCabinet::attributeChangeCB()` updates internal state directly and always returns success. Check individual endpoint headers for available `onChange` methods.

### begin() Initialization

The `begin()` method should initialize **both** the Matter config struct and internal state variables:

```cpp
bool MyEndpoint::begin(bool initialState, uint8_t brightness) {
  my_endpoint::config_t config;
  config.on_off.on_off = initialState;
  onOffState = initialState;

  config.level_control.current_level = brightness;
  brightnessLevel = brightness;

  endpoint_t *ep = my_endpoint::create(node::get(), &config, ENDPOINT_FLAG_NONE, (void *)this);
  // ...
}
```

Boolean State sensors (`MatterContactSensor`, `MatterWaterLeakDetector`, `MatterWaterFreezeDetector`, `MatterRainSensor`) are the exception: `begin()` takes no arguments, value-initializes the config, sets `config.boolean_state.state_value = false` (Ember create only; the live cluster ignores this field and starts at `false`), and forces the local cache to `false`. Apply the real sensor with the setter after `Matter.begin()`.

### updateAttributeVal vs setAttributeVal

| API | ESP-Matter function | Triggers PRE_UPDATE callback | Reports to subscribers | Use case |
|-----|-------------------|------------------------------|----------------------|----------|
| `updateAttributeVal()` | `attribute::update()` | Yes | Yes | Normal device-initiated state changes |
| `setAttributeVal()` | `attribute::set_val()` | No | No | Fallback for nullable attributes, syncing related attributes inside callbacks, avoiding re-entrancy |
| Color HSV `report()` | `attribute::report()` | No | Yes | `setColorHSV()` / `setColorRGB()`: notify subscribers without re-entering `attributeChangeCB` |

## Shared endpoint API

All device classes inherit `MatterEndPoint`. After `begin()` and before `Matter.begin()`, sketches can call `setTagList()` with `MatterTags` presets (Number, Location, Position, Switches) to set the Descriptor TagList. At most 3 tags per endpoint. See [`MatterSmartButtonsTagList`](examples/Control/MatterSmartButtonsTagList). TagList does not set a light's display name.

## Endpoint Classes

**Lighting**

| Class | Device Type |
|-------|-------------|
| `MatterOnOffLight` | On/Off Light |
| `MatterDimmableLight` | Dimmable Light |
| `MatterColorTemperatureLight` | Color Temperature Light |
| `MatterColorLight` | Color Light (HSV/XY, no color temperature) |
| `MatterEnhancedColorLight` | Extended Color Light (HSV/XY + color temperature) |

**Sensors**

| Class | Device Type |
|-------|-------------|
| `MatterTemperatureSensor` | Temperature Sensor |
| `MatterHumiditySensor` | Humidity Sensor |
| `MatterPressureSensor` | Pressure Sensor |
| `MatterLightSensor` | Light / Illuminance Sensor |
| `MatterOccupancySensor` | Occupancy Sensor (optional HoldTime) |
| `MatterContactSensor` | Contact Sensor (Boolean State) |
| `MatterWaterLeakDetector` | Water Leak Detector (Boolean State) |
| `MatterWaterFreezeDetector` | Water Freeze Detector (Boolean State) |
| `MatterRainSensor` | Rain Sensor (Boolean State) |

**Control and other**

| Class | Device Type |
|-------|-------------|
| `MatterOnOffPlugin` | On/Off Plug-in Unit |
| `MatterDimmablePlugin` | Dimmable Plug-in Unit |
| `MatterFan` | Fan |
| `MatterGenericSwitch` | Generic Switch (smart button — short click, long press, multi-press) |
| `MatterThermostat` | Thermostat |
| `MatterWindowCovering` | Window Covering |
| `MatterTemperatureControlledCabinet` | Temperature Controlled Cabinet |

## Node identity and commissioning

Call these on the `Matter` singleton **before** `Matter.begin()`. After `begin()` they log a warning and do nothing. String setters **copy** into internal storage (stack or `String` temporaries are safe). `setDeviceName()` writes Basic Information NodeLabel (not a per-light name). Do not change Vendor ID / Product ID unless the DAC matches. `SoftwareVersion` / `SoftwareVersionString` are Basic Information from ConfigurationManager (Alexa “version”), not `HardwareVersion`. They are stored in RAM for this boot; CHIP does not persist them on ESP32.

Vendor, product, serial, and hardware strings go through Arduino's instance-info wrapper when `CONFIG_CUSTOM_DEVICE_INSTANCE_INFO_PROVIDER` is set. `CONFIG_EXAMPLE_DEVICE_INSTANCE_INFO_PROVIDER` leaves CHIP's generic provider and does not publish those Arduino names. Factory or secure-cert instance-info providers take priority over the wrapper. Factory NVS still owns per-unit PIN and DAC.

FixedLabel and UserLabel (Generic Switch) need CHIP's `DeviceInfoProvider`. If the build uses `CONFIG_NONE_DEVICE_INFO_PROVIDER`, `Matter.begin()` registers Arduino's RAM provider before the stack starts. CHIP's `ESP32DeviceInfoProvider` is not linked unless factory data is enabled. Factory or custom device-info providers still replace it.

Most examples call `matterSetExampleIdentity("Color Light")` (or the matching endpoint name) for vendor `Espressif` and product `<SoC> <endpoint>`, for example `ESP32-C6 Color Light`. ProductName is capped at 32 characters. `MatterMinimum` skips this and keeps CHIP defaults. Override with the setters below, as in Matter Device Identity.

```cpp
matterSetExampleIdentity("Color Light");      // vendor Espressif, product "<SoC> Color Light"
Matter.setVendorName("Espressif");            // max 32
Matter.setProductName("KitchenLight");        // max 32
Matter.setDeviceName("KitchenHub");           // NodeLabel, max 32
Matter.setSerialNumber("KH-000123");          // max 32
Matter.setHardwareVersion(7);
Matter.setHardwareVersionString("RevA");      // max 64
Matter.setSoftwareVersion(7);                 // Basic Information SoftwareVersion (uint32)
Matter.setSoftwareVersionString("1.0.7");     // max 64; default is the IDF app version
Matter.setSetupDiscriminator(0xF01);          // 0–0xFFF
Matter.setSetupPasscode(20202024);            // valid Matter PIN
// Prefer selectNetwork(MATTER_NETWORK_WIFI or MATTER_NETWORK_THREAD, true) to pick a transport and turn CHIPoBLE off.
// setBLECommissioningEnabled(false) only if you keep the default network and just want BLE off.
Matter.setBLECommissioningEnabled(false);
// Matter.setBLEMemoryReleaseEnabled(false);  // only if CHIPoBLE is left on: keep NimBLE after commission
Light.begin();
Matter.begin();
Serial.println(Matter.getManualPairingCode());     // live code after begin()
Serial.println(Matter.getOnboardingQRCodeUrl());   // live QR URL after begin()
```

If the sketch never calls `setSetupPasscode()` / `setSetupDiscriminator()`, Arduino Matter uses the CHIP test pair **PIN `20202021`**, discriminator **`0xF00`**, manual code **`34970112332`** (same as On/Off Light and the other examples). Before `begin()`, or if `begin()` failed (`isStackStarted()` is false), the pairing getters log a warning and return empty.

On Wi-Fi station builds, `Matter.begin()` initializes the Wi-Fi driver with reduced RX/TX buffers before starting CHIP unless Thread or Ethernet was selected. Matter traffic is small, so the library uses 4 static RX, 8 dynamic RX, 8 dynamic TX, and an AMPDU RX BA window of 6 instead of the sdkconfig defaults. `esp_wifi_init()` keeps the first caller's counts, so CHIP inherits them. If the sketch already called `matterConnectWiFi()` / `WiFi.begin()` / `WiFi.mode()`, those limits are not applied.

Dual-stack images (C5/C6) still run CHIP's `InitWiFiStack()` inside `esp_matter::start()` — `InitChipStack()` needs the Wi-Fi controller even when Thread is selected. Arduino does not call `InitWiFiStack()` itself (that must happen after the default event loop exists). After `Matter.begin()`, Thread and Ethernet disable the Wi-Fi station so a leftover SSID does not join.

Commissioning examples turn CHIPoBLE off with `selectNetwork(MATTER_NETWORK_WIFI, true)` / `selectNetwork(MATTER_NETWORK_THREAD, true)` (or one-arg `selectNetwork(MATTER_NETWORK_ETHERNET)`). Do not also call `setBLECommissioningEnabled()`. That setter is only when you keep the default network and just want BLE off. Pairing codes are then on-network only. See [`MatterOnNetworkWiFi`](examples/Commissioning/MatterOnNetworkWiFi). With CHIPoBLE left on, BLE RAM is released after a successful commission by default (`setBLEMemoryReleaseEnabled(true)`); see [`MatterCHIPoBLERelease`](examples/Commissioning/MatterCHIPoBLERelease). Call `setBLEMemoryReleaseEnabled(false)` before `begin()` to keep the BLE host after CHIPoBLE commissioning. That option has no effect when `CONFIG_ENABLE_CHIPOBLE` is off.

## Network selection

Call `Matter.selectNetwork()` **before any accessory `begin()`**. No call (`MATTER_NETWORK_NONE`) keeps today's behavior.

| Network | `isNetworkSupported` | CHIPoBLE default | Notes |
| --- | --- | --- | --- |
| Wi-Fi | `CONFIG_ENABLE_WIFI_STATION` (all Matter targets except ESP32-H2) | On | Primary commissioning cluster at endpoint 0 |
| Thread | `CONFIG_ENABLE_MATTER_OVER_THREAD` (**ESP32-C6 and ESP32-H2**; **ESP32-C5** with Tools → Matter Network → Thread) | On | Thread Network Commissioning on endpoint 0. C5 is H2-style (Wi-Fi station off in that Matter `.a`). ESP32-C6 is Wi-Fi **or** Thread on the root (`selectNetwork(MATTER_NETWORK_THREAD)` replaces the prebuild Wi-Fi driver). `createSecondaryNetworkInterface()` is deprecated and does nothing. |
| Ethernet | `CONFIG_ETH_ENABLED` (capable if you wire hardware) | Off | No commissioning cluster. Sketch starts `ETH` (EMAC or SPI), `enableIPv6()`, then `Matter.waitForNetwork()` |

`selectNetwork(network, disableBLECommissioning)` overrides the BLE default. The library does **not** call `ETH.begin()` (PHY macros are sketch-local: EMAC `ETH.begin()`, or `SPI.begin()` plus `ETH.begin(..., SPI)`). Do **not** start Arduino `ESPmDNS` — CHIP owns mDNS; `MDNS.begin()` / `MDNS.end()` break Matter discovery.

After `Matter.begin()`, `OThread.begin()` attaches to CHIP's Thread stack (`isAttachedToExternalStack()`) and `OThread.end()` must not tear that stack down.

Do not mix the two paths: CHIPoBLE plus a sketch SSID/dataset fights the hub; CHIPoBLE off plus no credentials is a dead end.

### Which commissioning example?

Same On/Off Light in all of these. The only difference is how the node gets onto the network.

| Example | Transport | CHIPoBLE | Credentials in the sketch | When to use |
| --- | --- | --- | --- | --- |
| [`MatterCHIPoBLEWiFi`](examples/Commissioning/MatterCHIPoBLEWiFi) | Wi-Fi | On | No. Hub sends SSID/password | Factory-fresh Wi-Fi node |
| [`MatterOnNetworkWiFi`](examples/Commissioning/MatterOnNetworkWiFi) | Wi-Fi | Off | Yes. `selectNetwork(MATTER_NETWORK_WIFI, true)` then `WiFi.begin(ssid, password)` | Already on Wi-Fi, or no BLE |
| [`MatterCHIPoBLEThread`](examples/Commissioning/MatterCHIPoBLEThread) | Thread | On | No. Hub sends the dataset | Factory-fresh Thread node (ESP32-C5 / ESP32-C6 / ESP32-H2) |
| [`MatterOnNetworkThread`](examples/Commissioning/MatterOnNetworkThread) | Thread | Off | Yes. Network key after `Matter.begin()` | Already on the mesh |
| [`MatterOnNetworkEthernet`](examples/Commissioning/MatterOnNetworkEthernet) | Ethernet | Off | EMAC or SPI `ETH.begin()` + IPv6 first | Wired only (no commissioning cluster) |
| [`MatterCHIPoBLERelease`](examples/Commissioning/MatterCHIPoBLERelease) | Default (Wi-Fi; Thread on ESP32-C5 / ESP32-C6 / ESP32-H2) | On, then reclaimed | No | Same BLE path as the default accessory, plus heap reclaim |
| [`MatterEarlyBLERelease`](examples/Advanced/MatterEarlyBLERelease) | 1: Wi-Fi / 0: default | 1: off at boot / 0: CHIPoBLE then reclaim | 1: sketch SSID / 0: hub over BLE | One On/Off Light; `[heap]` shows internal RAM and PSRAM |

[`MatterOnOffLight`](examples/Lighting/MatterOnOffLight) is the generic accessory demo: CHIPoBLE when compiled in, otherwise sketch Wi-Fi credentials. Use the table above when you care about the commissioning path.

Register `Matter.onBLEMemoryReleased()` **before** `Matter.begin()` if the sketch must allocate a large buffer only after reclaim. The callback runs on the CHIP task when `kBLEDeinitialized` is posted (same moment as `MATTER_BLE_DEINITIALIZED`). Set a flag there and `malloc` from `loop()`. Reclaim is not instant: releasing the BLE regions while the NimBLE host task still runs would corrupt the heap, so the library polls for that task to exit every 2 s for up to 30 s. The callback may never run if CHIPoBLE is off, release is disabled, or the reclaim times out. Do not wait forever.

**Arduino IDE precompiled libraries** (`CONFIG_*` in that Matter static library):

| SoC | Default network | Thread | Ethernet | CHIPoBLE | Notes |
| --- | --- | --- | --- | --- | --- |
| ESP32 | Wi-Fi | No | Yes (EMAC or SPI) | **No** (Bluedroid) | Use [`MatterOnNetworkWiFi`](examples/Commissioning/MatterOnNetworkWiFi) or Ethernet. `setBLECommissioningEnabled(true)` fails. |
| ESP32-S2 | Wi-Fi | No | SPI PHY | **No** (no Bluetooth) | Same as ESP32 for BLE. |
| ESP32-S3 / ESP32-C3 | Wi-Fi | No | SPI PHY | Yes (NimBLE) | `selectNetwork(MATTER_NETWORK_WIFI)` keeps BLE on; `, true` turns it off. |
| ESP32-C5 | Wi-Fi (Tools → Matter Network default) | Yes (Matter Network → Thread) | SPI PHY | Yes (NimBLE) | One `esp32c5/` folder; `.wifi.a` / `.thread.a`. |
| ESP32-C6 | Wi-Fi until `selectNetwork` | Yes (image has both; one NC on endpoint 0) | SPI PHY | Yes (NimBLE) | `selectNetwork(MATTER_NETWORK_THREAD)` puts Thread NC on endpoint 0. Not both. |
| ESP32-H2 | Thread | Yes | SPI PHY | Yes (NimBLE) | No Wi-Fi. |

Ethernet is on-network only: `selectNetwork(MATTER_NETWORK_ETHERNET)` turns CHIPoBLE off. SPI PHYs (W5500, DM9051, KSZ8851SNL) work on every Arduino Matter SoC (tested: ESP32 + W5500). Internal RMII EMAC is original ESP32 only. Wi-Fi and Thread leave CHIPoBLE on when compiled in.

**Arduino as an ESP-IDF component:** enable `CONFIG_BT_ENABLED`, `CONFIG_BT_NIMBLE_ENABLED`, and `CONFIG_ENABLE_CHIPOBLE` — including on original ESP32. Sketches already follow `CONFIG_ENABLE_CHIPOBLE` (not a chip name). To keep BLE after commission, also set `CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING=n`. If Bluetooth is off (`CONFIG_BT_ENABLED=n`), CHIPoBLE is compiled out and the BLE setters are no-ops / return false.

**`Matter` class (runtime):**

| API | Effect |
|-----|--------|
| `isWiFiStationEnabled()` / `isThreadEnabled()` / `isEthernetEnabled()` | Compile-time only. Ethernet is “ETH can build”, not “cable up”. Thread is Matter-over-Thread, not “OpenThread is in the image”. |
| `isNetworkSupported(net)` | Same as the matching `is*Enabled()` above. `NONE` is false. |
| `selectNetwork(net)` | Records intent **before** any accessory `begin()`. Does **not** start a radio or apply a dataset. Ethernet turns CHIPoBLE **off**; Wi-Fi/Thread leave BLE on. `NONE` clears intent and does not change BLE. |
| `selectNetwork(net, disableBLE)` | `true` calls `setBLECommissioningEnabled(false)`. `false` does **not** re-enable BLE. |
| `getSelectedNetwork()` | Last successful `selectNetwork()`, or `NONE`. |
| `getActiveNetwork()` | First netif with IPv6 (prefers the selection). Not `isWiFiConnected()` / `isThreadConnected()`. |
| `getNetworkEndPointId(net)` | Expected root commissioning endpoint (0 Wi-Fi; 0 Thread when Thread is on the root; `0xFFFF` if none). C6 is Wi-Fi or Thread, not both. |
| `Matter.waitForNetwork(ms)` | Blocks until that IPv6 is there. `MATTER_NETWORK_NONE` waits for any. Does not bring up hardware. `0` = one check. |
| `isStackStarted()` | `true` only after a successful `Matter.begin()` |
| `isDeviceCommissioned()` | A Matter fabric exists |
| `isDeviceConnected()` | CHIP Wi-Fi or Thread connected, **or** Ethernet IPv6 |
| `isOnline()` | A controller has an active CASE session (until CHIP idle-evicts it) |
| `isWiFiConnected()` / `isThreadConnected()` | Wi-Fi associated / Thread attached |
| `isWiFiAccessPointEnabled()` | Compile-time Wi-Fi AP support |
| `onEvent()` | Matter `matterEvent_t` callback. `ChipDeviceEvent` is valid only during the call |
| `isBLECommissioningEnabled()` | CHIPoBLE is compiled in and still enabled |
| `isBLEMemoryReleaseEnabled()` | CHIPoBLE is on and BLE RAM will be released after commissioning |
| `onBLEMemoryReleased()` | BLE RAM is back on the heap (register before `begin()`) |

Do not gate LEDs on `isOnline()`. A session can stay up after the user leaves the app. See [`MatterDeviceIdentity`](examples/GettingStarted/MatterDeviceIdentity) and [`MatterStatus`](examples/GettingStarted/MatterStatus).

## Sketch helpers

These are **not** members of `Matter`. `#include <Matter.h>` pulls in `MatterHelpers.h` (and `MatterButton.h`). They print to Serial and may reboot. Do not wait for commissioning in `loop()`.

`matterConnectWiFi()` is compiled only when `CONFIG_ENABLE_CHIPOBLE` is off (ESP32 / ESP32-S2). `MatterHelpers.cpp` then includes `<WiFi.h>`. CHIPoBLE / Thread builds skip that include so the Arduino builder does not link `WiFiClass` (~80 KB flash, ~6 KB BSS). On-network sketches that still start STA with CHIPoBLE compiled in (`MatterOnNetworkWiFi`, EarlyBLE mode 1) include `<WiFi.h>` and call `WiFi.begin()` themselves.

| Helper | Effect |
|--------|--------|
| `matterConnectWiFi(ssid, password)` | `CONFIG_ENABLE_CHIPOBLE=n` only. `setup()` before `Matter.begin()`. Official examples pass `WIFI_SSID` / `WIFI_PASSWORD`. Enables STA IPv6, waits for IPv4. Not on CHIPoBLE or H2 |
| `matterSetExampleIdentity(endpointName)` | `setup()` before `Matter.begin()`. Vendor `Espressif`, product `<SoC> <endpointName>` (for example `ESP32-C6 Color Light`). ProductName max 32 characters |
| `matterWaitUntilReady()` | `setup()` after `Matter.begin()`: if `begin()` failed, prints that and halts. Otherwise pairing codes if needed; one-line status every 10 s and once more when CASE is up; wait up to 5 min (`timeoutMs` 0 = forever). Reboots if still no fabric. If commissioned but CASE never arrives, continues |
| `matterRestartIfNoFabric()` | `loop()`: no-op if the stack never started; reboot if the hub removed the fabric. `Matter.decommission()` already factory-resets |
| `MatterButton` | Board button (`MatterButton.h`). Timer samples the pin; `loop()` drains `poll()` (`PRESS` / `CLICK` / `DOUBLE_CLICK` / `LONG_HOLD`). Default: 50 ms debounce, 5 s long-hold, double-click off. Not a Generic Switch cluster |

```cpp
#include <Matter.h>

MatterButton button;

void setup() {
  button.begin(BOOT_PIN);
  // ... endpoint begin() ...
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();
  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_CLICK) { /* toggle */ }
    else if (ev == MATTER_BUTTON_LONG_HOLD) { Matter.decommission(); }
  }
}
```

Official examples call `matterWaitUntilReady()` in `setup()` and `matterRestartIfNoFabric()` in `loop()`.

## Further Reading

- [Arduino-ESP32 Matter Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [ESP-Matter Programming Guide](https://docs.espressif.com/projects/esp-matter/en/latest/)
- [Matter Specification (CSA)](https://csa-iot.org/developer-resource/specifications-download-request/)
- Examples: [`MatterDeviceIdentity`](examples/GettingStarted/MatterDeviceIdentity), [`MatterStatus`](examples/GettingStarted/MatterStatus), [`MatterOnNetworkWiFi`](examples/Commissioning/MatterOnNetworkWiFi), [`MatterCHIPoBLEWiFi`](examples/Commissioning/MatterCHIPoBLEWiFi), [`MatterOnNetworkEthernet`](examples/Commissioning/MatterOnNetworkEthernet), [`MatterOnNetworkThread`](examples/Commissioning/MatterOnNetworkThread), [`MatterCHIPoBLEThread`](examples/Commissioning/MatterCHIPoBLEThread), [`MatterCHIPoBLERelease`](examples/Commissioning/MatterCHIPoBLERelease), [`MatterEarlyBLERelease`](examples/Advanced/MatterEarlyBLERelease), [`MatterCustomEndpoint`](examples/Advanced/MatterCustomEndpoint), [`MatterSmartButtonsTagList`](examples/Control/MatterSmartButtonsTagList)
