# Matter Device Identity Example

This example shows how to set node identity and commissioning codes on the `Matter` singleton before `Matter.begin()`. It is an on/off light (same hardware as Matter On/Off Light) plus VendorName, ProductName, DeviceName (NodeLabel), SerialNumber, hardware version, and a non-default discriminator/PIN.

Use the **generated** pairing codes printed after `Matter.begin()`. Before `begin()` the getters log a warning and return empty. Do not use a remembered Arduino test code (`34970112332`) when the PIN or discriminator has been changed.

## Supported Targets

| SoC | Wi-Fi | Thread | BLE Commissioning | LED | Status |
| --- | ---- | ------ | ----------------- | --- | ------ |
| ESP32 | ✅ | ❌ | IDE: ❌ / IDF NimBLE+CHIPoBLE: ✅ | Required | Fully supported |
| ESP32-S2 | ✅ | ❌ | ❌ | Required | Fully supported |
| ESP32-S3 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-C3 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-C5 | ❌ | ✅ | ✅ | Required | Supported (Thread only) |
| ESP32-C6 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-H2 | ❌ | ✅ | ✅ | Required | Supported (Thread only) |

### Note on Commissioning

BLE commissioning is compiled in only when **CHIPoBLE** is enabled (`CONFIG_ENABLE_CHIPOBLE`). That is independent of the SoC name:

- **Arduino IDE (precompiled Matter):** original **ESP32** uses Bluedroid and does **not** include CHIPoBLE. **ESP32-S2** has no Bluetooth. Set Wi-Fi credentials in the sketch (`#if !CONFIG_ENABLE_CHIPOBLE`). C3/C6/S3 (and Thread prebuilds) use NimBLE + CHIPoBLE.
- **Arduino as an ESP-IDF component:** original ESP32 can use CHIPoBLE if you set `CONFIG_BT_ENABLED=y`, `CONFIG_BT_NIMBLE_ENABLED=y`, and `CONFIG_ENABLE_CHIPOBLE=y`. This sketch then skips the hardcoded Wi-Fi path and commissions over BLE.
- **ESP32-C6** Arduino Matter is precompiled Wi-Fi only. Thread-only needs Arduino as an IDF component.
- **ESP32-C5** Arduino Matter is precompiled Thread only.

The BLE Commissioning column in the table is the **Arduino IDE prebuild**.

## Call order

```cpp
Matter.setVendorName("Espressif");
Matter.setProductName("KitchenLight");
Matter.setDeviceName("KitchenHub");   // Basic Information NodeLabel
Matter.setSerialNumber("KH-000123");
Matter.setHardwareVersion(7);
Matter.setHardwareVersionString("RevA");
Matter.setSetupDiscriminator(0xF01);  // 0–0xFFF; test default is 0xF00
Matter.setSetupPasscode(20202024);    // valid PIN; test default is 20202021

OnOffLight.begin(lastOnOffState);
Matter.begin();  // applies identity, regenerates SPAKE2+ if the PIN changed, prints live codes
```

Setters after `Matter.begin()` log a warning and have no effect. String setters copy the text (literals, stack buffers, and `String` are all safe). Limits: names and serial 32, hardware version string 64.

`setDeviceName()` writes NodeLabel. On a single-endpoint node, controllers often use that as the device title. On a composed node it is the parent/node name; child lights are not renamed (use `MatterEndPoint::setTagList()` for switch-style tags, not light titles).

Do not change Vendor ID / Product ID from a sketch unless the DAC matches. SoftwareVersion is compile-time CHIP config.

Production devices should use a unique random PIN and discriminator per unit (factory NVS). This example stores values in RAM and reapplies them every boot.

## Commissioned, connected, online

After pairing, the sketch drives the LED from local Matter state (`updateAccessory()`). `loop()` keeps the button live and samples `Matter.isOnline()` every 2.5 seconds, logging only when the CASE session goes up or down.

1. `Matter.isDeviceCommissioned()` — fabric exists (pairing wait)
2. `Matter.isDeviceConnected()` — Wi-Fi or Thread is up
3. `Matter.isOnline()` — a controller has an active CASE session (stays true until the session is idle-evicted, not when the user closes the app)

Do not gate the LED on `isOnline()`. After a power cycle the light restores last on/off even if the hub is off. See [Matter Status](../MatterStatus) for a periodic print of all three flags.

## Hardware

- LED: `LED_BUILTIN` or pin 2
- Button: `BOOT_PIN` — short press toggles, hold 5 s decommissions

## Building and Flashing

1. Open `MatterDeviceIdentity.ino`.
2. Board: your ESP32 target.
3. Partition Scheme: **Huge APP (3 MB No OTA / 1 MB SPIFFS)**.
4. Enable **Erase All Flash Before Sketch Upload**.
5. Upload. Serial 115200.

Erase flash after changing identity or pairing codes. Controllers cache names at commissioning.

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Status](../MatterStatus) — commissioned / connected / online
- [Matter Smart Buttons TagList](../MatterSmartButtonsTagList) — Descriptor tags (not light names)

## License

Apache License 2.0.
