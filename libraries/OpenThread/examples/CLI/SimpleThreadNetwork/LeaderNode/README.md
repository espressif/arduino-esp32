# OpenThread Leader Node Example (CLI)

This example demonstrates how to create an OpenThread Leader node using the CLI Helper Functions API.\
The Leader node is the first device in a Thread network that manages the network and assigns router IDs. This example shows how to configure a Leader node manually using OpenThread CLI commands.

## Supported Targets

| SoC | Thread | Status |
| --- | ------ | ------ |
| ESP32-H2 | ✅ | Fully supported |
| ESP32-C6 | ✅ | Fully supported |
| ESP32-C5 | ✅ | Fully supported |

### Note on Thread Support:

- Thread support must be enabled in the ESP-IDF configuration (`CONFIG_OPENTHREAD_ENABLED`). This is done automatically when using the ESP32 Arduino OpenThread library.
- This example uses `OpenThread.begin(false)` which does not automatically start a Thread network, allowing manual configuration.

## Features

- Manual Leader node configuration using CLI Helper Functions API
- Complete dataset initialization with network key and channel
- Network information display using native OpenThread API calls
- Demonstrates both CLI Helper Functions and native OpenThread API usage
- IPv6 address listing (unicast and multicast)

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

Before uploading the sketch, you can modify the network configuration:

```cpp
#define CLI_NETWORK_KEY    "dataset networkkey 00112233445566778899aabbccddeeff"
#define CLI_NETWORK_CHANNEL "dataset channel 24"
```

**Important:**
- The network key must be a 32-character hexadecimal string (16 bytes)
- The channel must be between 11 and 26 (IEEE 802.15.4 channels)
- All devices in the same network must use the same network key and channel

## Building and Flashing

1. Open the `LeaderNode.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu (ESP32-H2, ESP32-C6, or ESP32-C5).
3. Connect your ESP32 board to your computer via USB.
4. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
Setting up OpenThread Node as Leader
=============================================
Thread Node State: Leader
Network Name: OpenThread-ESP
Channel: 24
PanID: 0x1234
Extended PAN ID: dead00beef00cafe
Network Key: 00112233445566778899aabbccddeeff
IP Address: fd00:db8:a0:0:0:ff:fe00:0
Multicast IP Address: ff02::1
Multicast IP Address: ff03::1
...
```

## Using the Device

### Leader Node Setup

The Leader node is automatically configured in `setup()` using the following sequence:

1. **Initialize new dataset**: Creates a complete dataset with random values
2. **Set network key**: Configures the network security key
3. **Set channel**: Configures the IEEE 802.15.4 channel
4. **Commit dataset**: Applies the dataset to the active configuration
5. **Start interface**: Brings up the network interface
6. **Start Thread**: Starts the Thread network

### Network Information

Once the device becomes a Leader, the `loop()` function displays:
- Device role (Leader)
- Network name
- Channel
- PAN ID and Extended PAN ID
- Network key
- All IPv6 addresses (unicast and multicast)

### Joining Other Devices

To join other devices to this network:
1. Use the same network key and channel in the Router/Child node examples
2. Start the Leader node first
3. Then start the Router/Child nodes

## Code Structure

The LeaderNode example consists of the following main components:

1. **`setup()`**:
   - Initializes Serial communication
   - Starts OpenThread stack with `OpenThread.begin(false)` (no auto-start)
   - Initializes OpenThread CLI
   - Configures the device as a Leader using CLI Helper Functions:
     - `OThreadCLI.println()` - Sends CLI commands
     - Commands: "dataset init new", "dataset networkkey", "dataset channel", "dataset commit active", "ifconfig up", "thread start"

2. **`loop()`**:
   - Checks if device role is Leader using `OpenThread.otGetDeviceRole()`
   - Displays network information using native OpenThread API calls:
     - `otThreadGetNetworkName()` - Network name
     - `otLinkGetChannel()` - Channel
     - `otLinkGetPanId()` - PAN ID
     - `otThreadGetExtendedPanId()` - Extended PAN ID
     - `otThreadGetNetworkKey()` - Network key
     - `otIp6GetUnicastAddresses()` - Unicast IPv6 addresses
     - `otIp6GetMulticastAddresses()` - Multicast IPv6 addresses
   - Updates every 5 seconds

## Troubleshooting

- **Device not becoming Leader**: Ensure this is the first device started, or clear NVS to start fresh
- **Network key/channel mismatch**: Verify all devices use the same network key and channel
- **No network information**: Wait for the device to become Leader (may take a few seconds)
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [OpenThread CLI Helper Functions API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_cli.html)
- [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html)
- [OpenThread Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html)

## License

This example is licensed under the Apache License, Version 2.0.
