## Bluetooth Serial Library

A simple Serial-compatible library using ESP32 Classic Bluetooth Serial Port Profile (SPP).

> **Note:** Bluetooth Classic (BR/EDR) is available on the **original ESP32** and on
> **ESP32-S31**. On ESP32-S31 the prebuilt libraries enable NimBLE only, so
> `BluetoothSerial` requires using Arduino as an ESP-IDF component or rebuilding the
> static libraries. It is not available on ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6,
> ESP32-H2, or ESP32-P4.

### How to use it?

There are 3 basic use cases: phone, other ESP32, or any MCU with a Bluetooth serial module.

#### Phone

- Download one of the Bluetooth terminal apps to your smartphone

    - For [Android](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal)
    - For [iOS](https://itunes.apple.com/us/app/hm10-bluetooth-serial-lite/id1030454675)

- Flash the [SerialBridge](examples/SerialBridge/SerialBridge.ino) example to your ESP32

- Scan and pair the device from your smartphone's Bluetooth settings

- Open the Bluetooth terminal app and connect

- Enjoy

#### ESP32 to ESP32

Flash one ESP32 with [SerialBridge](examples/SerialBridge/SerialBridge.ino) (the acceptor/slave) and
another ESP32 with [MasterConnect](examples/MasterConnect/MasterConnect.ino) (the initiator/master).
Those examples are preset to work out-of-the-box.

#### 3rd party Serial Bluetooth module

Using a 3rd party Serial Bluetooth module requires studying the module's documentation.
One side can use [SerialBridge](examples/SerialBridge/SerialBridge.ino) (acceptor) or
[MasterConnect](examples/MasterConnect/MasterConnect.ino) (initiator) as a starting point.

### Pairing options

#### Without SSP

SSP is disabled by default. The device will accept any pairing attempt automatically.
This method should not be used if security is a concern.
See the [SerialBridge](examples/SerialBridge/SerialBridge.ino) example.

To require a fixed PIN instead, call `SerialBT.setPin("1234")` before `begin()` and do
**not** call `enableSSP()` — the two settings are mutually exclusive (`enableSSP()`
wins and the PIN is never programmed). A fixed PIN is legacy pairing: phones and PCs
that support SSP negotiate SSP and never ask for it. To force legacy pairing they must
be prevented from using SSP, which needs the build-time option described in
[Legacy Pairing (IDF component)](#legacy-pairing-idf-component). If both are configured,
the stored PIN still answers a legacy-only peer that falls back to legacy pairing, but
an SSP peer never requests it.

### With SSP

Secure Simple Pairing (SSP) provides a secure connection. Enable it before calling `begin()`:

```cpp
SerialBT.enableSSP(bool inputCapability, bool outputCapability);
```

Call `disableSSP()` to turn it off. If called after `begin()`, restart the driver
(`end()` followed by `begin()`) for the change to take effect.
See the [SSPPairing](examples/SSPPairing/SSPPairing.ino) example.

#### The parameters define the method of authentication:

**inputCapability** - Whether the ESP32 has an input method (Serial terminal, keyboard, or similar)

**outputCapability** - Whether the ESP32 has an output method (Serial terminal, display, or similar)

* **inputCapability=true and outputCapability=true** (`ESP_BT_IO_CAP_IO`, DisplayYesNo)
    * Against another display-capable device this is Numeric Comparison: both sides show a random
      six-digit code that must match.
    * Register a callback via `onConfirmRequest()` that receives the code and **returns `true`** to
      accept or **`false`** to reject.
* **inputCapability=false and outputCapability=false** (`ESP_BT_IO_CAP_NONE`, NoInputNoOutput)
    * Just Works: the devices pair without a PIN.
* **inputCapability=false and outputCapability=true** (`ESP_BT_IO_CAP_OUT`, DisplayOnly)
    * Against a display-capable phone or PC this also resolves to Just Works (unauthenticated).
    * Against a keyboard-only peer it selects Passkey Entry: the stack generates a six-digit code and
      delivers it to `onPassKeyDisplay()`, and the user types it on the peer. Without a handler the
      code is only written to the info log, which is compiled out at the default core debug level —
      register the handler if the device must show it.
* **inputCapability=true and outputCapability=false** (`ESP_BT_IO_CAP_IN`, KeyboardOnly)
    * Requests Passkey Entry as soon as the peer can display a code.
    * The peer shows the code; `onPassKeyRequest()` must return it (0–999999). Returning a value
      above 999999, or leaving the handler unset, rejects the pairing instead of timing out.
    * Two KeyboardOnly boards also work: both sides are asked to enter the code, so entering the
      same value on both consoles is the SSP equivalent of a fixed PIN. See the
      [PasskeyEntry](examples/PasskeyEntry/PasskeyEntry.ino) example.

#### Passkey Entry callbacks

```cpp
// Keyboard side: the peer displays the code, the sketch returns it.
SerialBT.onPassKeyRequest([](const BTAddress &peer) -> uint32_t {
    Serial.printf("Type the 6-digit passkey shown on %s: ", peer.toString().c_str());
    // ... read six digits from Serial ...
    return passkey;    // 0-999999, or >999999 to reject
});

// Display side: show the generated code to the user.
SerialBT.onPassKeyDisplay([](const BTAddress &peer, uint32_t passkey) {
    Serial.printf("Passkey for %s: %06lu\n", peer.toString().c_str(), (unsigned long)passkey);
});
```

Both handlers run on the Bluetooth host task. Bound any interactive input: reply before the
authentication timeout (~30 s) instead of waiting indefinitely, so other Bluetooth work is not
stalled.

### Legacy Pairing (IDF component)

To use legacy PIN pairing, use [Arduino as an IDF component](https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/esp-idf_component.html) and disable `CONFIG_BT_SSP_ENABLED`.
Run `idf.py menuconfig`, navigate to `Component Config -> Bluetooth -> Bluedroid -> [ ] Secure Simple Pairing`, and disable it.
Also change the partition scheme: `Partition Table -> Partition Table -> (X) Single Factory app (large), no OTA`.
Save & quit menuconfig then run `idf.py monitor flash`.

> **Note:** When using PIN pairing with smartphones and computers, pass the PIN as a string:
> `SerialBT.setPin("1234");` — not as a number.

### Multi-client (acceptor)

In acceptor (slave) mode, multiple SPP clients may connect at once. As a single Arduino `Stream`,
`write()`/`print()` broadcast to every connected peer and `read()`/`available()` drain one merged
RX buffer. Additive APIs attribute traffic per peer:

| API | Role |
| --- | --- |
| `peerCount()` / `peers()` | How many / which clients are connected |
| `writeTo(address, buf, len)` | Send to one peer |
| `onPeerData(handler)` | RX callback with source `BTAddress` |
| `onConnect` / `onDisconnect` | Per-peer connect/disconnect (`BTAddress`) |
| `disconnect(address)` | Drop one peer |

See [MultiClientSerial](examples/MultiClientSerial/MultiClientSerial.ino). Initiator (master)
`connect()` remains 1:1.

### Related docs

- [MIGRATION.md](MIGRATION.md) — v3.x → v4.0 API mapping
- BLE Nordic-UART equivalent: `libraries/BLE` `BLEStream` / `examples/MultiClientUART`
