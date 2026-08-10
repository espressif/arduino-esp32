# ADC / PWM Validation Test

Single-device Unity test for ADC, LEDC (PWM), and SigmaDelta peripherals.

## Test Cases

| Test Function | Description |
|---|---|
| `test_adc_read_basic` | `analogRead` within 0–4095 |
| `test_adc_resolution` | `analogReadResolution` at 10-bit and 12-bit |
| `test_adc_millivolts` | `analogReadMilliVolts` within 0–3300 |
| `test_adc_attenuation` | `analogSetAttenuation` ADC_0db / ADC_11db |
| `test_adc_continuous` | `analogContinuous` DMA mode with ISR completion |
| `test_ledc_attach_detach` | `ledcAttach` / `ledcDetach` |
| `test_ledc_write_duty` | `ledcWrite` at 0 / 128 / 255 |
| `test_ledc_read_freq` | `ledcReadFreq` matches set frequency |
| `test_ledc_change_frequency` | `ledcChangeFrequency` to 10 kHz |
| `test_ledc_fade` | `ledcFade` 0 to 255 over 500 ms |
| `test_ledc_tone` | `ledcWriteTone` at 440 Hz |
| `test_sigmadelta_attach_detach` | SigmaDelta attach / write / detach (`SOC_SDM_SUPPORTED` only) |

## Requirements

- **Hardware**: Single ESP32 devkit
- **Wokwi**: Disabled — SigmaDelta is not simulated in Wokwi
- **QEMU**: Disabled

## Pin Configuration

Per-target GPIO numbers are in `pins_config.h`. They follow [CI_README — DevKit GPIO Reference](../../../.github/CI_README.md#devkit-gpio-reference) and [Peripheral API constraints](../../../.github/CI_README.md#peripheral-api-constraints).

| SoC | ADC | LEDC | SigmaDelta |
|---|---|---|---|
| ESP32 | `A4` (GPIO32, ADC1) | GPIO16 | GPIO13 |
| ESP32-S2 | `A0` (GPIO1) | GPIO4 | GPIO5 |
| ESP32-S3 | `A0` (GPIO1) | GPIO4 | GPIO5 |
| ESP32-C3 | `A0` (GPIO0) | GPIO3 | GPIO10 |
| ESP32-C5 | `A0` (GPIO1) | GPIO4 | GPIO9 |
| ESP32-C6 | `A0` (GPIO0) | GPIO2 | GPIO3 |
| ESP32-H2 | `A0` (GPIO1) | GPIO4 | GPIO5 |
| ESP32-P4 | `A4` (GPIO20) | GPIO6 | GPIO7 |

## Notes

- SigmaDelta tests only compile on chips with `SOC_SDM_SUPPORTED`.
- DAC loopback is covered by the separate `dac` multi-DUT test.
