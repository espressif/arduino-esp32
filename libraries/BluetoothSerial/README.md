## Bluetooth Serial Library

A simple Serial-compatible library using ESP32 Classic Bluetooth Serial Port Profile (SPP).

> **Note:** Bluetooth Classic (BR/EDR) is only available on the **original ESP32** SoC. It is not available on ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2, or ESP32-P4.

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

* **inputCapability=true and outputCapability=true**
    * Both devices display a randomly generated code. If they match, the user confirms pairing on both devices.
    * Register a callback via `onConfirmRequest()` that receives the passkey and **returns `true`** to accept or **`false`** to reject.
* **inputCapability=false and outputCapability=false**
    * Only the other device authenticates pairing without any PIN.
* **inputCapability=false and outputCapability=true**
    * Only the other device authenticates pairing without any PIN.
* **inputCapability=true and outputCapability=false**
    * The remote device displays a passkey; the user must confirm it matches on the ESP32 side.
    * Register a callback via `onConfirmRequest()` that returns `true` to accept or `false` to reject.

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
