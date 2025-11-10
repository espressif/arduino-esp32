# OpenThread Simple CLI Example

This example demonstrates how to use the OpenThread CLI (Command-Line Interface) for interactive control of a Thread network node using an ESP32 SoC microcontroller.\
The application provides an interactive console where you can manually configure and control the Thread network using OpenThread CLI commands.

## Supported Targets

| SoC | Thread | Status |
| --- | ------ | ------ |
| ESP32-H2 | ✅ | Fully supported |
| ESP32-C6 | ✅ | Fully supported |
| ESP32-C5 | ✅ | Fully supported |

### Note on Thread Support:

- Thread support must be enabled in the ESP-IDF configuration (`CONFIG_OPENTHREAD_ENABLED`). This is done automatically when using the ESP32 Arduino OpenThread library.
- This example uses `OpenThread.begin(false)` which does not automatically start a Thread network, allowing you to manually configure it using CLI commands.

## Features

- Interactive OpenThread CLI console via Serial Monitor
- Manual Thread network configuration using CLI commands
- Full control over Thread network parameters (network name, channel, PAN ID, network key, etc.)
- Support for all OpenThread CLI commands
- Useful for learning OpenThread CLI and debugging Thread networks

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

No configuration is required before uploading the sketch. The example starts with a fresh Thread stack that is not automatically started, allowing you to configure it manually using CLI commands.

## Building and Flashing

1. Open the `SimpleCLI.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu (ESP32-H2, ESP32-C6, or ESP32-C5).
3. Connect your ESP32 board to your computer via USB.
4. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
OpenThread CLI started - type 'help' for a list of commands.
ot>
```

The `ot> ` prompt indicates that the OpenThread CLI console is ready. You can now type OpenThread CLI commands directly.

## Using the Device

### Interactive CLI Commands

Type OpenThread CLI commands in the Serial Monitor. Some useful commands to get started:

**Get help:**
```
help
```

**Initialize a new dataset:**
```
dataset init new
```

**Set network parameters:**
```
dataset networkname MyThreadNetwork
dataset channel 15
dataset networkkey 00112233445566778899aabbccddeeff
```

**Commit and start the network:**
```
dataset commit active
ifconfig up
thread start
```

**Check device state:**
```
state
```

**View network information:**
```
networkname
channel
panid
ipaddr
```

**For a complete list of OpenThread CLI commands, refer to the** [OpenThread CLI Reference](https://openthread.io/reference/cli).

### Example Workflow

1. **Initialize a new Thread network (Leader):**
   ```
   dataset init new
   dataset networkname MyNetwork
   dataset channel 15
   dataset networkkey 00112233445566778899aabbccddeeff
   dataset commit active
   ifconfig up
   thread start
   ```

2. **Join an existing network (Router/Child):**
   ```
   dataset clear
   dataset networkkey 00112233445566778899aabbccddeeff
   dataset channel 15
   dataset commit active
   ifconfig up
   thread start
   ```

3. **Check network status:**
   ```
   state
   networkname
   ipaddr
   ```

## Code Structure

The SimpleCLI example consists of the following main components:

1. **`setup()`**:
   - Initializes Serial communication
   - Starts OpenThread stack with `OpenThread.begin(false)` (no auto-start)
   - Initializes OpenThread CLI
   - Starts the interactive CLI console on Serial

2. **`loop()`**:
   - Empty - all interaction happens through the CLI console

## Troubleshooting

- **CLI not responding**: Ensure Serial Monitor is set to 115200 baud and "Both NL & CR" line ending
- **Commands not working**: Make sure OpenThread stack is initialized (check for "OpenThread CLI started" message)
- **Network not starting**: Verify that you've committed the dataset and started the interface before starting Thread
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [OpenThread CLI Helper Functions API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_cli.html)
- [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html)
- [OpenThread Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html)

## License

This example is licensed under the Apache License, Version 2.0.
