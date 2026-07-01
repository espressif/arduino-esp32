# RAM Speed Benchmark

Benchmarks internal RAM `memcpy` and `memset` throughput using both the system implementation and a mock (manual loop) implementation. Based on the [NuttX ramspeed test](https://github.com/apache/nuttx-apps/blob/master/benchmarks/ramspeed/ramspeed_main.c). Tests power-of-2 block sizes from 32 bytes up to 64 KiB, averaged over multiple runs.

## Benchmarks

| Benchmark | Metric |
|---|---|
| System `memcpy()` | Throughput (KiB/s) per block size |
| Mock `memcpy()` | Throughput (KiB/s) per block size |
| System `memset()` | Throughput (KiB/s) per block size |
| Mock `memset()` | Throughput (KiB/s) per block size |

## Requirements

- **Hardware**: Supported (hardware-only)
- **Wokwi**: Disabled (`wokwi: false`) — performance benchmarks require real hardware for meaningful results
- **QEMU**: Disabled (`qemu: false`)
- **FQBN overrides**: ESP32, ESP32-S2, and ESP32-S3 are built with `PSRAM=disabled` to ensure allocations come from internal RAM

## Notes

- On SoCs that support per-address cache invalidation (cache line >= 16 bytes), the test invalidates the data cache before each timed region to measure actual RAM throughput rather than cache hits.
- Results are written to a JSON report file by the pytest harness for CI performance tracking.
