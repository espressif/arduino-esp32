# Zigbee Validation Test

Validates **ZigbeeCore** and all 26 Zigbee endpoint classes from `Zigbee.h`: stack configuration, endpoint registration, deep on-device API tests (getters, callbacks, reporting), network form/join, and coordinator→end-device ZCL interop. This is a **multi-DUT** test on ESP32-C6 / ESP32-H2.

## Architecture

```
 ┌──────────────────────────────┐         802.15.4          ┌───────────────────────┐
 │ Coordinator (device0)        │◄─────── wireless ────────►│ End device (device1)  │
 │ • All 26 endpoint types      │                           │ • Light + temp sensor │
 │ • Deep API + switch interop  │  epSwitch ──on/off──►     │ • Remote light control│
 └──────────────┬───────────────┘                           └──────────┬────────────┘
                │ serial                                               │ serial
                ▼                                                      ▼
 ┌─────────────────────────────────────────────────────────────────────────────────┐
 │                        pytest (test_zigbee.py)                                  │
 └─────────────────────────────────────────────────────────────────────────────────┘
```

Endpoint registration and pre-`begin()` configuration live in `coordinator/zigbee_endpoint_matrix.h`. Deep endpoint tests are in `coordinator/zigbee_endpoint_deep_tests.h`.

## Test Cases

### Coordinator (`coordinator/coordinator.ino`)

| Test Function | Description |
|---|---|
| `test_coordinator_config` | `ZigbeeCore` setters/getters before `begin()` (scan duration, debug, binding, radio/host config) |
| `test_coordinator_register_all_endpoints` | Register all 26 endpoint types + pre-`begin()` cluster configuration |
| `test_coordinator_begin` | `begin()`, verify duplicate `begin()` returns `false` |
| `test_coordinator_stop_start` | `stop()` / `start()` pause and resume |
| `test_coordinator_role` | `getRole()` returns coordinator |
| `test_coordinator_form_network` | `openNetwork()`, `connected()`, prints `NETWORK_READY` |
| `test_coordinator_endpoint_deep_tests` | 14 deep test groups: lights, outlet, fan, covering, IAS zones, temp/humidity, environmental sensors, analog/binary/multistate I/O, electrical measurement |

After Unity, `loop()` runs coordinator→end-device switch/light interop when pytest sends `RUN_INTEROP`.

### End device (`end_device/end_device.ino`)

| Test Function | Description |
|---|---|
| `test_end_device_config` | `ZigbeeCore` setters/getters before `begin()` |
| `test_end_device_add_endpoints` | `ZigbeeLight` and `ZigbeeTempSensor` registration |
| `test_end_device_begin` | `begin()`, verify duplicate `begin()` returns `false` |
| `test_end_device_stop_start` | `stop()` / `start()` |
| `test_end_device_role` | `getRole()` returns end device |
| `test_end_device_join_network` | Join coordinator network, `connected()`, prints `JOINED` |
| `test_end_device_temp_sensor` | `setReporting`, `setTemperature`, `reportTemperature` after join |
| `test_end_device_zcl_interop` | Coordinator switch controls local light via binding (`RUN_INTEROP`) |
| `test_end_device_light` | Local light callback, on/off, `restoreLight` |

## Requirements

- **Hardware**: Two ESP32-C6 or ESP32-H2 boards (802.15.4 radio required)
- **Wokwi/QEMU**: Not supported (802.15.4 radio not simulated)
- **CI Runner**: `two_duts`
- **SoC Config**: `CONFIG_ZB_ENABLED=y`, `CONFIG_SOC_IEEE802154_SUPPORTED=y`
- **FQBN**: Coordinator `PartitionScheme=zigbee_zczr,ZigbeeMode=zczr`; end device `PartitionScheme=zigbee,ZigbeeMode=ed`

## Serial Protocol

The coordinator and end device flash at different speeds; pytest drives the end device after the coordinator finishes its suite.

1. Coordinator prints `[COORDINATOR] Ready`, runs Unity tests, then `[COORDINATOR] NETWORK_READY` → `API_TESTS_DONE` → `INTEROP_READY` → `DONE`
2. End device prints `[ENDDEVICE] Ready`
3. pytest sends `START` to the end device
4. End device runs Unity tests, joins network, prints `[ENDDEVICE] JOINED`
5. pytest sends `RUN_INTEROP` to both devices
6. Coordinator prints `[COORDINATOR] INTEROP_ARMED`; switch drives end-device light → `[ENDDEVICE] REMOTE_LIGHT_ON` / `REMOTE_LIGHT_OFF` → `SWITCH_INTEROP_DONE` on both
7. End device completes remaining Unity tests, prints `[ENDDEVICE] DONE`

## Notes

- `Zigbee.begin()` is called once per boot; a second call returns `false`.
- Unlike Matter validation, Zigbee cannot decommission/reboot mid-suite on a single DUT — depth comes from getter round-trips, callbacks, reporting, pre-begin cluster config, and real ZCL over the air with a second board.
- Thermostat, gateway, and range-extender endpoints are registered on the coordinator but have no simple local attribute setters in the Arduino API; control paths are covered via registration and ZCL examples.
- ED→coordinator switch control is not covered (would need bind/discover APIs); coordinator→ED interop uses automatic binding on join.
- `scanNetworks`, `factoryReset`, and RCP host mode are not covered here (see library examples).
