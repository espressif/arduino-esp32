# Filesystem Validation Test

Validates file system operations across SPIFFS, FFat, and LittleFS using a unified interface. Each test is run against all three filesystems.

## Test Cases

| Test Function | Description |
|---|---|
| `test_info_sanity` | Verify `totalBytes()` > 0 and `usedBytes()` <= `totalBytes()` |
| `test_basic_write_and_read` | Write "hello", read back, verify content and overwrite behavior |
| `test_append_behavior` | Append two lines, verify both are present, then remove |
| `test_dir_ops_and_list` | Create nested directories, write files, list and verify contents |
| `test_rename_and_remove` | Rename a file, verify old path gone and new path exists, then remove |
| `test_binary_write_and_seek` | Write 256 bytes, seek to offset 123, verify byte value |
| `test_binary_incremental_with_size_tracking` | Write/read 8 chunks of 64 bytes, verify position and size tracking |
| `test_empty_file_operations` | Create empty file, verify zero size, read returns EOF |
| `test_seek_edge_cases` | Seek to beginning, middle, end, and back; verify position and data |
| `test_file_truncation_and_overwrite` | Overwrite large file with small content, verify truncation |
| `test_multiple_file_handles` | Open two files simultaneously, write and read independently |
| `test_free_space_tracking` | Write 4KB file, verify space usage increases, remove and verify freed |
| `test_error_cases` | Open/remove/rename nonexistent paths return false (skipped on SPIFFS) |
| `test_large_file_operations` | Write and read back a 10KB file in 256-byte chunks |
| `test_write_read_patterns` | Sequential write, random-access read from various positions |
| `test_directory_operations_edge_cases` | Duplicate mkdir, nested mkdir without parent, rmdir nonexistent |
| `test_max_open_files_limit` | Open files up to the max limit, verify next open fails (skipped on LittleFS) |
| `test_open_read_mode_type_detection` | Regression test for TOCTOU fix: read-mode open detects files vs directories |

## Requirements

- **Hardware**: Any ESP32 variant
- **Wokwi**: Supported (diagram.json provided)
- **QEMU**: Not supported
