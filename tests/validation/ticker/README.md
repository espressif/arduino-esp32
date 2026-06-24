# Ticker Validation Test

Validates the Ticker library: initial inactive state, periodic callbacks (attach/attach_ms/attach_us), one-shot callbacks (once/once_ms/once_us), active() state transitions, detach behavior, restart with period change, re-attach while running, typed argument callbacks, and large struct arguments.

## Test Cases

| Test Function | Description |
|---|---|
| `test_initially_inactive` | New Ticker reports `active() == false` |
| `test_attach_ms_periodic` | 100 ms periodic fires ~5 times in 550 ms |
| `test_attach_us_periodic` | 100000 µs periodic fires ~5 times in 550 ms |
| `test_attach_seconds_periodic` | 0.1 s periodic fires ~5 times in 550 ms |
| `test_once_ms` | One-shot fires exactly once, becomes inactive |
| `test_once_us` | One-shot (µs) fires exactly once |
| `test_once_seconds` | One-shot (float seconds) fires exactly once |
| `test_active_while_running` | `active()` is true while attached, false after detach |
| `test_detach_stops_periodic` | No callbacks fire after detach |
| `test_detach_on_inactive_is_safe` | Double-detach does not crash |
| `test_restart_ms` | Change period from 5 s to 100 ms via restart |
| `test_restart_us` | Restart with µs period |
| `test_restart_seconds` | Restart with float seconds |
| `test_reattach_while_running` | Re-attach without manual detach succeeds |
| `test_reattach_different_callback` | Switch callback while running |
| `test_attach_ms_with_arg` | Periodic callback receives typed int argument |
| `test_once_ms_with_arg` | One-shot callback receives typed int argument |
| `test_once_ms_with_large_arg` | One-shot with struct argument (> sizeof(void*)) |
| `test_attach_ms_with_large_arg` | Periodic with struct argument |

## Requirements

- **Hardware**: Any ESP32 variant
- **Wokwi**: Supported
- **QEMU**: Supported

## Notes

- All timing assertions use `TEST_ASSERT_INT_WITHIN` to tolerate ±1 jitter from simulation.
- setUp/tearDown detach the ticker between tests for isolation.
- Large argument tests verify the Ticker template correctly copies structs larger than a pointer.
