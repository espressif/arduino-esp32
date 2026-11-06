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
| `test_string_concat_cstr_inline_growth` | Own C-string append across inline-to-heap growth |
| `test_string_concat_cstr_inline_substring` | Interior C-string append across inline-to-heap growth |
| `test_string_concat_cstr_heap_growth` | Full self-append while growing a heap buffer |
| `test_string_concat_buffer_heap_substring` | Interior byte-pointer append with an explicit length and heap growth |
| `test_string_concat_cstr_reserved` | Reserved capacity, interior slices without growth, and small inline appends |
| `test_string_concat_self_overloads` | String-object self-concatenation and own C-string `+=` |
| `test_string_concat_external_and_empty` | External RAM/flash strings, empty appends, and null-source rejection |
| `test_string_concat_binary_alias` | Explicit-length self-append preserves embedded NUL bytes |
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

String concatenation regressions are defined in `string_tests.h` and run through
the existing `test_unity.py` driver, which is available in the trusted CI
base-branch checkout. No external components are required. Hardware heap growth
may extend an allocation in place; the same regressions can be run on a host
with a forced-moving allocator to exercise heap relocation deterministically.
