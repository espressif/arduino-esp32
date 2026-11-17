# StreamString Benchmark

Measures the read throughput of `StreamString` on large buffers.

## Benchmarks

| Benchmark | Metric | Description |
|---|---|---|
| `read()` per byte | Average time (us) | Reads 4 KiB one byte at a time |
| `readBytes()` one shot | Average time (us) | Reads 16 KiB with a single `readBytes()` call |
| `readBytes()` chunked | Average time (us) | Reads 16 KiB in 1 KiB `readBytes()` calls |
| `readString()` | Average time (us) | Reads 4 KiB through `readString()` |
| `readBytesUntil()` | Average time (us) | Reads 16 KiB through `readBytesUntil()` up to a terminator |

Every run verifies the returned data against the expected pattern and that the stream is fully consumed, so the benchmark also checks functional correctness.

## Requirements

- **Hardware**: Supported (hardware-only)
- **Wokwi**: Disabled (`wokwi: false`) — performance benchmarks require real hardware for meaningful results
- **QEMU**: Disabled (`qemu: false`)

## Notes

- Timeouts are disabled for the `readString()` and `readBytesUntil()` benchmarks so the metrics reflect processing time instead of the idle stream timeout.
- Results are written to a JSON report file by the pytest harness for CI performance tracking.
