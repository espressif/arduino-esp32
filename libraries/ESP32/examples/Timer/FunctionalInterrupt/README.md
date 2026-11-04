# Hardware Timer FunctionalInterrupt Example

This example attaches several kinds of callbacks to one hardware timer so you
can compare the APIs. Every 5 alarms it re-attaches the next style. Press the
BOOT button to stop and free the timer.

## Supported Targets

All Arduino-ESP32 targets that provide a general-purpose hardware timer
(`SOC_GPTIMER_SUPPORTED`).

## Callback styles

1. **C function pointer** — `timerAttachInterrupt(timer, &bumpCounter)`. This is
   the C overload. `FunctionalInterrupt.h` is not required for this form.
2. **Capturing lambda** — `timerAttachInterrupt(timer, [&]() { ... })`. Include
   `FunctionalInterrupt.h`. A non-capturing lambda can be ambiguous with the C
   overload; capture a variable or wrap it in `std::function<void(void)>`.
3. **`std::function` object** — store the functor in DRAM
   (`std::function<void()> onTimerFn = ...`) and pass that. Do **not**
   mark the object `IRAM_ATTR`: on RISC-V (ESP32-C6 and similar) the
   constructor writes into instruction RAM and store-faults at boot.
4. **`std::bind` member** — `timerAttachInterrupt(timer, std::bind(&TickSource::isr, &tickSource))`.
   Same pattern as the GPIO `FunctionalInterrupt` example.
5. **`timerAttachInterruptArg`** — C callback with a context pointer, like GPIO
   `attachInterruptArg()`.

Re-attaching replaces the previous callback. `timerDetachInterrupt()` and
`timerEnd()` release any heap-allocated `std::function` state.

Keep ISR work short. Use `volatile` for data shared with `loop()`.
`std::function::operator()` and most lambda bodies are not placed in IRAM.
For an IRAM-safe callback, use a C function pointer with `ARDUINO_ISR_ATTR`.

## How to Use Example

### Hardware Connection

No extra wiring. Open the Serial Monitor at 115200 baud. Press the board BOOT
button to call `timerEnd()`.

### Configure the Project

Timer frequency is 1 MHz. The alarm fires every 1 000 000 ticks (1 second),
auto-reload, unlimited count:

```cpp
timer = timerBegin(1000000);
attachMode(currentMode);
timerAlarm(timer, 1000000, true, 0);
```

#### Using Arduino IDE

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port`.

## Example Output

```
Hardware Timer FunctionalInterrupt Example
Re-attaches a different callback style every 5 ticks.
Press BOOT to stop the timer.
Using C function pointer
[C function pointer] fired 1 times
[C function pointer] fired 5 times
Switched to capturing lambda
[capturing lambda] fired 6 times
Switched to std::function object
...
Timer stopped
```

## Troubleshooting

**Important: Make sure you are using a good quality USB cable and that you have a reliable power source**

* **No serial output:** Check the USB cable, the USB-to-serial driver, and the selected COM port.
* **Compile error about ambiguous `timerAttachInterrupt`:** The lambda does not capture. Capture a variable or wrap it in `std::function<void(void)>`.
* **Timer never fires:** Confirm `timerBegin()` did not return `NULL`.
* **Store access fault at boot (ESP32-C6 and other RISC-V chips):** A
  `std::function` was declared `IRAM_ATTR`. Keep the object in DRAM.

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
