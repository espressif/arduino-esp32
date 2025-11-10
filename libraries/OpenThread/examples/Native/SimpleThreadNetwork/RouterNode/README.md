# OpenThread Router/Child Node Example (Native API)

This example demonstrates how to create an OpenThread Router or Child node that joins an existing Thread network using the Classes API (native OpenThread API).\
The Router/Child node joins a network created by a Leader node and can route messages or operate as an end device. This example shows how to configure a Router/Child node using the `OpenThread` and `DataSet` classes.

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
- **Important:** A Leader node must be running before starting this Router/Child node.

## Features

- Router/Child node configuration using Classes API
- Dataset configuration using `DataSet` class
- Joins an existing Thread network created by a Leader node
- Network information display using `OpenThread` class methods
- Active dataset retrieval and display
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
uint8_t networkKey[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
dataset.setNetworkKey(networkKey);
```

**Important:**
- The network key **must match** the Leader node's network key exactly
- The network key must be a 16-byte array
- Only the network key is required to join (other parameters are learned from the Leader)
- **Start the Leader node first** before starting this Router/Child node

## Building and Flashing

1. **First, start the Leader node** using the LeaderNode example (Native API)
2. Open the `RouterNode.ino` sketch in the Arduino IDE.
3. Select your ESP32 board from the **Tools > Board** menu (ESP32-H2, ESP32-C6, or ESP32-C5).
4. Connect your ESP32 board to your computer via USB.
5. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
==============================================
OpenThread Network Information (Active Dataset):
Role: Router
RLOC16: 0xfc00
Network Name: ESP_OpenThread
Channel: 15
PAN ID: 0x1234
Extended PAN ID: dead00beef00cafe
Network Key: 00112233445566778899aabbccddeeff
Mesh Local EID: fd00:db8:a0:0:0:ff:fe00:fc00
Node RLOC: fd00:db8:a0:0:0:ff:fe00:fc00
Leader RLOC: fd00:db8:a0:0:0:ff:fe00:0
```

The device will join as either a **Router** (if network needs more routers) or **Child** (end device).

## Using the Device

### Router/Child Node Setup

The Router/Child node is automatically configured in `setup()` using the Classes API:

1. **Initialize OpenThread**: `threadChildNode.begin(false)` - Starts OpenThread stack without using NVS
2. **Clear dataset**: `dataset.clear()` - Clears any existing dataset
3. **Configure dataset**: Sets only the network key (must match Leader)
4. **Apply dataset**: `threadChildNode.commitDataSet(dataset)` - Applies the dataset
5. **Start network**: `threadChildNode.networkInterfaceUp()` and `threadChildNode.start()` - Starts the Thread network and joins existing network

### Network Information

The `loop()` function displays network information using Classes API methods:
- Device role and RLOC16
- Active dataset information (retrieved using `threadChildNode.getCurrentDataSet()`):
  - Network name, channel, PAN ID
  - Extended PAN ID and network key
- Runtime information:
  - Mesh Local EID, Node RLOC, Leader RLOC

### Active Dataset Retrieval

This example demonstrates how to retrieve the active dataset:
- Uses `threadChildNode.getCurrentDataSet()` to get the current active dataset
- Displays dataset parameters that were learned from the Leader node
- Shows that only the network key needs to be configured to join

### Multi-Device Network

To create a multi-device Thread network:

1. Start the Leader node first (using Native API LeaderNode example)
2. Start Router/Child nodes (using this example)
3. All devices will form a mesh network
4. Routers extend network range and route messages
5. Children are end devices that can sleep

## Code Structure

The RouterNode example consists of the following main components:

1. **`setup()`**:
   - Initializes Serial communication
   - Starts OpenThread stack with `OpenThread.begin(false)`
   - Creates and configures a `DataSet` object:
     - `dataset.clear()` - Clear existing dataset
     - `dataset.setNetworkKey()` - Set network key (must match Leader)
   - Applies dataset: `threadChildNode.commitDataSet(dataset)`
   - Starts network: `threadChildNode.networkInterfaceUp()` and `threadChildNode.start()`

2. **`loop()`**:
   - Gets current device role using `threadChildNode.otGetDeviceRole()`
   - Retrieves active dataset using `threadChildNode.getCurrentDataSet()`
   - Displays network information using Classes API methods:
     - `threadChildNode.otGetStringDeviceRole()` - Device role as string
     - `threadChildNode.getRloc16()` - RLOC16
     - `activeDataset.getNetworkName()` - Network name from dataset
     - `activeDataset.getChannel()` - Channel from dataset
     - `activeDataset.getPanId()` - PAN ID from dataset
     - `activeDataset.getExtendedPanId()` - Extended PAN ID from dataset
     - `activeDataset.getNetworkKey()` - Network key from dataset
     - `threadChildNode.getMeshLocalEid()` - Mesh Local EID
     - `threadChildNode.getRloc()` - Node RLOC
     - `threadChildNode.getLeaderRloc()` - Leader RLOC
   - Updates every 5 seconds

## Troubleshooting

- **Device not joining network**: Ensure the Leader node is running first. Verify network key matches the Leader exactly.
- **Role stuck as "Detached"**: Wait a few seconds for the device to join. Check that network key matches the Leader.
- **Network key mismatch**: Double-check that both Leader and Router/Child nodes use identical network key values.
- **No network information**: Wait for the device to join the network (may take 10-30 seconds)
- **Active dataset empty**: Ensure device has successfully joined the network before checking dataset
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html)
- [OpenThread Dataset API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_dataset.html)
- [OpenThread Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html)

## License

This example is licensed under the Apache License, Version 2.0.
