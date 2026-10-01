# Matter Validation Test

Validates the Matter stack initialization, all 20 endpoint types in the library, commissioning status, pairing code availability, and state control. Tests actual commissioning and cluster command handling via an external Matter controller (`matterjs-server`) when the infrastructure is available.

## Test Phases

### Phase 0 -- Stack & Endpoint Tests (always runs)

| Test Function | Description |
|---|---|
| `test_matter_begin` | Initialize Matter stack with OnOffLight endpoint |
| `test_matter_pairing_code` | Verify manual pairing code is non-empty |
| `test_matter_qr_url` | Verify QR onboarding URL starts with `https://` |
| `test_matter_not_commissioned` | Verify device is not commissioned after fresh start |
| `test_matter_onoff_state` | OnOffLight set/get on/off state |
| `test_matter_dimmable_begin` | DimmableLight begin with initial brightness |
| `test_matter_dimmable_brightness` | Set/get brightness at 0, 128, 255 |
| `test_matter_dimmable_onoff` | DimmableLight on/off and toggle |
| `test_matter_color_begin` | ColorLight begin with custom HSV |
| `test_matter_color_hsv` | Set/get HSV color values |
| `test_matter_color_rgb` | Set RGB color, verify conversion |
| `test_matter_color_toggle` | ColorLight toggle on/off |
| `test_matter_multiple_endpoints` | Create multiple endpoints of different types |
| `test_matter_color_temp` | ColorTemperatureLight: brightness, color temp bounds, toggle |
| `test_matter_enhanced_color` | EnhancedColorLight: brightness, color temp, HSV, RGB, toggle |
| `test_matter_fan` | Fan: speed %, mode enum, mode string, on/off, toggle |
| `test_matter_onoff_plugin` | OnOffPlugin: on/off, toggle |
| `test_matter_dimmable_plugin` | DimmablePlugin: level control at boundaries, on/off, toggle |
| `test_matter_window_covering` | WindowCovering: lift/tilt %, covering type |
| `test_matter_thermostat` | Thermostat: mode, setpoints, local temp, mode string |
| `test_matter_cabinet` | TemperatureControlledCabinet: setpoint, min/max, step |
| `test_matter_temperature_sensor` | TemperatureSensor: set/get, operator overloads |
| `test_matter_humidity_sensor` | HumiditySensor: set/get, operator overloads |
| `test_matter_pressure_sensor` | PressureSensor: set/get, operator overloads |
| `test_matter_contact_sensor` | ContactSensor: set/get bool contact state |
| `test_matter_occupancy_sensor` | OccupancySensor: occupancy state, sensor type |
| `test_matter_water_leak` | WaterLeakDetector: set/get leak state |
| `test_matter_water_freeze` | WaterFreezeDetector: set/get freeze state |
| `test_matter_rain_sensor` | RainSensor: set/get rain state |
| `test_matter_generic_switch` | GenericSwitch: begin + click |
| `test_matter_onoff_operators` | OnOffLight operator bool/= and toggle |
| `test_matter_identify` | Identify callback registration |
| `test_matter_capabilities` | WiFi station enabled, device not connected |

### Phase 1 -- Commissioning & Multi-Endpoint Control Tests

Requires WiFi credentials (`--wifi-ssid`) on the pytest host. When WiFi credentials are present, `matterjs-server` and IPv6 must also be available — their absence is treated as a runner misconfiguration and causes a `FAIL`. Tests only `IGNORE` when no WiFi credentials are provided (non-`wifi_router` runner).

Five endpoints are created for Phase 1: OnOffLight (ep 1), DimmableLight (ep 2), ColorLight (ep 3), Fan (ep 4), TemperatureSensor (ep 5). The controller commissions the node once and then exercises each endpoint independently.

| Test Function | Description |
|---|---|
| `test_matter_decommissioned` | Verify `Matter.isDeviceCommissioned()` is false after decommission + reboot |
| `test_matter_commissioned` | Verify `Matter.isDeviceCommissioned()` returns true after external commissioning |
| `test_matter_connectivity` | Verify `Matter.isDeviceConnected()` returns true (WiFi connected + commissioned) |
| `test_matter_onoff_controlled` | Verify `onChange` callback fired when the controller sent `OnOff::Toggle` |
| `test_matter_dimmable_controlled` | Verify `onChangeBrightness` callback fired with level 128 from `LevelControl::MoveToLevel` |
| `test_matter_color_controlled` | Verify `onChangeColorHSV` callback fired with hue=120, sat=200 from `ColorControl::MoveToHueAndSaturation` |
| `test_matter_fan_controlled` | Verify `onChangeSpeedPercent` callback fired with 50% from `FanControl::PercentSetting` attribute write |

The host (pytest) also reads the `TemperatureMeasurement::MeasuredValue` attribute from the sensor endpoint and asserts it matches the value set by the device (22.5 °C).

## Requirements

- **Hardware**: Any ESP32 variant with sufficient flash
- **Wokwi/QEMU**: Not supported (Matter stack size)
- **SoC Config**: `CONFIG_ESP_MATTER_ENABLE_DATA_MODEL=y`
- **FQBN**: `PartitionScheme=huge_app`
- **CI Runner**: `wifi_router` (single device with WiFi network access)

### Phase 1 Additional Requirements

- `matterjs-server` installed on the CI runner:
  - Node.js 22.13+, 24.x, or 26.x (24.x recommended): `npm install -g matter-server`
  - Python client: `pip install matter-python-client==1.1.0`
  - Python >= 3.12
- The runner and ESP32 must be on the same L2 network (same VLAN) for mDNS device discovery
- IPv6 must be enabled on the runner

## Commissioning Flow

1. Device runs Phase 0 stack tests (no WiFi needed), then calls `Matter.decommission()` which triggers a `SW_CPU_RESET` reboot
2. After reboot, device re-initializes Matter, verifies decommission cleared state, prints `MATTER_READY`, waits for WiFi credentials via serial. An `RTC_NOINIT_ATTR` marker distinguishes the post-decommission boot from a fresh boot.
3. Device connects to WiFi, initializes 5 endpoints (OnOffLight, DimmableLight, ColorLight, Fan, TemperatureSensor) with callbacks, prints `COMMISSION_WAITING`
4. Pytest starts `matter-server` as a background process, connects via WebSocket, calls `commission_with_code` with `network_only=true` (no BLE required)
5. Controller discovers device via mDNS and commissions the node (all 5 endpoints)
6. Controller sends cluster commands to each endpoint: `OnOff::Toggle` (ep 1), `LevelControl::MoveToLevel` (ep 2), `ColorControl::MoveToHueAndSaturation` (ep 3), `FanControl::PercentSetting` attribute write (ep 4), reads `TemperatureMeasurement::MeasuredValue` (ep 5)
7. Device receives commands via Matter stack, endpoint callbacks set corresponding flags
8. Pytest signals results to the device via serial (`COMMISSION_DONE`/`COMMISSION_SKIP`)
9. Device waits for callbacks to propagate, runs Phase 1 Unity tests checking commissioning status, connectivity, and all callback flags

## Test Behavior

| Condition | Phase 1 Result |
|---|---|
| No WiFi credentials (`--wifi-ssid` not provided) | `IGNORED` |
| `matterjs-server` not installed (runner misconfiguration) | `FAIL` |
| Host has no IPv6 support (runner misconfiguration) | `FAIL` |
| WiFi connection fails | `FAIL` |
| Commissioning fails (mDNS, network, controller error) | `FAIL` |
| Any cluster command/attribute write fails | `FAIL` |
| Everything succeeds | `PASS` |

Phase 0 stack tests always run and always produce results regardless of Phase 1 outcome.

## Notes

- The default Matter pairing code (`34970112332`) is hardcoded in the Arduino Matter library and in `test_matter.py`.
- `matter-server` (Node.js, from `npm install -g matter-server`) is started on port 5580 with a temporary storage directory that is cleaned up after the test.
- Builds without `CONFIG_ESP_MATTER_ENABLE_DATA_MODEL` run a single graceful skip test.
