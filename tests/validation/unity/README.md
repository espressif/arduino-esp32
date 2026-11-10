# Unity Framework Validation Test

Comprehensive smoke test that exercises the Unity assertion and control macros available in the ESP32 Arduino core build.

Also runs signed integer conversion regressions from `stdlib_tests.h`. Keeping
them in this existing suite lets the trusted base-branch runtime driver discover
and execute them for pull requests.

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
| `test_uri_braces_suffixed_argument` | Valid filename capture before `.json` |
| `test_uri_braces_suffix_rejects_nested_paths` | Nested and double-slash paths rejected before a suffix |
| `test_uri_braces_multiple_arguments` | Valid captures in separate segments; nested final capture rejected |
| `test_uri_braces_arguments_in_one_segment` | Multiple captures before suffixes; slash rejected in either capture |
| `test_uri_braces_trailing_argument` | Existing trailing-placeholder behavior preserved |
| `test_uri_braces_rejects_mismatched_literals` | Wrong extension, prefix, missing suffix, and extra trailing path rejected |
| `test_uri_braces_empty_argument` | Existing empty-capture behavior preserved |
| `test_uri_braces_valid_after_rejection` | Valid capture succeeds after a rejected nested path |
| `test_hooks` | `setUp`/`tearDown` invocation and Unity version |
| `test_pass_early` | `TEST_PASS_MESSAGE` early exit |
| `test_ignore` | `TEST_IGNORE_MESSAGE` (counted as ignored, not failed) |
| `test_ltoa_signed_limits` | 32-bit minimum, adjacent value, maximum, zero, and known binary/decimal/hex output |
| `test_lltoa_signed_limits` | 64-bit minimum, adjacent value, maximum, zero, and known binary/decimal/hex output |
| `test_signed_conversion_all_radices` | Boundary and ordinary values round-trip in every supported base (2 through 16) |
| `test_signed_conversion_invalid_radices` | Unsupported bases return an empty string in the supplied buffer |
| `test_string_signed_minimum_values` | Signed-minimum String construction, hexadecimal construction, `concat`, and `+=` |

## Not covered

- 64-bit integer macros when `CONFIG_UNITY_ENABLE_64BIT` is disabled (default)
- `TEST_FAIL` / `TEST_IGNORE` failure paths (would fail CI)
- Unity Fixture (`TEST_CASE`, groups) when `CONFIG_UNITY_ENABLE_FIXTURE` is disabled

## Requirements

URI matching regressions are defined in `uri_braces_tests.h` and exercise the
matcher directly without a network connection. They run through the existing
`test_unity.py` driver available in the trusted CI base-branch checkout.

- **Hardware**: Any ESP32 variant
- **Wokwi/QEMU**: Supported

String concatenation regressions are defined in `string_tests.h` and run through
the existing `test_unity.py` driver, which is available in the trusted CI
base-branch checkout. No external components are required. Hardware heap growth
may extend an allocation in place; the same regressions can be run on a host
with a forced-moving allocator to exercise heap relocation deterministically.
