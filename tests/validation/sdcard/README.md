# SD Card Validation Test

Validates SD card operations over SPI, including file I/O, directory management, and multi-bus support. Tests run on each available SPI bus (FSPI and HSPI where supported) using Wokwi-simulated SD cards.

## Test Cases

| Test Function | Description |
|---|---|
| `test_nonexistent_spi_interface` | Creating SPIClass with invalid SPI number fails gracefully |
| `test_sd_basic` | Mount SD, write/read file, verify content, check total/used bytes |
| `test_sd_dir` | Create and remove a directory, verify existence |
| `test_sd_file_operations` | Write and read back a file, verify content matches |
| `test_sd_open_limit` | Open files up to `max_files=2` limit, verify third open fails |
| `test_sd_directory_listing` | Create directory tree with files and subdirectory, list and count entries |
| `test_sd_file_size_operations` | Verify file size, `available()`, `position()`, and `seek()` |
| `test_sd_nested_directories` | Create 3-level nested directories, write/read files at each level |
| `test_sd_file_count_in_directory` | Create 5 files in nested directory, list and verify all found |
| `test_sd_file_append_operations` | Write, append two lines, verify all three lines present |
| `test_sd_large_file_operations` | Write and read 5KB file in 512-byte chunks, verify data integrity |

## Requirements

- **Hardware**: Not supported (no physical SD card wiring in CI)
- **Wokwi**: Supported (per-SoC diagram files with simulated SD cards)
- **QEMU**: Not supported

## Notes

- Tests run on both FSPI and HSPI buses when the SoC supports multiple SPI peripherals.
- Each test is executed with two initialization strategies: all buses mounted first then tested, and mount-test-unmount per bus sequentially.
