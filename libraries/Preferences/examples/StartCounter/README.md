# Preferences startup counter

Counts how many times the board has booted. The count is stored in the `my-app` namespace under the key `counter`, then the board restarts after 10 seconds.

## Supported Targets

Any ESP32 board.

## How to Use Example

Open the Serial Monitor at 115200 baud, then flash the sketch. Each boot reads the counter, adds one, prints it, and saves it. The first boot prints `1`.

Namespace and key names are limited to 15 characters.

Expected log:

```
Current counter value: 1
Restarting in 10 seconds...
```

The next boot prints `2`, and so on. The value stays in NVS across restarts. To start again at `1`, uncomment `preferences.clear()` or `preferences.remove("counter")` in the sketch.
