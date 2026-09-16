# Clock Validation Test

Validates the CPU and bus clock API: the list of supported CPU frequencies, that every listed frequency can be applied exactly and every unlisted one is refused, the clock source names, the reported APB frequency, and that a configured SPI bit rate survives a CPU frequency change.

## Test Cases

| Test Function | Description |
|---|---|
| `test_supported_frequency_list` | List is non-empty, descending, and contains the frequency the chip booted at |
| `test_every_supported_frequency_applies` | Every listed frequency is accepted and `getCpuFrequencyMhz()` returns it exactly |
| `test_unsupported_frequencies_are_refused` | 0 MHz, 1000 MHz and frequencies missing from the list are refused, and leave the CPU frequency untouched |
| `test_clock_source_names` | Every clock source the chip selects across the list has a name; invalid input returns `Invalid` |
| `test_apb_frequency_matches_hardware` | `getApbFrequency()` equals the APB frequency the HAL derives from the dividers in hardware |
| `test_spi_bitrate_matches_request` | A timed 2 KB transfer confirms the bus really runs at the 1 MHz that was requested |
| `test_spi_frequency_survives_cpu_change` | A 1 MHz SPI bit rate stays within 10% across every CPU frequency |

## Requirements

- **Hardware**: Any ESP32 variant
- **Wokwi**: Not supported (the clock tree is not modeled)
- **QEMU**: Not supported (the clock tree is not modeled)

## Notes

- The list of supported frequencies is read from `getSupportedCpuFrequencyMhz()` rather than hardcoded per target, so the same test covers every variant and follows whatever the chip and IDF version support.
- The console prints at every frequency under test, so a frequency that breaks the UART shows up as garbled or missing output instead of as a passing test.
- `test_unsupported_frequencies_are_refused` checks at most eight unlisted frequencies, which is enough to cover the gaps between the crystal dividers on every target.
- The APB comparison uses `clk_hal_apb_get_freq_hz()`, which reads the dividers the IDF programmed, so the assertion does not simply restate the core's own arithmetic.
- `test_spi_bitrate_matches_request` is a timed measurement because a divider computed from the wrong source frequency is self-consistent, and therefore invisible to `spiClockDivToFrequency()`. It only runs at the frequency the chip booted at, since feeding the FIFO in software distorts the measurement once the CPU is slow.
- No SPI wiring is needed: the test measures how long the peripheral takes to shift the bytes out, so MOSI can be left unconnected.
