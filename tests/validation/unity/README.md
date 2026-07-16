# Unity Framework Validation Test

Comprehensive smoke test that exercises the Unity assertion and control macros available in the ESP32 Arduino core build.

## Test Cases

| Test Function | Description |
|---|---|
| `test_boolean` | `TEST_ASSERT*`, null/empty, and `_MESSAGE` boolean variants |
| `test_shorthand_int` | `TEST_ASSERT_EQUAL` / `TEST_ASSERT_NOT_EQUAL` shorthand |
| `test_integers_equal` | Typed equal macros and array comparisons (int/uint/hex/char) |
| `test_integers_compare` | Not-equal, greater/less, and or-equal variants |
| `test_integers_within` | Delta and array-within macros |
| `test_bits` | `TEST_ASSERT_BITS*` and bit high/low macros |
| `test_strings_memory_ptr` | String, memory, and pointer comparisons |
| `test_each_equal` | `TEST_ASSERT_EACH_EQUAL_*` scalar-to-array macros |
| `test_float` | Float comparisons and special values (inf/nan) |
| `test_double` | Double comparisons and special values |
| `test_message_variants` | `TEST_MESSAGE` and representative `_MESSAGE` asserts |
| `test_esp_err_helpers` | ESP-IDF `TEST_ESP_OK` / `TEST_ESP_ERR` helpers |
| `test_hooks` | `setUp`/`tearDown` invocation and Unity version |
| `test_pass_early` | `TEST_PASS_MESSAGE` early exit |
| `test_ignore` | `TEST_IGNORE_MESSAGE` (counted as ignored, not failed) |

## Not covered

- 64-bit integer macros when `CONFIG_UNITY_ENABLE_64BIT` is disabled (default)
- `TEST_FAIL` / `TEST_IGNORE` failure paths (would fail CI)
- Unity Fixture (`TEST_CASE`, groups) when `CONFIG_UNITY_ENABLE_FIXTURE` is disabled

## Requirements

- **Hardware**: Any ESP32 variant
- **Wokwi/QEMU**: Supported
