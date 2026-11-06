# LINPACK Float Benchmark

Runs the LINPACK benchmark using single-precision (`float`) arithmetic to measure floating-point performance in MFLOPS. Based on [EmbeddedLinpack](https://github.com/VioletGiraffe/EmbeddedLinpack). Solves an 8x8 linear system via LU factorization (DGEFA + DGESL) over 1000 runs and reports min/max/average/median MFLOPS.

## Benchmarks

| Metric | Unit |
|---|---|
| Average MFLOPS | MFLOPS |
| Min MFLOPS | MFLOPS |
| Max MFLOPS | MFLOPS |
| Median MFLOPS | MFLOPS |

## Requirements

- **Hardware**: Supported (hardware-only)
- **Wokwi**: Disabled (`wokwi: false`) — performance benchmarks require real hardware for meaningful results
- **QEMU**: Disabled (`qemu: false`)

## Notes

- The pytest harness validates that the data type is `float` and that all MFLOPS values are positive.
- Results are written to a JSON report file for CI performance tracking.
