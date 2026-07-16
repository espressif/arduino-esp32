# Keyboard Layout Validation Test

Validates structural properties of all USB HID keyboard layout tables (12 layouts). Tests verify control character mappings, modifier flag constraints, scancode ranges, letter case consistency, and printable character coverage.

## Test Cases

| Test Function | Description |
|---|---|
| `test_unused_control_chars` | NUL–BEL, VT, FF, CR, SO–US map to 0x00 in all layouts |
| `test_standard_control_keys` | BS→0x2A, TAB→0x2B, LF→0x28 in all layouts |
| `test_space_mapping` | Space maps to USB HID scancode 0x2C in all layouts |
| `test_no_combined_shift_altgr` | No entry has both SHIFT and ALT_GR flags set |
| `test_valid_scancode_range` | Non-zero entries have base scancode in [0x04, 0x38] |
| `test_lowercase_letters` | All 26 lowercase letters are mapped without modifiers |
| `test_uppercase_letters` | All 26 uppercase letters have SHIFT modifier only |
| `test_letter_case_consistency` | Upper and lowercase letters share the same base scancode |
| `test_digits_mapped` | All 10 digits are mapped (non-zero) |
| `test_unique_letter_scancodes` | Each layout uses unique scancodes for distinct letters |

## Requirements

- **Hardware**: Any ESP32 variant with USB OTG support
- **Wokwi**: Supported
- **QEMU**: Not supported
- **SoC Config**: `CONFIG_SOC_USB_OTG_SUPPORTED=y`

## Notes

- Tested layouts: da_DK, de_DE, en_US, es_ES, fr_CH, fr_FR, hu_HU, it_IT, ja_JP, pt_BR, pt_PT, sv_SE.
- To add a new layout, add an entry to the `layouts[]` array — all tests run automatically against it.
