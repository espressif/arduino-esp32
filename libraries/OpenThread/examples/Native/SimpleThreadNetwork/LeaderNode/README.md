# OpenThread Leader Node Example (Native API)

This example demonstrates how to create an OpenThread Leader node using the Classes API (native OpenThread API).\
The Leader node is the first device in a Thread network that manages the network and assigns router IDs. This example shows how to configure a Leader node using the `OpenThread` and `DataSet` classes.

## Supported Targets

| SoC | Thread | Status |
| --- | ------ | ------ |
| ESP32-H2 | ✅ | Fully supported |
| ESP32-C6 | ✅ | Fully supported |
| ESP32-C5 | ✅ | Fully supported |

### Note on Thread Support:

- Thread support must be enabled in the ESP-IDF configuration (`CONFIG_OPENTHREAD_ENABLED`). This is done automatically when using the ESP32 Arduino OpenThread library.
- This example uses the Classes API (`OpenThread` and `DataSet` classes) instead of CLI Helper Functions.
- This example uses `OpenThread.begin(false)` which does not use NVS dataset information, allowing fresh configuration.

## Features

- Leader node configuration using Classes API
- Dataset creation and configuration using `DataSet` class
- Network information display using `OpenThread` class methods
- IPv6 address management (unicast and multicast)
- Address cache management on role changes
- Comprehensive network status monitoring

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
dataset.setNetworkName("ESP_OpenThread");
dataset.setChannel(15);
dataset.setPanId(0x1234);
uint8_t networkKey[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
dataset.setNetworkKey(networkKey);
```

**Important:**
- The network key must be a 16-byte array
- The channel must be between 11 and 26 (IEEE 802.15.4 channels)
- All devices in the same network must use the same network key and channel
- Extended PAN ID should be unique for your network

## Building and Flashing

1. Open the `LeaderNode.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu (ESP32-H2, ESP32-C6, or ESP32-C5).
3. Connect your ESP32 board to your computer via USB.
4. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
==============================================
OpenThread Network Information:
Role: Leader
RLOC16: 0x0000
Network Name: ESP_OpenThread
Channel: 15
PAN ID: 0x1234
Extended PAN ID: dead00beef00cafe
Network Key: 00112233445566778899aabbccddeeff
Mesh Local EID: fd00:db8:a0:0:0:ff:fe00:0
Leader RLOC: fd00:db8:a0:0:0:ff:fe00:0
Node RLOC: fd00:db8:a0:0:0:ff:fe00:0

--- Unicast Addresses (Using Count + Index API) ---
  [0]: fd00:db8:a0:0:0:ff:fe00:0
  [1]: fe80:0:0:0:0:ff:fe00:0

--- Multicast Addresses (Using std::vector API) ---
  [0]: ff02::1
  [1]: ff03::1
  [2]: ff03::fc
...
```

## Using the Device

### Leader Node Setup

The Leader node is automatically configured in `setup()` using the Classes API:

1. **Initialize OpenThread**: `threadLeaderNode.begin(false)` - Starts OpenThread stack without using NVS
2. **Create dataset**: `dataset.initNew()` - Creates a new complete dataset
3. **Configure dataset**: Sets network name, extended PAN ID, network key, channel, and PAN ID
4. **Apply dataset**: `threadLeaderNode.commitDataSet(dataset)` - Applies the dataset
5. **Start network**: `threadLeaderNode.networkInterfaceUp()` and `threadLeaderNode.start()` - Starts the Thread network

### Network Information

The `loop()` function displays comprehensive network information using Classes API methods:
- Device role and RLOC16
- Network name, channel, PAN ID
- Extended PAN ID and network key
- IPv6 addresses (Mesh Local EID, Leader RLOC, Node RLOC)
- Unicast addresses (using count + index API)
- Multicast addresses (using std::vector API)

### Address Cache Management

The example demonstrates address cache management:
- Clears address cache when device role changes
- Tracks role changes to optimize address resolution

### Joining Other Devices

To join other devices to this network:
1. Use the same network key and channel in the Router/Child node examples
2. Start the Leader node first
3. Then start the Router/Child nodes

## Code Structure

The LeaderNode example consists of the following main components:

1. **`setup()`**:
   - Initializes Serial communication
   - Starts OpenThread stack with `OpenThread.begin(false)`
   - Creates and configures a `DataSet` object:
     - `dataset.initNew()` - Initialize new dataset
     - `dataset.setNetworkName()` - Set network name
     - `dataset.setExtendedPanId()` - Set extended PAN ID
     - `dataset.setNetworkKey()` - Set network key
     - `dataset.setChannel()` - Set channel
     - `dataset.setPanId()` - Set PAN ID
   - Applies dataset: `threadLeaderNode.commitDataSet(dataset)`
   - Starts network: `threadLeaderNode.networkInterfaceUp()` and `threadLeaderNode.start()`

2. **`loop()`**:
   - Gets current device role using `threadLeaderNode.otGetDeviceRole()`
   - Displays network information using Classes API methods:
     - `threadLeaderNode.otGetStringDeviceRole()` - Device role as string
     - `threadLeaderNode.getRloc16()` - RLOC16
     - `threadLeaderNode.getNetworkName()` - Network name
     - `threadLeaderNode.getChannel()` - Channel
     - `threadLeaderNode.getPanId()` - PAN ID
     - `threadLeaderNode.getExtendedPanId()` - Extended PAN ID
     - `threadLeaderNode.getNetworkKey()` - Network key
     - `threadLeaderNode.getMeshLocalEid()` - Mesh Local EID
     - `threadLeaderNode.getLeaderRloc()` - Leader RLOC
     - `threadLeaderNode.getRloc()` - Node RLOC
     - `threadLeaderNode.getUnicastAddressCount()` and `threadLeaderNode.getUnicastAddress(i)` - Unicast addresses
     - `threadLeaderNode.getAllMulticastAddresses()` - Multicast addresses
   - Manages address cache on role changes
   - Updates every 5 seconds

## Troubleshooting

- **Device not becoming Leader**: Ensure this is the first device started, or clear NVS to start fresh
- **Network key/channel mismatch**: Verify all devices use the same network key and channel
- **No network information**: Wait for the device to become Leader (may take a few seconds)
- **Address cache issues**: The example automatically clears cache on role changes
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html)
- [OpenThread Dataset API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_dataset.html)
- [OpenThread Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html)

## License

This example is licensed under the Apache License, Version 2.0.
