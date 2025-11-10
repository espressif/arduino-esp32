# OpenThread CoAP Lamp Example

This example demonstrates how to create a CoAP (Constrained Application Protocol) server on a Thread network that controls an RGB LED lamp.\
The application acts as a CoAP resource server that receives PUT requests to turn the lamp on or off, demonstrating Thread-based IoT device communication.

## Supported Targets

| SoC | Thread | RGB LED | Status |
| --- | ------ | ------- | ------ |
| ESP32-H2 | ✅ | Required | Fully supported |
| ESP32-C6 | ✅ | Required | Fully supported |
| ESP32-C5 | ✅ | Required | Fully supported |

### Note on Thread Support:

- Thread support must be enabled in the ESP-IDF configuration (`CONFIG_OPENTHREAD_ENABLED`). This is done automatically when using the ESP32 Arduino OpenThread library.
- This example requires a companion CoAP Switch device (coap_switch example) to control the lamp.
- The lamp device acts as a Leader node and CoAP server.

## Features

- CoAP server implementation on Thread network
- RGB LED control with smooth fade in/out transitions
- Leader node configuration using CLI Helper Functions API
- CoAP resource creation and management
- Multicast IPv6 address for CoAP communication
- Automatic network setup with retry mechanism
- Visual status indication using RGB LED (Red = failed, Green = ready)

## Hardware Requirements

- ESP32 compatible development board with Thread support (ESP32-H2, ESP32-C6, or ESP32-C5)
- RGB LED (built-in RGB LED or external RGB LED)
- USB cable for Serial communication
- A CoAP Switch device (coap_switch example) to control the lamp

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with OpenThread support
3. ESP32 Arduino libraries:
   - `OpenThread`

### Configuration

Before uploading the sketch, you can modify the network and CoAP configuration:

```cpp
#define OT_CHANNEL            "24"
#define OT_NETWORK_KEY        "00112233445566778899aabbccddeeff"
#define OT_MCAST_ADDR         "ff05::abcd"
#define OT_COAP_RESOURCE_NAME "Lamp"
```

**Important:**
- The network key and channel must match the Switch device configuration
- The multicast address and resource name must match the Switch device
- The network key must be a 32-character hexadecimal string (16 bytes)
- The channel must be between 11 and 26 (IEEE 802.15.4 channels)

## Building and Flashing

1. Open the `coap_lamp.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu (ESP32-H2, ESP32-C6, or ESP32-C5).
3. Connect your ESP32 board to your computer via USB.
4. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
Starting OpenThread.
Running as Lamp (RGB LED) - use the other C6/H2 as a Switch
OpenThread started.
Waiting for activating correct Device Role.
........
Device is Leader.
OpenThread setup done. Node is ready.
```

The RGB LED will turn **green** when the device is ready to receive CoAP commands.

## Using the Device

### Lamp Device Setup

The lamp device automatically:
1. Configures itself as a Thread Leader node
2. Creates a CoAP server
3. Registers a CoAP resource named "Lamp"
4. Sets up a multicast IPv6 address for CoAP communication
5. Waits for CoAP PUT requests from the Switch device

### CoAP Resource

The lamp exposes a CoAP resource that accepts:
- **PUT with payload "0"**: Turns the lamp OFF (fades to black)
- **PUT with payload "1"**: Turns the lamp ON (fades to white)

### Visual Status Indication

The RGB LED provides visual feedback:
- **Red**: Setup failed or error occurred
- **Green**: Device is ready and waiting for CoAP commands
- **White/Black**: Lamp state (controlled by CoAP commands)

### Working with Switch Device

1. Start the Lamp device first (this example)
2. Start the Switch device (coap_switch example) with matching network key and channel
3. Press the button on the Switch device to toggle the lamp
4. The lamp will fade in/out smoothly when toggled

## Code Structure

The coap_lamp example consists of the following main components:

1. **`otDeviceSetup()` function**:
   - Configures the device as a Leader node using CLI Helper Functions
   - Sets up CoAP server and resource
   - Waits for device to become Leader
   - Returns success/failure status

2. **`setupNode()` function**:
   - Retries setup until successful
   - Calls `otDeviceSetup()` with Leader role configuration

3. **`otCOAPListen()` function**:
   - Listens for CoAP requests from the Switch device
   - Parses CoAP PUT requests
   - Controls RGB LED based on payload (0 = OFF, 1 = ON)
   - Implements smooth fade transitions

4. **`setup()`**:
   - Initializes Serial communication
   - Starts OpenThread stack with `OpenThread.begin(false)`
   - Initializes OpenThread CLI
   - Sets CLI timeout
   - Calls `setupNode()` to configure the device

5. **`loop()`**:
   - Continuously calls `otCOAPListen()` to process incoming CoAP requests
   - Small delay for responsiveness

## Troubleshooting

- **LED stays red**: Setup failed. Check Serial Monitor for error messages. Verify network configuration.
- **Lamp not responding to switch**: Ensure both devices use the same network key, channel, multicast address, and resource name. Check that Switch device is running.
- **Device not becoming Leader**: Clear NVS or ensure this is the first device started. Check network configuration.
- **CoAP requests not received**: Verify multicast address matches between Lamp and Switch devices. Check Thread network connectivity.
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [OpenThread CLI Helper Functions API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_cli.html)
- [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html)
- [OpenThread Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html)
- [CoAP Protocol](https://coap.technology/)

## License

This example is licensed under the Apache License, Version 2.0.
