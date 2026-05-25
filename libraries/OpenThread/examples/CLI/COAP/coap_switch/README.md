# OpenThread CoAP Switch Example

This example demonstrates how to create a CoAP (Constrained Application Protocol) client on a Thread network that controls a remote CoAP server (lamp).\
The application acts as a CoAP client that sends PUT requests to a lamp device, demonstrating Thread-based IoT device control.

## Supported Targets

| SoC | Thread | Button | Status |
| --- | ------ | ------ | ------ |
| ESP32-H2 | ✅ | Required | Fully supported |
| ESP32-C6 | ✅ | Required | Fully supported |
| ESP32-C5 | ✅ | Required | Fully supported |

### Note on Thread Support:

- Thread support must be enabled in the ESP-IDF configuration (`CONFIG_OPENTHREAD_ENABLED`). This is done automatically when using the ESP32 Arduino OpenThread library.
- This example requires a companion CoAP Lamp device (coap_lamp example) to control.
- The switch device joins the network as a Router or Child node and acts as a CoAP client.

## Features

- CoAP client implementation on Thread network
- Button-based control of remote lamp device
- Router/Child node configuration using CLI Helper Functions API
- CoAP PUT request sending with confirmation
- Automatic network join with retry mechanism
- Visual status indication using RGB LED (Red = failed, Blue = ready)
- Button debouncing for reliable input

## Hardware Requirements

- ESP32 compatible development board with Thread support (ESP32-H2, ESP32-C6, or ESP32-C5)
- User button (BOOT button or external button)
- RGB LED for status indication (optional, but recommended)
- USB cable for Serial communication
- A CoAP Lamp device (coap_lamp example) must be running first

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with OpenThread support
3. ESP32 Arduino libraries:
   - `OpenThread`

### Configuration

Before uploading the sketch, you can modify the network and CoAP configuration:

```cpp
#define USER_BUTTON           9  // C6/H2 Boot button (change if needed)
#define OT_CHANNEL            "24"
#define OT_NETWORK_KEY        "00112233445566778899aabbccddeeff"
#define OT_MCAST_ADDR         "ff05::abcd"
#define OT_COAP_RESOURCE_NAME "Lamp"
```

**Important:**
- The network key and channel **must match** the Lamp device configuration
- The multicast address and resource name **must match** the Lamp device
- The network key must be a 32-character hexadecimal string (16 bytes)
- The channel must be between 11 and 26 (IEEE 802.15.4 channels)
- **Start the Lamp device first** before starting this Switch device

## Building and Flashing

1. **First, start the Lamp device** using the coap_lamp example
2. Open the `coap_switch.ino` sketch in the Arduino IDE.
3. Select your ESP32 board from the **Tools > Board** menu (ESP32-H2, ESP32-C6, or ESP32-C5).
4. Connect your ESP32 board to your computer via USB.
5. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
Starting OpenThread.
Running as Switch - use the BOOT button to toggle the other C6/H2 as a Lamp
OpenThread started.
Waiting for activating correct Device Role.
........
Device is Router.
OpenThread setup done. Node is ready.
```

The RGB LED will turn **blue** when the device is ready to send CoAP commands.

## Using the Device

### Switch Device Setup

The switch device automatically:
1. Joins the existing Thread network (created by the Lamp Leader)
2. Configures itself as a Router or Child node
3. Creates a CoAP client
4. Waits for button presses to send CoAP commands

### Button Control

- **Press the button**: Toggles the lamp state (ON/OFF)
- The switch sends CoAP PUT requests to the lamp:
  - Payload "1" = Turn lamp ON
  - Payload "0" = Turn lamp OFF

### Visual Status Indication

The RGB LED provides visual feedback:
- **Red**: Setup failed or CoAP request failed
- **Blue**: Device is ready and can send CoAP commands
- **Red (after button press)**: CoAP request failed, device will restart setup

### Working with Lamp Device

1. Start the Lamp device first (coap_lamp example)
2. Start this Switch device with matching network key and channel
3. Wait for Switch device to join the network (LED turns blue)
4. Press the button on the Switch device
5. The lamp on the other device should toggle ON/OFF

## Code Structure

The coap_switch example consists of the following main components:

1. **`otDeviceSetup()` function**:
   - Configures the device to join an existing network using CLI Helper Functions
   - Sets up CoAP client
   - Waits for device to become Router or Child
   - Returns success/failure status

2. **`setupNode()` function**:
   - Retries setup until successful
   - Calls `otDeviceSetup()` with Router/Child role configuration

3. **`otCoapPUT()` function**:
   - Sends CoAP PUT request to the lamp device
   - Waits for CoAP confirmation response
   - Returns success/failure status
   - Uses CLI Helper Functions to send commands and read responses

4. **`checkUserButton()` function**:
   - Monitors button state with debouncing
   - Toggles lamp state on button press
   - Calls `otCoapPUT()` to send commands
   - Restarts setup if CoAP request fails

5. **`setup()`**:
   - Initializes Serial communication
   - Starts OpenThread stack with `OpenThread.begin(false)`
   - Initializes OpenThread CLI
   - Sets CLI timeout
   - Calls `setupNode()` to configure the device

6. **`loop()`**:
   - Continuously calls `checkUserButton()` to monitor button input
   - Small delay for responsiveness

## Troubleshooting

- **LED stays red**: Setup failed. Check Serial Monitor for error messages. Verify network configuration matches Lamp device.
- **Button press doesn't toggle lamp**: Ensure Lamp device is running and both devices are on the same Thread network. Check that network key, channel, multicast address, and resource name match.
- **Device not joining network**: Ensure Lamp device (Leader) is running first. Verify network key and channel match exactly.
- **CoAP request timeout**: Check Thread network connectivity. Verify multicast address and resource name match the Lamp device. Ensure Lamp device is responding.
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [OpenThread CLI Helper Functions API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_cli.html)
- [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html)
- [OpenThread Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html)
- [CoAP Protocol](https://coap.technology/)

## License

This example is licensed under the Apache License, Version 2.0.
