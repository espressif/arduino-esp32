# Touch Sensor Validation Test

Validates the Arduino `touchRead` and `touchAttachInterrupt` APIs using hardware-level faking of touch press/release events via charge speed or internal capacitor manipulation.

## Test Cases

| Test Function | Description |
|---|---|
| `test_touch_read` | Read all touch channels in unpressed and faked-press states, verify value difference exceeds threshold |
| `test_touch_interrtupt` | Attach interrupts to two touch channels, fake press, verify both callbacks fired |
| `test_touch_errors` | Call `touchRead` on a non-touch GPIO, verify it returns 0 |

## Requirements

- **Hardware**: Any ESP32 variant with touch sensor support (ESP32, ESP32-S2, ESP32-S3, ESP32-P4)
- **Wokwi/QEMU**: Not supported
- **SoC Config**: `CONFIG_SOC_TOUCH_SENSOR_SUPPORTED=y`
- **FQBN**: `DebugLevel=verbose`

## Notes

- Touch press/release is faked in software by changing the charge speed (touch sensor v1/v2) or internal capacitor value (v3/ESP32-P4), so no physical touch input is needed.
- The expected value difference between pressed and unpressed states varies by chip: -200 for ESP32, +200 for ESP32-P4, and +2000 for ESP32-S2/S3.
