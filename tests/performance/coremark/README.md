# CoreMark Benchmark

Runs the [CoreMark](https://github.com/PaulStoffregen/CoreMark) industry-standard CPU benchmark to produce a single-number performance score. Averaged over 3 runs. Reports the CoreMark 1.0 score, which measures overall CPU performance across list processing, matrix manipulation, and state machine workloads.

## Benchmarks

| Metric | Unit |
|---|---|
| Average CoreMark score | CoreMark 1.0 iterations/sec |

## Requirements

- **Hardware**: Supported (hardware-only)
- **Wokwi**: Disabled (`wokwi: false`) — performance benchmarks require real hardware for meaningful results
- **QEMU**: Disabled (`qemu: false`)

## Notes

- The task watchdog timer is reconfigured to a 20-second timeout to prevent resets during the CPU-intensive benchmark.
- The sketch provides a custom `ee_printf` implementation that routes CoreMark output through Arduino `Serial`.
- Results are written to a JSON report file by the pytest harness for CI performance tracking.
