# DAC Loopback Validation Test

Multi-DUT test validating DAC output via ADC readback on a second device. Uses the `generic_multi_device` runner so that the sender's DAC output is physically wired to the reader's ADC input.

## Architecture

| Device | Sketch | Role |
|---|---|---|
| device0 (sender) | `sender/sender.ino` | Writes DAC values on `DAC1` via serial commands |
| device1 (reader) | `reader/reader.ino` | Reads ADC on the pin connected to sender's DAC output |

## Test Cases

| Step | Description |
|---|---|
| DAC_WRITE 0 + ADC_READ | Sender writes DAC=0, reader verifies ADC < 500 mV |
| DAC_WRITE 255 + ADC_READ | Sender writes DAC=255, reader verifies ADC > 2000 mV |
| DAC_DISABLE | Sender disables DAC output |

## Requirements

- **Hardware**: Two ESP32 devkits on a `generic_multi_device` runner
- **Targets**: ESP32 and ESP32-S2 only (`CONFIG_SOC_DAC_SUPPORTED=y`)
- **Wokwi**: Disabled — DAC is not simulated in Wokwi
- **QEMU**: Disabled
- **CI Runner**: `generic_multi_device`

## Harness Wiring

Connect the reader's ADC input to the sender's `DAC1` pin (same GPIO on both boards):

| SoC | Loopback wire |
|---|---|
| ESP32 | GPIO25 ↔ GPIO25 |
| ESP32-S2 | GPIO17 ↔ GPIO17 |

## Serial Protocol

After boot the sender prints `[SENDER] DAC_READY`. The Python orchestrator then sends:

- **Sender**: `DAC_WRITE <value>` / `DAC_DISABLE` / `DONE`
- **Reader**: `ADC_READ` / `DONE`
