# UART Validation Test

Comprehensive Unity test suite for the `HardwareSerial` (UART) driver covering transmission, baudrate changes, pin management, peripheral manager integration, CPU frequency changes, hardware flow control, IrDA mode, RX internal pull, and one-wire UART.

## Test Cases

Tests run in the order listed in `setup()` (`RUN_TEST` sequence).

| Test Function | Description |
|---|---|
| `begin_when_running_test` | Call `begin()` twice on a running UART without crash |
| `basic_transmission_test` | Transmit and receive via `onReceive` + internal loopback on default pins |
| `resize_buffers_test` | RX/TX buffer resize while running (expect error) and while stopped (expect success) |
| `change_baudrate_test` | Change baudrate to 9600 and back; verify transmission at each rate |
| `change_cpu_frequency_test` | Change CPU frequency; verify UART at new and restored frequency |
| `disabled_uart_calls_test` | Call UART APIs on a stopped UART; verify safe return values / no crash |
| `enabled_uart_calls_test` | Echo a message, then exercise running-UART APIs (`peek`, `setRxTimeout`, inverts, etc.); verify pre-begin APIs fail while running |
| `auto_baudrate_test` | Auto baudrate detection (**ESP32 and ESP32-S2 only**) |
| `periman_test` | Attach I2C to UART RX pin; verify UART cannot receive; reattach UART and verify reception |
| `change_pins_test` | Move RX to alternate GPIO (keep TX on default when only one test UART); verify transmission |
| `irda_mode_test` | IrDA mode/direction API checks; with 2+ UARTs, cross-UART IrDA RX/TX behavior at 9600 baud |
| `inter_uart_pin_move_test` | Move UART1 to UART2's pins; UART2 terminates (**requires 2+ test UARTs**) |
| `same_uart_pin_swap_test` | Swap RX/TX on the same UART; driver stays up; echo works |
| `move_rx_tx_to_cts_rts_test` | Move RX/TX into CTS/RTS slots; UART terminates (no valid RX/TX) |
| `cross_uart_cts_rts_test` | Assign another UART's pins as CTS/RTS; source UART terminates (**requires 2+ test UARTs**) |
| `rx_pull_pre_begin_api_test` | `enableRxInternalPull` succeeds only before `begin()` |
| `rx_pull_inversion_test` | RX pull-up/down vs `begin(invert)`, `setRxInvert`, not `setTxInvert`; disabled pull → float |
| `same_pin_validation_test` | Same-pin auto-detects one-wire; shared-pin CTS/RTS overlap is rejected; `-1` preserves shared pins and non-conflicting CTS updates; implicit split↔one-wire transitions preserve the kept pin, bus ownership, driver, and RX pull |
| `same_pin_pull_disabled_test` | One-wire same pin → RX pad floating even with invert |
| `same_pin_transmission_test` | One-wire echo on `TEST_AUX_PIN` (`SCL`) with internal loopback |
| `same_pin_periman_test` | Shared pin registers as `UART_RX_TX`; `pinMode()` terminates UART |
| `same_pin_mode_rejection_test` | RS485/IrDA reject same-pin `setPins`; `setMode` rejects active one-wire |
| `same_pin_split_transition_test` | Split pins → one-wire (`end`/`begin`) → back to split via `setPins`; driver stays up; echo at each stage |
| `rx_pull_setpins_reattach_test` | Pull follows RX pin after `setPins` swap; updates after `setRxInvert` |
| `end_when_stopped_test` | Call `end()` twice without crash |
| `hardware_flow_control_test` | RTS/CTS on spare GPIOs with internal TX→RX and RTS→CTS loopback; ON/OFF flow control |

## Requirements

- **Hardware**: Any ESP32 variant with at least **one HP UART besides UART0** (UART0/`Serial` is used only for status output)
- **Multi-UART tests**: `inter_uart_pin_move_test`, `cross_uart_cts_rts_test`, and the functional section of `irda_mode_test` require **2+ HP UARTs** beyond UART0 (e.g. ESP32-S3 with Serial1 + Serial2)
- **Wokwi/QEMU**: QEMU not supported

## Pin assignments (sketch constants)

Default test UART pins come from board macros (`RX1`/`TX1`, `RX2`/`TX2`, …). See the pin table at the top of `uart.ino`.

Additional constants come from the board `pins_arduino.h` I2C pin variables (`SDA`, `SCL`):

| Constant | Source | Use |
|---|---|---|
| `NEW_RX1` | `SDA` | Alternate RX for pin-change, pull, and flow-control (CTS) tests |
| `NEW_TX1` | `SCL` | Alternate TX for split-pin tests; RTS in flow-control test |
| `TEST_AUX_PIN` | `SCL` | One-wire tests (same pad as `NEW_TX1`, but never active as split TX in the same step) |

Compile-time checks reject boards where `SDA`/`SCL` overlap `Serial1`/`Serial2` default UART pins. One-wire stages use `SCL` so split-pin steps can use `SDA` (RX) and `SCL` (TX) without two roles on one GPIO at once.

On ESP32-P4, UART1 defaults are hardcoded to RX=2, TX=3 for EV-board compatibility.

Hardware flow control assigns CTS/RTS to `SDA`/`SCL` while data RX/TX remain on the UART default pins.

## Test harness behavior

- **`setUp()`** (before each test): `end()` → `begin()` → register stable `onReceive` callback → establish internal loopback on the current RX pin.
- **`tearDown()`** (after each test): `end()` on all configured test UARTs.
- **Loopback**: `transmit_and_check_msg()` re-applies loopback, drains RX, sends a string, polls `available()` (and invokes `onReceive` synchronously when data is pending).
- **Internal loopback mechanism**: On native IOMUX RX pins (e.g. ESP32-S3 UART1 RX=15), the core uses peripheral loopback (`uart_set_loop_back`). On GPIO-matrix RX pins or cross-UART routing, it uses GPIO-matrix TX→RX wiring. See `uart_internal_loopback()` in `esp32-hal-uart.c`.

## Notes

- UART0 (`Serial`) is reserved for Unity status and diagnostics only.
- `auto_baudrate_test` is compiled only on ESP32 and ESP32-S2.
- One-wire and pull tests operate on `Serial1` directly (outside the shared `UARTTestConfig` vector) and call `Serial1.end()` at the start of each test.
- `change_pins_test` with a single test UART moves **RX only** (`setPins(NEW_RX1, -1)`); TX stays on the default pin.
