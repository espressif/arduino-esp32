# I2C Master Validation Test

Validates the Wire (I2C master) library by communicating with a simulated DS1307 RTC in Wokwi. Tests cover bus scanning, time set/get, clock speed changes, pin swapping, and API methods.

## Test Cases

| Test Function | Description |
|---|---|
| `scan_bus` | Scan all I2C addresses (1–126), verify DS1307 found at 0x68 |
| `scan_bus_with_wifi` | Scan I2C bus while Wi-Fi is connected (Wi-Fi supported SoCs only) |
| `rtc_set_time` | Set DS1307 time, read back, verify all fields match |
| `rtc_run_clock` | Start RTC clock for 5 s, verify seconds advanced, stop and verify halted |
| `change_clock` | Switch I2C to 400 kHz, verify `getClock()`, read/write RTC at new speed |
| `swap_pins` | Swap SDA/SCL pins via `setPins()`, verify RTC communication still works |
| `test_api` | Test `setBufferSize()`, `setTimeOut()`, `getTimeOut()`, `peek()`, `flush()` |

## Requirements

- **Hardware**: Not supported (requires Wokwi DS1307 simulation)
- **Wokwi**: Supported (per-SoC diagram files with DS1307 RTC)
- **QEMU**: Not supported
- **SoC Config**: `CONFIG_SOC_I2C_SUPPORTED=y`

## Notes

- The `scan_bus_with_wifi` test only runs on SoCs with Wi-Fi support (`SOC_WIFI_SUPPORTED`).
- Each test resets the RTC time in `tearDown()` to ensure test isolation.
