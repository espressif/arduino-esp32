# OpenThread CLI onReceive Callback Example

This example demonstrates how to use the OpenThread CLI callback mechanism to capture and process CLI responses asynchronously.\
The application shows how to set up a callback function that processes CLI responses line by line, allowing non-blocking CLI interaction.

## Supported Targets

| SoC | Thread | Status |
| --- | ------ | ------ |
| ESP32-H2 | ✅ | Fully supported |
| ESP32-C6 | ✅ | Fully supported |
| ESP32-C5 | ✅ | Fully supported |

### Note on Thread Support:

- Thread support must be enabled in the ESP-IDF configuration (`CONFIG_OPENTHREAD_ENABLED`). This is done automatically when using the ESP32 Arduino OpenThread library.
- This example uses `OpenThread.begin()` which automatically starts a Thread network with default settings.

## Features

- CLI response callback using `OpenThreadCLI.onReceive()`
- Asynchronous CLI response processing
- Non-blocking CLI command execution
- Demonstrates callback-based CLI interaction pattern
- Automatic Thread network startup with default settings
- Device role monitoring via CLI

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

No configuration is required before uploading the sketch. The example automatically starts a Thread network with default settings.

## Building and Flashing

1. Open the `onReceive.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu (ESP32-H2, ESP32-C6, or ESP32-C5).
3. Connect your ESP32 board to your computer via USB.
4. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
OpenThread CLI RESP===> disabled
OpenThread CLI RESP===> disabled
OpenThread CLI RESP===> detached
OpenThread CLI RESP===> child
OpenThread CLI RESP===> router
OpenThread CLI RESP===> router
...
```

The callback function processes each line of CLI response, showing the device state transitions from "disabled" to "detached" to "child" to "router" (or "leader").

## Using the Device

### Callback Mechanism

The example demonstrates the callback-based CLI interaction pattern:

1. **Callback Registration**: `OThreadCLI.onReceive(otReceivedLine)` registers a callback function
2. **Command Execution**: `OThreadCLI.println("state")` sends CLI commands
3. **Response Processing**: The callback function `otReceivedLine()` processes responses asynchronously
4. **Non-blocking**: The main loop continues while CLI responses are processed in the callback

### OpenThread worker task (important)

`onReceive()` runs on the **OpenThread worker task**, not in `loop()`. Keep the
callback short: do not block, sleep, or call synchronous CLI helpers such as
`otGetRespCmd()` or `otExecCommand()` from inside `otReceivedLine()` — they can
deadlock waiting on the same task. Parse or copy data in the callback and defer
heavier work (including those helpers) to `loop()` or another task. See
`OThreadCLI.h` and the OpenThread CLI documentation for the full note.

### Device State Monitoring

The example continuously monitors the device state:
- Sends "state" command every second
- Callback processes the response
- Shows state transitions as the device joins the Thread network

### Customizing the Callback

You can modify the `otReceivedLine()` function to:
- Parse specific CLI responses
- Extract data from CLI output
- Trigger actions based on CLI responses
- Filter or process specific response patterns

## Code Structure

The onReceive example consists of the following main components:

1. **`otReceivedLine()` callback function**:
   - Reads all available data from OpenThread CLI
   - Filters out empty lines (EOL sequences)
   - Prints non-empty lines with a prefix

2. **`setup()`**:
   - Initializes Serial communication
   - Starts OpenThread stack with `OpenThread.begin()` (auto-start)
   - Initializes OpenThread CLI
   - Registers the callback function using `OThreadCLI.onReceive(otReceivedLine)`

3. **`loop()`**:
   - Sends "state" CLI command every second using `OThreadCLI.println("state")`
   - The callback function processes the response asynchronously
   - Non-blocking operation allows other tasks to run

## Troubleshooting

**Startup order (multi-board):** Flash the **first board** first — it becomes Leader and forms the network. Flash additional boards only after the first is attached. All boards must share the same network key and channel; reset any board that booted before the Leader was ready.

| Symptom | Likely cause |
| --- | --- |
| No callback responses | Callback not registered in `setup()`, or `OThreadCLI` not initialized. |
| Empty lines filtered from output | Normal — callback skips EOL-only lines. |
| State not changing | Wait for attach — first device becomes Leader, subsequent devices Router/Child; send `state` only after `OThread.begin()`. |
| No serial output | Serial Monitor not at **115200** or USB disconnected. |

## Related Documentation

- [OpenThread CLI Helper Functions API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_cli.html)
- [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html)
- [OpenThread Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html)

## License

This example is licensed under the Apache License, Version 2.0.
