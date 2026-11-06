# Peripheral Manager Validation Test

Validates the Peripheral Manager (periman) ability to attach and detach peripherals on shared pins. Each test attaches a peripheral to the UART1 pins, verifies that UART1 is auto-detached, then restores UART1 via the peripheral manager. Peripherals with manual deinit support are also tested for the explicit deinit path.

## Test Cases

| Test Function | Description |
|---|---|
| `gpio_test` | GPIO `pinMode`/`digitalRead`/`digitalWrite` auto-detaches UART1 |
| `sigmadelta_test` | SigmaDelta attach auto-detaches UART1; also tests manual `sigmaDeltaDetach` |
| `ledc_test` | LEDC attach auto-detaches UART1; also tests manual `ledcDetach` |
| `rmt_test` | RMT init auto-detaches UART1; also tests manual `rmtDeinit` |
| `i2s_test` | I2S begin auto-detaches UART1; also tests manual `i2s.end()` |
| `i2c_test` | I2C (Wire) begin auto-detaches UART1; also tests manual `Wire.end()` |
| `spi_test` | SPI begin auto-detaches UART1; also tests manual `SPI.end()` |
| `adc_oneshot_test` | ADC oneshot `analogRead` auto-detaches UART1 |
| `adc_continuous_test` | ADC continuous mode auto-detaches UART1; also tests manual `analogContinuousDeinit` |
| `dac_test` | DAC `dacWrite` auto-detaches UART1 |
| `touch_test` | Touch `touchRead` auto-detaches UART1 |

## Requirements

- **Hardware**: One ESP32 board (peripherals are tested based on SoC capabilities)
- **Wokwi/QEMU**: Not supported
- **SoC Config**: Runs on all targets; peripheral tests are conditionally compiled based on `SOC_*_SUPPORTED` macros

## Notes

- UART1 is configured with internal loopback so no external wiring is needed.
- Each test follows the pattern: attach peripheral → verify UART1 is detached (auto-detach via `pinMode`) → restore UART1 → verify UART1 works again.
- For peripherals with deinit support, a second sub-test calls the manual deinit function and verifies UART1 can be restored afterward.
- Peripherals that cannot be tested: USB (cannot detach), SDMMC (requires mounted card), ETH (requires Ethernet connection).
