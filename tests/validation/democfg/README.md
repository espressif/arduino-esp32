# Demo Configuration Validation Test

Demonstrates `ci.yml` configuration options for the CI system. The sketch itself is minimal (prints "Hello cfg!") — this test exists as a reference example for CI configuration features.

## Test Cases

| Test Function | Description |
|---|---|
| N/A | Prints "Hello cfg!" — pytest verifies the message is received |

## Requirements

- **Hardware**: Supported
- **Wokwi**: Not supported
- **QEMU**: Not supported
- **SoC Config**: `CONFIG_SOC_UART_SUPPORTED=y`

## Notes

- The `ci.yml` in this directory is a fully annotated example showing all available CI configuration options:
  - `fqbn_append` — appended to default FQBNs (e.g., `DebugLevel=verbose`)
  - `fqbn` — per-target FQBN overrides with custom PSRAM/partition/flash settings
  - `platforms` — enable/disable hardware, Wokwi, and QEMU platforms
  - `requires` / `requires_any` — sdkconfig-based eligibility filters (AND/OR logic)
  - `targets` — per-SoC enable/disable overrides
  - `tags` / `soc_tags` — hardware CI runner tag selection
- Specific targets are disabled: esp32c3, esp32h2, esp32p4.
- Custom FQBNs are defined for esp32 (two flash modes), esp32s2 (PSRAM), and esp32s3 (OPI PSRAM).
