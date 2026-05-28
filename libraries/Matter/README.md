# Arduino Matter Library

Arduino-friendly wrapper around [ESP-Matter](https://docs.espressif.com/projects/esp-matter/en/latest/) (Espressif's SDK for Matter), providing high-level endpoint classes for common Matter device types.

## Architecture

Each Matter device type is represented by a C++ class under `src/MatterEndpoints/` (e.g., `MatterOnOffLight`, `MatterTemperatureSensor`, `MatterFan`). These classes manage:

- **Internal state variables** — C++ members that cache the device's current state (e.g., `onOffState`, `brightnessLevel`, `rawTemperature`).
- **Matter attribute store** — The ESP-Matter SDK's attribute database, which is the protocol-level representation read by controllers and used for subscriptions/reporting.

## Attribute Update Pattern

This library is built on ESP-Matter, which uses an Ember-based attribute store. The attribute store is the protocol-level source of truth: when a Matter controller reads an attribute, it reads from this store.

All endpoint setters and getters **must** follow one of the two patterns below.

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
- **Never assign internal state before the attribute store update.** If `updateAttributeVal()` fails and the function returns `false`, the internal state must remain unchanged.
- **Internal state assignment goes inside the `if (value changed)` block**, after the successful update call.
- When `updateAttributeVal()` may fail for nullable or dynamic attributes, a `setAttributeVal()` fallback can be used (see `MatterWindowCovering`, `MatterTemperatureControlledCabinet`). `setAttributeVal()` writes to the store without triggering the `PRE_UPDATE` callback chain or subscriber reporting.
- For composite setters that update multiple attributes, attempt all sub-updates and return the combined result. Partial commits are possible (some attributes updated, others not) since true atomic rollback is not feasible with the current ESP-Matter API. Each sub-update independently follows the Ember pattern (internal state only after its own attribute store success).

### Getter Pattern

Getters return the internal state variable directly, without reading from the attribute store:

```cpp
int MyEndpoint::getValue() {
  return internalValue;
}
```

This is consistent across all endpoints. Since setters only update internal state after the attribute store succeeds, the getter always reflects the last successfully committed value.

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

### updateAttributeVal vs setAttributeVal

| API | ESP-Matter function | Triggers PRE_UPDATE callback | Reports to subscribers | Use case |
|-----|-------------------|------------------------------|----------------------|----------|
| `updateAttributeVal()` | `attribute::update()` | Yes | Yes | Normal device-initiated state changes |
| `setAttributeVal()` | `attribute::set_val()` | No | No | Fallback for nullable attributes, syncing related attributes inside callbacks, avoiding re-entrancy |

## Endpoint Classes

| Class | Device Type |
|-------|-------------|
| `MatterOnOffLight` | On/Off Light |
| `MatterDimmableLight` | Dimmable Light |
| `MatterColorLight` | Color Light (HSV) |
| `MatterColorTemperatureLight` | Color Temperature Light |
| `MatterEnhancedColorLight` | Enhanced Color Light |
| `MatterOnOffPlugin` | On/Off Plug-in Unit |
| `MatterDimmablePlugin` | Dimmable Plug-in Unit |
| `MatterFan` | Fan |
| `MatterGenericSwitch` | Generic Switch |
| `MatterTemperatureSensor` | Temperature Sensor |
| `MatterHumiditySensor` | Humidity Sensor |
| `MatterPressureSensor` | Pressure Sensor |
| `MatterContactSensor` | Contact Sensor |
| `MatterOccupancySensor` | Occupancy Sensor |
| `MatterWaterLeakDetector` | Water Leak Detector |
| `MatterWaterFreezeDetector` | Water Freeze Detector |
| `MatterRainSensor` | Rain Sensor |
| `MatterThermostat` | Thermostat |
| `MatterWindowCovering` | Window Covering |
| `MatterTemperatureControlledCabinet` | Temperature Controlled Cabinet |

## Further Reading

- [ESP-Matter Programming Guide](https://docs.espressif.com/projects/esp-matter/en/latest/)
- [Arduino-ESP32 Matter Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/index.html)
- [Matter Specification (CSA)](https://csa-iot.org/developer-resource/specifications-download-request/)
