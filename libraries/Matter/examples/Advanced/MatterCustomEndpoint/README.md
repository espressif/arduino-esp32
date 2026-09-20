# MatterCustomEndpoint Example

This **advanced** example shows how to **create and use user-defined custom endpoints** when esp-matter exposes a device type that is not wrapped by a stock `Matter*` class in the Arduino Matter library.

The sketch uses a **PM2.5 air-quality sensor** as the concrete illustration: an Air Quality Sensor endpoint with a PM2.5 concentration measurement cluster, a `MatterEndPoint` subclass (`MatterAirQualityPm25Sensor`), `ensureMatterNode()` in subclass `begin()`, the public `registerCreatedEndpoint()`, inherited `updateAttributeVal()`, and an `onStackStarted()` override.

## Custom endpoint steps (summary)

1. Subclass `MatterEndPoint` and implement `attributeChangeCB`.
2. Call `ensureMatterNode()` in `begin()`.
3. Create the endpoint with `esp_matter::endpoint::*::create(node::get(), …, ENDPOINT_FLAG_NONE, (void *)this)`.
4. Add extra clusters if needed (here: PM2.5 concentration measurement).
5. Call `registerCreatedEndpoint(endpoint)`.
6. Set the initial measured value in the esp-matter `create()` config when possible. Override `onStackStarted()` to re-apply cached C++ state after `Matter.begin()`.
7. Do not rely on `setPm25()` (or similar setters using `updateAttributeVal()`) between endpoint `begin()` and `Matter.begin()`; the stack is not running yet. After `Matter.begin()`, use the setter from `loop()` or other runtime code.

See also [Custom endpoints](../../../README.md#custom-endpoints-esp-matter-device-types-not-wrapped-in-tree) in the library README.

## Sketch flow (same as other Matter examples)

1. **`setup()`**: Boot button for decommission, Serial, `matterConnectWiFi()` when CHIPoBLE is off, custom endpoint `begin()`, `Matter.begin()`, then `matterWaitUntilReady()` (pairing codes and commissioning / CASE).
2. **`loop()`**: `matterRestartIfNoFabric()`, simulated PM2.5 updates every 5 s via `setPm25()`, long-press BOOT (>5 s) to decommission.

## Usage

1. Open `MatterCustomEndpoint.ino` in the Arduino IDE.
2. Set `WIFI_SSID` / `WIFI_PASSWORD` when BLE commissioning is disabled (`CONFIG_ENABLE_CHIPOBLE=n`).
3. Build and flash with a Matter-capable board profile (`PartitionScheme=huge_app`).
4. Commission the device and observe PM2.5 readings on the Matter controller.

For Wi-Fi / Thread / Ethernet commissioning paths, see the [Commissioning](../../Commissioning/) examples and call `Matter.selectNetwork()` before any accessory `begin()`.
