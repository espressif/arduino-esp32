# OpenThread Simple Node Example

This example demonstrates how to create a simple OpenThread node that automatically starts in a Thread network using default settings.\
The application automatically initializes a Thread network with default parameters and displays the current device role and network information.

## Supported Targets

| SoC | Thread | Status |
| --- | ------ | ------ |
| ESP32-H2 | ✅ | Fully supported |
| ESP32-C6 | ✅ | Fully supported |
| ESP32-C5 | ✅ | Fully supported |

### Note on Thread Support:

- Thread support must be enabled in the ESP-IDF configuration (`CONFIG_OPENTHREAD_ENABLED`). This is done automatically when using the ESP32 Arduino OpenThread library.
- This example uses `OpenThread.begin()` which automatically starts a Thread network using default settings or previously stored NVS dataset information.

## Features

- Automatic Thread network initialization with default settings
- Automatic device role assignment (first device becomes Leader, subsequent devices become Router or Child)
- Network information display using CLI Helper Functions API
- Periodic device role monitoring
- Support for persistent network configuration via NVS

## Hardware Requirements

- ESP32 compatible development board with Thread support (ESP32-H2, ESP32-C6, or ESP32-C5)
- USB cable for Serial communication

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with OpenThread support
3. ESP32 Arduino libraries:
   - `OpenThread`

### Configuration

No configuration is required before uploading the sketch. The example uses default Thread network parameters:

- **Network Name**: "OpenThread-ESP"
- **Mesh Local Prefix**: "fd00:db8:a0:0::/64"
- **Channel**: 15
- **PAN ID**: 0x1234
- **Extended PAN ID**: "dead00beef00cafe"
- **Network Key**: "00112233445566778899aabbccddeeff"
- **PSKc**: "104810e2315100afd6bc9215a6bfac53"

**Note:** If NVS (Non-Volatile Storage) already contains dataset information, it will be loaded from there instead of using defaults.

## Building and Flashing

1. Open the `SimpleNode.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu (ESP32-H2, ESP32-C6, or ESP32-C5).
3. Connect your ESP32 board to your computer via USB.
4. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
OpenThread Network Information:
Role: Leader
RLOC16: 0x0000
Network Name: OpenThread-ESP
Channel: 15
PAN ID: 0x1234
Extended PAN ID: dead00beef00cafe
Network Key: 00112233445566778899aabbccddeeff
Mesh Local EID: fd00:db8:a0:0:0:ff:fe00:0
Leader RLOC: fd00:db8:a0:0:0:ff:fe00:0
Node RLOC: fd00:db8:a0:0:0:ff:fe00:0

Thread Node State: Leader
Thread Node State: Leader
...
```

The first device to start will become the **Leader**. Subsequent devices will automatically join as **Router** or **Child** nodes.

## Using the Device

### Network Behavior

- **First device**: Automatically becomes the Leader and creates a new Thread network
- **Subsequent devices**: Automatically join the existing network as Router or Child nodes
- **Device role**: Displayed every 5 seconds in the Serial Monitor

### Multiple Devices

To create a multi-device Thread network:

1. Flash the first device - it will become the Leader
2. Flash additional devices - they will automatically join the network
3. All devices must use the same network key and channel (defaults are used if NVS is empty)

## Code Structure

The SimpleNode example consists of the following main components:

1. **`setup()`**:
   - Initializes Serial communication
   - Starts OpenThread stack with `OpenThread.begin()` (auto-start with default settings)
   - Initializes OpenThread CLI
   - Prints current Thread network information using `OpenThread.otPrintNetworkInformation()`

2. **`loop()`**:
   - Periodically displays the current device role using `OpenThread.otGetStringDeviceRole()`
   - Updates every 5 seconds

## Troubleshooting

- **Device not joining network**: Ensure all devices use the same network key and channel. Check that the Leader node is running first.
- **Role stuck as "Detached"**: Wait a few seconds for the device to join the network. Verify network key and channel match the Leader.
- **No network information displayed**: Check that OpenThread stack initialized successfully (look for network information in setup)
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html)
- [OpenThread CLI Helper Functions API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_cli.html)
- [OpenThread Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html)

## License

This example is licensed under the Apache License, Version 2.0.
