# Filesystem Validation Test Suite

This test suite validates the functionality of different filesystem implementations available for ESP32: SPIFFS, FFat, and LittleFS.

## Overview

The test suite uses Unity testing framework to run a comprehensive set of filesystem operations across all three filesystem implementations. Each test is executed for each filesystem to ensure consistent behavior and identify filesystem-specific differences.

## Tested Filesystems

- **SPIFFS** (`/spiffs`) - SPI Flash File System
- **FFat** (`/ffat`) - FAT filesystem implementation
- **LittleFS** (`/littlefs`) - Little File System

## Test Cases

The suite includes the following test categories:

- **Basic Operations**: File write, read, append
- **Directory Operations**: Create directories, list files, nested directories
- **File Management**: Rename, remove, exists checks
- **Binary Operations**: Binary data write/read, seek operations
- **Edge Cases**: Empty files, seek edge cases, file truncation
- **Multiple Handles**: Concurrent file operations
- **Space Tracking**: Free space monitoring
- **Error Handling**: Non-existent file operations
- **Large Files**: Operations with larger file sizes
- **Write/Read Patterns**: Sequential and random access patterns
- **Open Files Limit**: Maximum concurrent open files testing

## Known Filesystem-Specific Behaviors

### SPIFFS

- **Directory Support**: SPIFFS does not have true directory support. Opening a directory always returns `true`, and closing it also always returns `true`, regardless of whether the directory actually exists or not. This is a limitation of the SPIFFS implementation.
- **Error Handling**: Some error case tests are skipped for SPIFFS due to different error handling behavior compared to other filesystems.

### LittleFS

- **Open Files Limit**: LittleFS does not enforce a maximum open files limit at the same time. The `test_max_open_files_limit()` test is skipped for LittleFS as it doesn't have this constraint.

### FFat

- FFat follows standard FAT filesystem behavior and supports all tested operations including proper directory handling and open files limits.
