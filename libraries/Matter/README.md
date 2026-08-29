# Arduino Matter Library

Arduino-friendly wrapper around [ESP-Matter](https://docs.espressif.com/projects/esp-matter/en/latest/) (Espressif's SDK for Matter), providing high-level endpoint classes for common Matter device types.

**Do not use the Arduino `BLE` library (`BLE.h` / `BLEDevice`) in a Matter sketch.** When CHIPoBLE is compiled in (`CONFIG_ENABLE_CHIPOBLE`) Matter owns the BLE host (NimBLE if `CONFIG_BT_NIMBLE_ENABLED`). After `Matter.begin()`, after `setBLECommissioningEnabled(false)`, or after CHIPoBLE commissioning with the default `setBLEMemoryReleaseEnabled(true)`, `BLEDevice::init()` will fail or crash. Turning CHIPoBLE off does not hand the radio to Arduino BLE.

## Architecture

Each Matter device type is represented by a C++ class under `src/MatterEndpoints/` (e.g., `MatterOnOffLight`, `MatterTemperatureSensor`, `MatterFan`). These classes manage:

- **Internal state variables** — C++ members that cache the device's current state (e.g., `onOffState`, `brightnessLevel`, `rawTemperature`).
- **Matter attribute store** — The ESP-Matter SDK's attribute database, which is the protocol-level representation read by controllers and used for subscriptions/reporting.

## Attribute Update Pattern

This library is built on ESP-Matter, which uses an Ember-based attribute store. The attribute store is the protocol-level source of truth: when a Matter controller reads an attribute, it reads from this store.

Most endpoint setters and getters **must** follow one of the two Ember store patterns below. Exceptions:

- Boolean State sensors: `StateValue` is internally managed in ESP-Matter 1.5+ and cannot be written with `updateAttributeVal()`.
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

`MatterContactSensor`, `MatterWaterLeakDetector`, `MatterWaterFreezeDetector`, and `MatterRainSensor` use the code-driven Boolean State cluster. `attribute::update()` returns `ESP_ERR_NOT_SUPPORTED` (262) for `StateValue`. Those setters call `MatterEndPoint::setBooleanStateValue()`, which looks up the live cluster and uses `BooleanStateCluster::SetStateValue()`.

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
- **Call the setter after `Matter.begin()`.** The cluster instance is not available before the stack starts. Sketches should `begin()` the endpoint, then `Matter.begin()`, then `setLeak()` / `setFreeze()` / `setRain()` / `setContact()` with the real sensor reading.
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

All device classes inherit `MatterEndPoint`. After `begin()` and before `Matter.begin()`, sketches can call `setTagList()` with `MatterTags` presets (Number, Location, Position, Switches) to set the Descriptor TagList. At most 3 tags per endpoint. See `MatterSmartButtonsTagList`. TagList does not set a light's display name.

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

Call these on the `Matter` singleton **before** `Matter.begin()`. After `begin()` they log a warning and do nothing. String setters **copy** into internal storage (stack or `String` temporaries are safe). `setDeviceName()` writes Basic Information NodeLabel (not a per-light name). Do not change Vendor ID / Product ID unless the DAC matches. SoftwareVersion is compile-time CHIP config.

```cpp
Matter.setVendorName("Espressif");            // max 32
Matter.setProductName("KitchenLight");        // max 32
Matter.setDeviceName("KitchenHub");           // NodeLabel, max 32
Matter.setSerialNumber("KH-000123");          // max 32
Matter.setHardwareVersion(7);
Matter.setHardwareVersionString("RevA");      // max 64
Matter.setSetupDiscriminator(0xF01);          // 0–0xFFF
Matter.setSetupPasscode(20202024);            // valid Matter PIN
Matter.setBLECommissioningEnabled(false);     // optional: on-network only (Wi-Fi first); releases BLE RAM
// Matter.setBLEMemoryReleaseEnabled(false);  // only if CHIPoBLE is left on: keep NimBLE after commission
Light.begin();
Matter.begin();
Serial.println(Matter.getManualPairingCode());     // live code after begin()
Serial.println(Matter.getOnboardingQRCodeUrl());   // live QR URL after begin()
```

If the sketch never calls `setSetupPasscode()` / `setSetupDiscriminator()`, Arduino Matter uses the CHIP test pair **PIN `20202021`**, discriminator **`0xF00`**, manual code **`34970112332`** (same as On/Off Light and the other examples). Before `begin()` the pairing getters log a warning and return empty.

On Wi-Fi station builds, `Matter.begin()` initializes the Wi-Fi driver with reduced RX/TX buffers before starting CHIP. Matter traffic is small, so the library uses 4 static RX, 8 dynamic RX, 8 dynamic TX, and an AMPDU RX BA window of 6 instead of the sdkconfig defaults. `esp_wifi_init()` keeps the first caller's counts, so CHIP inherits them. If the sketch already called `WiFi.begin()` / `WiFi.mode()`, those limits are not applied.

`setBLECommissioningEnabled(false)` (before `begin()`) makes a CHIPoBLE build commission on-network: connect Wi-Fi or Ethernet first, then `Matter.begin()`. BLE RAM is released automatically. Pairing codes are on-network only. See `MatterOnNetworkWiFi`. With CHIPoBLE left on, BLE RAM is released after a successful commission by default (`setBLEMemoryReleaseEnabled(true)`); see `MatterCHIPoBLERelease`. Call `setBLEMemoryReleaseEnabled(false)` before `begin()` to keep the BLE host after CHIPoBLE commissioning. That option has no effect when `CONFIG_ENABLE_CHIPOBLE` is off.

Register `Matter.onBLEMemoryReleased()` **before** `Matter.begin()` if the sketch must allocate a large buffer only after reclaim. The callback runs on the CHIP task when `kBLEDeinitialized` is posted (same moment as `MATTER_BLE_DEINITIALIZED`). Set a flag there and `malloc` from `loop()`. Reclaim is not instant: releasing the BLE regions while the NimBLE host task still runs would corrupt the heap, so the library polls for that task to exit every 2 s for up to 30 s. The callback may never run if CHIPoBLE is off, release is disabled, or the reclaim times out. Do not wait forever.

**Arduino IDE precompiled libraries:** CHIPoBLE is built with NimBLE on C3/C6/S3 (and Thread prebuilds such as C5/H2). Original ESP32 prebuilds use Bluedroid and do not compile CHIPoBLE. ESP32-S2 has no Bluetooth. **Arduino as an ESP-IDF component:** enable `CONFIG_BT_ENABLED`, `CONFIG_BT_NIMBLE_ENABLED`, and `CONFIG_ENABLE_CHIPOBLE` — including on original ESP32. Sketches already follow `CONFIG_ENABLE_CHIPOBLE` (not a chip name). To keep BLE after commission, also set `CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING=n`. If Bluetooth is off (`CONFIG_BT_ENABLED=n`), CHIPoBLE is compiled out and the BLE setters are no-ops / return false.

| API | Meaning |
|-----|---------|
| `isDeviceCommissioned()` | A Matter fabric exists |
| `isDeviceConnected()` | Wi-Fi or Thread is up |
| `isOnline()` | A controller has an active CASE session (until CHIP idle-evicts it) |
| `isBLECommissioningEnabled()` | CHIPoBLE is compiled in and still enabled |
| `isBLEMemoryReleaseEnabled()` | CHIPoBLE is on and BLE RAM will be released after commissioning |
| `onBLEMemoryReleased()` | BLE RAM is back on the heap (register before `begin()`) |

Do not gate LEDs on `isOnline()`. A session can stay up after the user leaves the app. See examples `MatterDeviceIdentity` and `MatterStatus`.

## Further Reading

- [Arduino-ESP32 Matter Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/index.html)
- [ESP-Matter Programming Guide](https://docs.espressif.com/projects/esp-matter/en/latest/)
- [Matter Specification (CSA)](https://csa-iot.org/developer-resource/specifications-download-request/)
- Examples: `MatterDeviceIdentity`, `MatterStatus`, `MatterOnNetworkWiFi`, `MatterCHIPoBLERelease`, `MatterSmartButtonsTagList`
