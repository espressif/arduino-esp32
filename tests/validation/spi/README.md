# SPI Validation Test

Validates the SPI library API using a Wokwi custom echo chip. The chip echoes back the bytes received in the previous CS transaction, enabling a prime-then-echo verification pattern for all transfer methods.

## Test Cases

| Test Function | Description |
|---|---|
| `test_begin_end` | Exercise `SPI.begin()` and `SPI.end()` cycle |
| `test_transfer_byte` | Single byte `transfer()` with echo verification |
| `test_transfer16` | 16-bit `transfer16()` with echo verification |
| `test_transfer32` | 32-bit `transfer32()` with echo verification |
| `test_transfer_buffer` | In-place buffer `transfer(buf, size)` with echo verification |
| `test_transfer_bytes` | Separate in/out `transferBytes()` with echo verification |
| `test_write_byte` | `write()` single byte, verify via transfer echo |
| `test_write16` | `write16()` 16-bit value, verify via transfer echo |
| `test_write32` | `write32()` 32-bit value, verify via transfer echo |
| `test_write_bytes` | `writeBytes()` multi-byte, verify via echo |
| `test_write_pixels` | `writePixels()` verifies per-pixel byte swap (ILI9341-style) |
| `test_write_pattern` | `writePattern()` repeats a 2-byte pattern 3 times, verify echo |
| `test_begin_transaction` | `beginTransaction()`/`endTransaction()` with SPISettings |
| `test_set_frequency` | `setFrequency()` changes clock divider, data integrity preserved |
| `test_pin_ss` | `pinSS()` returns correct SS pin |
| `test_stress` | 256-byte incrementing pattern transfer and echo verification |

## Requirements

- **Hardware**: Not supported (requires Wokwi custom SPI echo chip)
- **Wokwi**: Supported (per-SoC diagram files with custom echo chip)
- **QEMU**: Not supported
