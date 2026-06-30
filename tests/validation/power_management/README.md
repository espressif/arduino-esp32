# Power Management Validation Test

Validates power management features including task watchdog timer (TWDT), light sleep, deep sleep with timer wakeup, and RTC data persistence. Uses a two-phase approach with a deep sleep reboot between phases.

## Test Cases

### Phase 0 (Initial Boot)

| Test Function | Description |
|---|---|
| `test_reset_reason_power_on` | Reset reason is power-on or deep sleep |
| `test_twdt_feed` | Add task to TWDT, feed it, verify no timeout in 500 ms |
| `test_light_sleep_timer` | Enter light sleep for 500 ms, verify wakeup cause and elapsed time |

### Phase 1 (After Deep Sleep Wakeup)

| Test Function | Description |
|---|---|
| `test_deep_sleep_verify` | Wakeup cause is `ESP_SLEEP_WAKEUP_TIMER` |
| `test_rtc_data_persisted` | `RTC_DATA_ATTR` magic value (0xDEADBEEF) and boot count survive deep sleep |

## Requirements

- **Hardware**: Any ESP32 variant
- **Wokwi**: Not supported (RTC only partially simulated)
- **QEMU**: Not supported

## Serial Protocol

1. Phase 0: Unity test output for initial boot tests
2. Firmware prints `DEEP_SLEEP_START` then enters deep sleep for 2 seconds
3. Phase 1: Unity test output for deep sleep verification tests

## Notes

- The pytest script handles the two-phase output by calling `expect_unity_test_output()` twice, with a `DEEP_SLEEP_START` sentinel between them.
- `RTC_DATA_ATTR` variables (`boot_count`, `rtc_magic`) persist across deep sleep to verify RTC memory retention.
