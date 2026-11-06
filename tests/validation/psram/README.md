# PSRAM Validation Test

Validates external PSRAM (SPI RAM) functionality: detection, allocation APIs (malloc, calloc, realloc), failure handling, heap_caps integration, large allocations, memory pattern verification (memset/memcpy), and concurrent access from multiple tasks.

## Test Cases

| Test Function | Description |
|---|---|
| `psram_found` | Verify PSRAM is detected and size > 0 |
| `test_malloc_success` | `ps_malloc` 512 kB succeeds |
| `test_malloc_fail` | `ps_malloc` with impossibly large size returns NULL |
| `test_calloc_success` | `ps_calloc` 512 kB succeeds |
| `test_realloc_success` | `ps_realloc` from 512 kB to 513 kB succeeds |
| `test_heap_caps_spiram` | Verify `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` > 0 |
| `test_heap_caps_alloc` | Allocate via `heap_caps_malloc`, verify pointer is in external RAM |
| `test_large_allocation` | Allocate up to 2 MB, write first/last pages, verify |
| `test_memset_all_zeroes` | Fill 512 kB with 0x00, verify every byte |
| `test_memset_all_ones` | Fill 512 kB with 0xFF, verify every byte |
| `test_memset_alternating` | Fill with 0xAA pattern, verify |
| `test_memset_random` | Overwrite random data with 0x55, verify |
| `test_memcpy` | Copy 1 kB pattern across 512 kB buffer, verify with memcmp |
| `test_concurrent_access` | Two tasks read/write shared PSRAM buffer simultaneously |

## Requirements

- **Hardware**: ESP32 variant with PSRAM (ESP32-WROVER, ESP32-S2, ESP32-S3, ESP32-P4)
- **Wokwi**: Supported
- **CI Runner**: `psram` or `octal_psram` (target-dependent)

## Notes

- If PSRAM is not found, only `psram_found` runs (fails) and remaining tests are skipped.
- `test_memset_random`, `test_memcpy`, and `test_concurrent_access` are skipped on ESP32-P4 in Wokwi (simulation timeout).
- `test_heap_caps_alloc` uses `esp_ptr_external_ram()` to verify the pointer is truly in SPIRAM.
