# Standard library conversion validation

Checks `ltoa`, `lltoa`, and their Arduino `String` consumers without external
hardware or network access. Supports all ESP32 targets.

| Test | Coverage |
|---|---|
| `test_ltoa_signed_limits` | 32-bit minimum, adjacent value, maximum, zero, ordinary negatives, and known binary/decimal/hex output |
| `test_lltoa_signed_limits` | 64-bit minimum, adjacent value, maximum, zero, ordinary negatives, and known binary/decimal/hex output |
| `test_signed_conversion_all_radices` | Boundary and ordinary values round-trip through `strtol`/`strtoll` in every supported base (2 through 16) |
| `test_signed_conversion_invalid_radices` | Unsupported bases return the supplied buffer containing an empty string |
| `test_string_signed_minimum_values` | Signed-minimum String construction, hexadecimal construction, `concat`, and `+=` |

Run with the repository's test scripts:

```sh
bash .github/scripts/tests_build.sh -s stdlib -t esp32
bash .github/scripts/tests_run.sh -s stdlib -t esp32
```
