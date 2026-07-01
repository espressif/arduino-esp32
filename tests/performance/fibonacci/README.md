# Fibonacci Benchmark

Computes Fibonacci(40) using naive recursive implementation (no memoization) to stress-test CPU integer performance. Measures wall-clock computation time averaged over 3 runs. The pytest harness independently computes the expected result and validates correctness.

## Benchmarks

| Metric | Unit |
|---|---|
| Average computation time | seconds |

## Requirements

- **Hardware**: Supported (hardware-only)
- **Wokwi**: Disabled (`wokwi: false`) — performance benchmarks require real hardware for meaningful results
- **QEMU**: Disabled (`qemu: false`)

## Notes

- The naive recursive algorithm has exponential time complexity, which is intentional — it exercises deep call stacks and integer arithmetic without relying on memory bandwidth.
- `FIB_N` should be kept between 35 and 45 to ensure reasonable execution times across targets.
- Results are written to a JSON report file by the pytest harness for CI performance tracking.
