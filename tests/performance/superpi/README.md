# Super PI Benchmark

Calculates pi to 16384 (2^14) decimal digits using FFT and AGM, measuring computation time. Based on ["Calculation of PI using FFT and AGM"](https://github.com/Fibonacci43/SuperPI) by T. Ooura. Averaged over 3 runs.

## Benchmarks

| Metric | Unit |
|---|---|
| Average computation time | seconds |

## Requirements

- **Hardware**: Supported (hardware-only)
- **Wokwi**: Disabled (`wokwi: false`) — performance benchmarks require real hardware for meaningful results
- **QEMU**: Disabled (`qemu: false`)

## Notes

- Results are written to a JSON report file by the pytest harness for CI performance tracking.
