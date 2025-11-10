# OpenThread Thread Scan Example

This example demonstrates how to scan for IEEE 802.15.4 devices and Thread networks using OpenThread CLI commands.\
The application continuously scans for nearby devices and Thread networks, showing both raw IEEE 802.15.4 scans and Thread-specific discovery scans.

## Supported Targets

| SoC | Thread | Status |
| --- | ------ | ------ |
| ESP32-H2 | ✅ | Fully supported |
| ESP32-C6 | ✅ | Fully supported |
| ESP32-C5 | ✅ | Fully supported |

### Note on Thread Support:

- Thread support must be enabled in the ESP-IDF configuration (`CONFIG_OPENTHREAD_ENABLED`). This is done automatically when using the ESP32 Arduino OpenThread library.
- This example uses `OpenThread.begin(true)` which automatically starts a Thread network, required for Thread discovery scans.

## Features

- IEEE 802.15.4 device scanning (works even when Thread is not started)
- Thread network discovery scanning (requires device to be in Child, Router, or Leader state)
- Continuous scanning with configurable intervals
- Demonstrates CLI Helper Functions API for scanning
- Useful for network discovery and debugging

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

No configuration is required before uploading the sketch. The example automatically starts a Thread network for discovery scanning.

## Building and Flashing

1. Open the `ThreadScan.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu (ESP32-H2, ESP32-C6, or ESP32-C5).
3. Connect your ESP32 board to your computer via USB.
4. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
This sketch will continuously scan the Thread Local Network and all devices IEEE 802.15.4 compatible

Scanning for nearby IEEE 802.15.4 devices:
| J | Network Name     | Extended PAN     | PAN  | MAC Address      | Ch | dBm | LQI |
+---+------------------+------------------+------+------------------+----+-----+-----+
| 0 | OpenThread-ESP   | dead00beef00cafe | 1234 | 1234567890abcdef | 24 | -45 | 255 |
Done

Scanning MLE Discover:
| J | Network Name     | Extended PAN     | PAN  | MAC Address      | Ch | dBm | LQI |
+---+------------------+------------------+------+------------------+----+-----+-----+
| 0 | OpenThread-ESP   | dead00beef00cafe | 1234 | 1234567890abcdef | 24 | -45 | 255 |
Done
```

## Using the Device

### Scanning Behavior

The example performs two types of scans:

1. **IEEE 802.15.4 Scan**:
   - Scans for all IEEE 802.15.4 compatible devices in the area
   - Works even when the device is not part of a Thread network
   - Shows devices on all channels

2. **Thread Discovery Scan (MLE Discover)**:
   - Scans for Thread networks specifically
   - Only works when the device is in Child, Router, or Leader state
   - Shows Thread networks with their network names and parameters

### Scan Results

The scan results show:
- **J**: Joinable flag (1 = can join, 0 = cannot join)
- **Network Name**: Thread network name
- **Extended PAN**: Extended PAN ID
- **PAN**: PAN ID
- **MAC Address**: Device MAC address
- **Ch**: Channel
- **dBm**: Signal strength in dBm
- **LQI**: Link Quality Indicator

### Continuous Scanning

The example continuously scans:
- IEEE 802.15.4 scan every 5 seconds
- Thread discovery scan every 5 seconds (only if device is in Thread network)

## Code Structure

The ThreadScan example consists of the following main components:

1. **`setup()`**:
   - Initializes Serial communication
   - Starts OpenThread stack with `OpenThread.begin(true)` (auto-start required for discovery)
   - Initializes OpenThread CLI
   - Sets CLI timeout to 100 ms using `OThreadCLI.setTimeout()`

2. **`loop()`**:
   - Performs IEEE 802.15.4 scan using `otPrintRespCLI("scan", Serial, 3000)`
   - Checks if device is in Thread network (Child, Router, or Leader)
   - If in network, performs Thread discovery scan using `otPrintRespCLI("discover", Serial, 3000)`
   - Waits 5 seconds between scan cycles

## Troubleshooting

- **No scan results**: Ensure there are Thread devices nearby. Check that other devices are running and on the same channel.
- **Discovery scan not working**: Wait for the device to join a Thread network (should become Child, Router, or Leader)
- **Scan timeout**: Increase the timeout value in `otPrintRespCLI()` if scans are taking longer
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [OpenThread CLI Helper Functions API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_cli.html)
- [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html)
- [OpenThread Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html)

## License

This example is licensed under the Apache License, Version 2.0.
