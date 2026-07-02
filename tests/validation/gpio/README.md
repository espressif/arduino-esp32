# GPIO Validation Test

Validates GPIO digital read/write, pull-up/pull-down modes, pin mode switching, and interrupt handling (FALLING, RISING, CHANGE edges, and interrupt with argument). Uses Wokwi simulation with pytest-driven synchronization.

## Test Cases

| Test Function | Description |
|---|---|
| `test_read_basic` | Read INPUT_PULLUP as HIGH, pytest presses button → LOW, releases → HIGH |
| `test_write_basic` | Set OUTPUT, write HIGH/LOW, pytest verifies LED pin state |
| `test_read_pulldown` | INPUT_PULLDOWN reads LOW, switch to INPUT_PULLUP reads HIGH |
| `test_pin_mode_switch` | OUTPUT HIGH → INPUT_PULLUP (reads HIGH) → OUTPUT LOW, pytest verifies each state |
| `test_interrupt_attach_detach` | Attach FALLING ISR, trigger 3 times, detach, verify no further interrupts |
| `test_interrupt_falling` | FALLING edge ISR triggers on 3 button presses |
| `test_interrupt_rising` | RISING edge ISR triggers on 3 button releases |
| `test_interrupt_change` | CHANGE edge ISR triggers on both press and release (6 total) |
| `test_interrupt_with_arg` | FALLING ISR with `attachInterruptArg`, verify argument passed correctly |

## Requirements

- **Hardware**: Not supported (requires Wokwi simulation for button/LED interaction)
- **Wokwi**: Supported (per-SoC diagram files with button and LED)
- **QEMU**: Not supported

## Serial Protocol

The test uses serial handshaking between the firmware and pytest for synchronization:

1. Firmware prints a status message (e.g., `BTN read as HIGH after pinMode INPUT_PULLUP`)
2. pytest reads the message, performs Wokwi actions (press button, read pin), then sends `OK\n`
3. Firmware's `waitForSyncAck()` unblocks and continues to the next assertion
4. For interrupt tests, pytest sends indexed acks (`OK:1\n`, `OK:2\n`, etc.) after each trigger

## Notes

- Pin assignments vary by SoC: BTN is GPIO14 on ESP32/S2/S3, GPIO2 on P4, GPIO10 on C-series; LED is always GPIO4.
- All interrupt tests detach the ISR after verification to avoid side effects on subsequent tests.
