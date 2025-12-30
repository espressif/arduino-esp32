# OpenThread Router/Child Node Example (CLI)

This example demonstrates how to create an OpenThread Router or Child node that joins an existing Thread network using the CLI Helper Functions API.\
The Router/Child node joins a network created by a Leader node and can route messages or operate as an end device.

## Supported Targets

| SoC | Thread | Status |
| --- | ------ | ------ |
| ESP32-H2 | ✅ | Fully supported |
| ESP32-C6 | ✅ | Fully supported |
| ESP32-C5 | ✅ | Fully supported |

### Note on Thread Support:

- Thread support must be enabled in the ESP-IDF configuration (`CONFIG_OPENTHREAD_ENABLED`). This is done automatically when using the ESP32 Arduino OpenThread library.
- This example uses `OpenThread.begin(false)` which does not automatically start a Thread network, allowing manual configuration.
- **Important:** A Leader node must be running before starting this Router/Child node.

## Features

- Manual Router/Child node configuration using CLI Helper Functions API
- Joins an existing Thread network created by a Leader node
- Network information display using native OpenThread API calls
- Demonstrates both CLI Helper Functions and native OpenThread API usage
- IPv6 address listing (unicast and multicast)
- Automatic role assignment (Router or Child based on network conditions)

## Hardware Requirements

- ESP32 compatible development board with Thread support (ESP32-H2, ESP32-C6, or ESP32-C5)
- USB cable for Serial communication
- A Leader node must be running on the same network

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with OpenThread support
3. ESP32 Arduino libraries:
   - `OpenThread`

### Configuration

Before uploading the sketch, configure the network parameters to match the Leader node:

```cpp
#define CLI_NETWORK_KEY    "dataset networkkey 00112233445566778899aabbccddeeff"
#define CLI_NETWORK_CHANNEL "dataset channel 24"
```

**Important:**
- The network key **must match** the Leader node's network key
- The channel **must match** the Leader node's channel
- The network key must be a 32-character hexadecimal string (16 bytes)
- The channel must be between 11 and 26 (IEEE 802.15.4 channels)

## Building and Flashing

1. **First, start the Leader node** using the LeaderNode example
2. Open the `RouterNode.ino` sketch in the Arduino IDE.
3. Select your ESP32 board from the **Tools > Board** menu (ESP32-H2, ESP32-C6, or ESP32-C5).
4. Connect your ESP32 board to your computer via USB.
5. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
Setting up OpenThread Node as Router/Child
Make sure the Leader Node is already running
=============================================
Thread Node State: Router
Network Name: OpenThread-ESP
Channel: 24
PanID: 0x1234
Extended PAN ID: dead00beef00cafe
Network Key: 00112233445566778899aabbccddeeff
IP Address: fd00:db8:a0:0:0:ff:fe00:fc00
Multicast IP Address: ff02::1
Multicast IP Address: ff03::1
...
```

The device will join as either a **Router** (if network needs more routers) or **Child** (end device).

## Using the Device

### Router/Child Node Setup

The Router/Child node is automatically configured in `setup()` using the following sequence:

1. **Clear dataset**: Clears any existing dataset
2. **Set network key**: Configures the network security key (must match Leader)
3. **Set channel**: Configures the IEEE 802.15.4 channel (must match Leader)
4. **Commit dataset**: Applies the dataset to the active configuration
5. **Start interface**: Brings up the network interface
6. **Start Thread**: Starts the Thread network and joins the existing network

### Network Information

Once the device joins the network (as Router or Child), the `loop()` function displays:
- Device role (Router or Child)
- Network name (from the Leader)
- Channel
- PAN ID and Extended PAN ID
- Network key
- All IPv6 addresses (unicast and multicast)

### Multi-Device Network

To create a multi-device Thread network:

1. Start the Leader node first (using LeaderNode example)
2. Start Router/Child nodes (using this example)
3. All devices will form a mesh network
4. Routers extend network range and route messages
5. Children are end devices that can sleep

## Code Structure

The RouterNode example consists of the following main components:

1. **`setup()`**:
   - Initializes Serial communication
   - Starts OpenThread stack with `OpenThread.begin(false)` (no auto-start)
   - Initializes OpenThread CLI
   - Configures the device to join an existing network using CLI Helper Functions:
     - `OThreadCLI.println()` - Sends CLI commands
     - Commands: "dataset clear", "dataset networkkey", "dataset channel", "dataset commit active", "ifconfig up", "thread start"

2. **`loop()`**:
   - Checks if device role is Router or Child using `OpenThread.otGetDeviceRole()`
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

- **Device not joining network**: Ensure the Leader node is running first. Verify network key and channel match the Leader exactly.
- **Role stuck as "Detached"**: Wait a few seconds for the device to join. Check that network key and channel match the Leader.
- **Network key/channel mismatch**: Double-check that both Leader and Router/Child nodes use identical network key and channel values.
- **No network information**: Wait for the device to join the network (may take 10-30 seconds)
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [OpenThread CLI Helper Functions API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_cli.html)
- [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html)
- [OpenThread Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html)

## License

This example is licensed under the Apache License, Version 2.0.
