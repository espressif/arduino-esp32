# USB Host Mouse (callback style)

Same device support as [USBHostMouse](../USBHostMouse/README.md), but each report is delivered through `setReportCallback()` instead of being polled with `available()` in `loop()`.

## Flash / run checklist

1. **Board / USB mode:** see the [USBHostMouse checklist](../USBHostMouse/README.md) for the **USB Mode** / **USB CDC On Boot** combination to avoid.
2. **Declare the handler in the sketch:** `USBHostHIDMouse USBHostMouse;`. The library ships no instance, so a sketch only pays for the handlers it names.
3. **`USBHostMouse.registerWithHost()`** and **`setReportCallback()`** both run in `setup()` **before** `USBHost.begin()`.
4. `loop()` only has to call **`USBHost.task()`**.

## Polling or callback?

Both styles see the same reports, so pick whichever fits the sketch:

- **Polling** ([USBHostMouse](../USBHostMouse/README.md)) reads the latest state whenever it suits you, and a burst of reports between two `available()` calls collapses into one.
- **Callback** (this example) fires once per report, so nothing is missed — useful for counting clicks or accumulating movement. It runs from `USBHost.task()`, on whichever task called it, so keep the handler short and do not block in it.

The callback signature is `void (int16_t x, int16_t y, uint8_t buttons, int8_t wheel, void *arg)`, where `arg` is the pointer passed as the second argument to `setReportCallback()`.
