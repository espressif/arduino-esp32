# PSRAM Speed Benchmark

Benchmarks PSRAM (SPIRAM) `memcpy` and `memset` throughput using both the system implementation and a mock (manual loop) implementation. Based on the [NuttX ramspeed test](https://github.com/apache/nuttx-apps/blob/master/benchmarks/ramspeed/ramspeed_main.c). Tests power-of-2 block sizes from 64 KiB up to 512 KiB, averaged over multiple runs.

## Benchmarks

| Benchmark | Metric |
|---|---|
| System `memcpy()` | Throughput (KiB/s) per block size |
| Mock `memcpy()` | Throughput (KiB/s) per block size |
| System `memset()` | Throughput (KiB/s) per block size |
| Mock `memset()` | Throughput (KiB/s) per block size |

## Requirements

- **Hardware**: Supported (hardware-only) — requires boards with PSRAM
- **Wokwi**: Disabled (`wokwi: false`) — performance benchmarks require real hardware for meaningful results
- **QEMU**: Disabled (`qemu: false`)
- **Requires**: `CONFIG_SPIRAM=y`
- **CI Runner Tags**: `psram` (ESP32, ESP32-S2, ESP32-C5), `octal_psram` (ESP32-S3)

## Notes

- All allocations use `MALLOC_CAP_SPIRAM` to ensure buffers reside in PSRAM, not internal RAM.
- On SoCs that support per-address cache invalidation (cache line >= 16 bytes), the test invalidates the data cache before each timed region to measure actual PSRAM throughput rather than cache hits.
- The start size is 64 KiB (vs 32 bytes in ramspeed) to avoid cache effects dominating results on PSRAM.
- Results are written to a JSON report file by the pytest harness for CI performance tracking.
