# Timer Validation Test

Validates the hardware timer API: timer read/write, interrupt attach/detach, frequency divider accuracy, one-shot alarm, auto-reload counting, stop/start behavior, multiple concurrent timers, and XTAL clock source selection.

## Test Cases

| Test Function | Description |
|---|---|
| `timer_read_test` | Write a value to timer, read it back, verify match |
| `timer_interrupt_test` | Attach interrupt, verify alarm fires; detach, verify it stops |
| `timer_divider_test` | Compare timer counts at different frequencies (4 MHz, 8 MHz, 250 kHz) |
| `timer_oneshot_test` | One-shot alarm fires exactly once |
| `timer_auto_reload_test` | Auto-reload alarm fires ~4 times in 1.1 s at 0.25 s period |
| `timer_stop_start_test` | Verify timer stops counting when stopped, resumes on start |
| `timer_multiple_test` | Run two timers at different frequencies concurrently |
| `timer_clock_select_test` | Select XTAL clock source at 1 kHz, verify frequency (non-ESP32 only) |

## Requirements

- **Hardware**: Any ESP32 variant
- **Wokwi**: Supported
- **QEMU**: Supported

## Notes

- Default timer frequency is 4 MHz (APB clock derived).
- `timer_clock_select_test` is skipped on original ESP32 (APB clock only, 1 kHz not achievable).
- Each test uses setUp/tearDown for timer init/deinit to ensure clean state.
