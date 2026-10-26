# Hardware Timer FunctionalInterrupt Example

This example attaches a capturing C++ lambda to a hardware timer using
`timerAttachInterrupt()` from `FunctionalInterrupt.h`. The callback increments
a counter every second. Press the BOOT button to stop and free the timer.

## Supported Targets

All Arduino-ESP32 targets that provide a general-purpose hardware timer
(`SOC_GPTIMER_SUPPORTED`).

## How to Use Example

* Include `<FunctionalInterrupt.h>` to get the `std::function` overload of `timerAttachInterrupt()`.
* A capturing lambda (or `std::function<void(void)>`) selects the C++ overload.
* A raw function pointer still uses the C overload.
* A non-capturing lambda can match both; capture a variable or wrap it in `std::function` if the compiler reports an ambiguity.

### Hardware Connection

No extra wiring. Open the Serial Monitor at 115200 baud. Press the board BOOT
button to call `timerEnd()`.

### Configure the Project

Timer frequency is 1 MHz. The alarm fires every 1 000 000 ticks (1 second),
auto-reload, unlimited count:

```cpp
timer = timerBegin(1000000);
timerAttachInterrupt(timer, onTimer);
timerAlarm(timer, 1000000, true, 0);
```

The callback is a global `IRAM_ATTR std::function<void()>`. Keep ISR work short
and use `volatile` for data shared with `loop()`. When `CONFIG_ARDUINO_ISR_IRAM`
is enabled, `std::function::operator()` and most lambda bodies are not placed in
IRAM; prefer this global `IRAM_ATTR` form.

#### Using Arduino IDE

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port`.

## Example Output

```
Hardware Timer FunctionalInterrupt Example
Press BOOT to stop the timer.
Timer lambda fired 1 times
Timer lambda fired 2 times
Timer lambda fired 3 times
Timer stopped
```

## Troubleshooting

**Important: Make sure you are using a good quality USB cable and that you have a reliable power source**

* **No serial output:** Check the USB cable, the USB-to-serial driver, and the selected COM port.
* **Compile error about ambiguous `timerAttachInterrupt`:** The lambda does not capture. Capture a variable or wrap it in `std::function<void(void)>`.
* **Timer never fires:** Confirm the sketch includes `FunctionalInterrupt.h` and that `timerBegin()` did not return `NULL`.

If the error persists, you can ask for help at the official [ESP32 forum](https://esp32.com) or see [Contribute](#contribute).

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.md)

If you have any **feedback** or **issue** to report on this example, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try Troubleshooting and check if the same issue was already created by someone else.

## Resources

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
* Timer API: [docs](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/timer.html)
