# OpenThread Extended Router Node Example (CLI)

This example demonstrates how to create an OpenThread Router or Child node that joins an existing Thread network, with extended functionality showing both CLI Helper Functions API and native OpenThread API usage.\
The example shows how to retrieve network information using both methods and demonstrates error handling.

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
- Demonstrates both CLI Helper Functions API (`otGetRespCmd()`) and native OpenThread API (`otLinkGetPanId()`) usage
- Network information display using both API methods
- Error handling and timeout management
- Comprehensive network status monitoring

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
#define CLI_NETWORK_KEY    "00112233445566778899aabbccddeeff"
#define CLI_NETWORK_CHANNEL "24"
```

**Important:**
- The network key **must match** the Leader node's network key
- The channel **must match** the Leader node's channel
- The network key must be a 32-character hexadecimal string (16 bytes)
- The channel must be between 11 and 26 (IEEE 802.15.4 channels)

## Building and Flashing

1. **First, start the Leader node** using the LeaderNode example
2. Open the `ExtendedRouterNode.ino` sketch in the Arduino IDE.
3. Select your ESP32 board from the **Tools > Board** menu (ESP32-H2, ESP32-C6, or ESP32-C5).
4. Connect your ESP32 board to your computer via USB.
5. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
Setting up OpenThread Node as Router/Child
Make sure the Leader Node is already running

PanID[using CLI]: 0x1234

PanID[using OT API]: 0x1234

Thread NetworkInformation:
---------------------------
Role: Router
RLOC16: 0xfc00
Network Name: OpenThread-ESP
Channel: 24
PAN ID: 0x1234
Extended PAN ID: dead00beef00cafe
Network Key: 00112233445566778899aabbccddeeff
Mesh Local EID: fd00:db8:a0:0:0:ff:fe00:fc00
Leader RLOC: fd00:db8:a0:0:0:ff:fe00:0
Node RLOC: fd00:db8:a0:0:0:ff:fe00:fc00
---------------------------
```

## Using the Device

### Extended Router/Child Node Setup

The Extended Router/Child node is automatically configured in `setup()` using the following sequence:

1. **Clear dataset**: Clears any existing dataset
2. **Set network key**: Configures the network security key (must match Leader)
3. **Set channel**: Configures the IEEE 802.15.4 channel (must match Leader)
4. **Commit dataset**: Applies the dataset to the active configuration
5. **Start interface**: Brings up the network interface
6. **Start Thread**: Starts the Thread network and joins the existing network
7. **Wait for role**: Waits up to 90 seconds for the device to become Router or Child

### Dual API Demonstration

This example demonstrates two ways to access OpenThread information:

1. **CLI Helper Functions API**: Uses `otGetRespCmd("panid", resp)` to get PAN ID via CLI
2. **Native OpenThread API**: Uses `otLinkGetPanId(esp_openthread_get_instance())` to get PAN ID directly

Both methods should return the same value, demonstrating API equivalence.

### Network Information

Once the device joins the network, the `loop()` function displays comprehensive network information using `OpenThread.otPrintNetworkInformation()`, including:
- Device role
- RLOC16
- Network name
- Channel
- PAN ID and Extended PAN ID
- Network key
- IPv6 addresses (Mesh Local EID, Leader RLOC, Node RLOC)

## Code Structure

The ExtendedRouterNode example consists of the following main components:

1. **`setup()`**:
   - Initializes Serial communication
   - Starts OpenThread stack with `OpenThread.begin(false)` (no auto-start)
   - Initializes OpenThread CLI
   - Configures the device to join an existing network using CLI Helper Functions:
     - `otExecCommand()` - Executes CLI commands with error handling
     - Commands: "dataset clear", "dataset networkkey", "dataset channel", "dataset commit active", "ifconfig up", "thread start"
   - Waits for device to become Router or Child (with 90-second timeout)
   - Demonstrates dual API usage for getting PAN ID

2. **`loop()`**:
   - Displays comprehensive network information using `OpenThread.otPrintNetworkInformation()`
   - Updates every 10 seconds
   - Shows error message if setup failed

## Troubleshooting

- **Device not joining network**: Ensure the Leader node is running first. Verify network key and channel match the Leader exactly.
- **Timeout error**: The device waits 90 seconds to join. If timeout occurs, check network key and channel match the Leader.
- **Network key/channel mismatch**: Double-check that both Leader and Router/Child nodes use identical network key and channel values.
- **Setup failed message**: Check Serial Monitor for specific error messages. Verify Leader node is running and within range.
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [OpenThread CLI Helper Functions API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_cli.html)
- [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html)
- [OpenThread Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html)

## License

This example is licensed under the Apache License, Version 2.0.
