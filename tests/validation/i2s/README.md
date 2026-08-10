# I2S Validation Test

Multi-DUT test validating the Arduino `ESP_I2S` library in STD mode. Uses the `generic_multi_device` runner so that the sender's DOUT is physically wired to the receiver's DIN for real TX-to-RX loopback verification.

## Architecture

| Device | Sketch | Role |
|---|---|---|
| device0 (sender) | `sender/sender.ino` | Runs single-device Unity tests, then transmits known data via I2S TX |
| device1 (receiver) | `receiver/receiver.ino` | Receives I2S data, verifies ramp pattern, reports match results |

## Test Cases

### Single-device tests (sender, Unity)

| Test Function | Description |
|---|---|
| `test_i2s_begin_end` | I2S STD mode begin and end |
| `test_i2s_sample_rate_8k` | Write samples at 8 kHz sample rate |
| `test_i2s_sample_rate_44k` | Write samples at 44.1 kHz sample rate |
| `test_i2s_sample_rate_48k` | Write samples at 48 kHz sample rate |
| `test_i2s_32bit` | Write samples at 32-bit depth |
| `test_i2s_8bit` | Write samples at 8-bit depth |
| `test_i2s_mono` | Write samples in mono slot mode |
| `test_i2s_reopen` | End + begin with different params to verify resource release |
| `test_i2s_slave_mode_begin_end` | `begin()` accepts `I2S_ROLE_SLAVE` without error |
| `test_i2s_mono_aligned_write` | 16-bit mono `write()` returns full buffer size; `lastError()` is 0 (covers HW v1 workaround path) |
| `test_i2s_mono_subbyte_write_no_error` | Sub-sample `write(uint8_t)` returns 0 without setting an error (fix for HW v1 workaround path) |
| `test_i2s_configure_tx_after_mono_begin` | `configureTX()` re-applies HW v1 workaround; subsequent `write()` returns full size |
| `test_i2s_configure_rx_after_mono_begin` | `configureRX()` re-applies HW v1 workaround on rate change; returns true |
| `test_i2s_configure_rx_slot_mask` | `configureRX()` with explicit slot_mask LEFT → RIGHT at 16-bit; returns true |
| `test_i2s_configure_rx_slot_mask_32bit` | `configureRX()` with explicit slot_mask LEFT → RIGHT at 32-bit (no workaround); returns true |
| `test_i2s_mono_left_slot_write` | Mono with explicit `I2S_STD_SLOT_LEFT`; `write()` succeeds, no error |
| `test_i2s_mono_right_slot_write` | Mono with explicit `I2S_STD_SLOT_RIGHT`; `write()` succeeds, no error |
| `test_i2s_mono_both_slot_write` | Mono with explicit `I2S_STD_SLOT_BOTH`; `write()` succeeds, no error |
| `test_i2s_stereo_left_slot_write` | Stereo with explicit `I2S_STD_SLOT_LEFT`; `write()` succeeds, no error |
| `test_i2s_stereo_right_slot_write` | Stereo with explicit `I2S_STD_SLOT_RIGHT`; `write()` succeeds, no error |
| `test_i2s_stereo_frame_write` | 16-bit stereo single `int16_t[2]` frame write returns 4 bytes (Simple_tone pattern; regression #12668) |
| `test_i2s_stereo_tone_loop_frames` | Multiple stereo frames at 8 kHz like the Simple_tone loop |
| `test_i2s_stereo_legacy_byte_pattern_ignored` | Pre-3.3.9 four-byte `write(uint8_t)` pattern sends nothing for 16-bit stereo |
| `test_i2s_stereo_subbyte_write_no_error` | Sub-sample `write(uint8_t)` in stereo returns 0 without error |
| `test_i2s_stereo_subsample_buffer_rejected` | 1-byte buffer write in 16-bit stereo returns 0 without error |
| `test_i2s_stereo_frame_write_after_legacy_pattern` | Sample-aligned frame write succeeds after ignored byte writes |
| `test_i2s_mono_32bit_write` | 32-bit mono (HW v1 workaround must NOT activate); write succeeds |
| `test_i2s_mono_right_subbyte_write_no_error` | Sub-sample `write(uint8_t)` with RIGHT slot returns 0 without error |
| `test_i2s_configure_tx_stereo_to_mono` | `configureTX()` switches stereo → mono; subsequent `write()` works correctly |
| `test_i2s_configure_tx_mono_to_stereo` | `configureTX()` switches mono → stereo; subsequent `write()` works correctly |
| `test_i2s_configure_tx_mono_left_to_right` | `configureTX()` switches mono LEFT → mono RIGHT slot; both writes succeed |
| `test_i2s_stereo_rx_left_slot` | Stereo RX with explicit `I2S_STD_SLOT_LEFT`; `begin()` succeeds |
| `test_i2s_stereo_rx_right_slot` | Stereo RX with explicit `I2S_STD_SLOT_RIGHT`; `begin()` succeeds |
| `test_i2s_configure_rx_stereo_slot_change` | `configureRX()` cycles through stereo LEFT → RIGHT → BOTH; all succeed |
| `test_i2s_configure_rx_mono_both_slot` | `configureRX()` with mono BOTH slot_mask; succeeds (valid use case) |
| `test_i2s_configure_rx_default_slot_preserves` | `configureRX()` with default slot_mask preserves previously-set RIGHT slot across rate changes |
| `test_i2s_configure_rx_stereo_to_mono_resets_slot` | `configureRX()` stereo→mono with default slot_mask resets to LEFT |
| `test_i2s_configure_rx_reject_wrong_transform_width` | `configureRX()` rejects `I2S_RX_TRANSFORM_16_STEREO_TO_MONO` at 32-bit (bit-width validation) |
| `test_i2s_tdm_begin_end` ¹ | TDM mode begin and end (stereo, SLOT0+SLOT1) |
| `test_i2s_tdm_stereo_write` ¹ | TDM stereo write (SLOT0+SLOT1) |
| `test_i2s_tdm_mono_write` ¹ | TDM mono write (SLOT0); verifies byte count and no error |
| `test_i2s_tdm_32bit_write` ¹ | TDM 32-bit stereo write (SLOT0+SLOT1) |
| `test_i2s_tdm_slave_begin_end` ¹ | TDM `begin()` with `I2S_ROLE_SLAVE` accepted without error |

¹ Compiled and run only on targets where `SOC_I2S_SUPPORTS_TDM=1` (ESP32-S3, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-H2, ESP32-P4).

### Cross-device loopback (serial-orchestrated)

Each loopback phase reports four metrics for data-integrity verification:

- **`matches`**: Best sliding-window match out of 64 samples (how many consecutive ramp values aligned).
- **`full_ramps`**: Number of positions where ALL 64 samples match the ramp pattern perfectly — catches subtle corruption.
- **`zeros`**: Number of zero-valued samples in the receive buffer — detects wrong-channel extraction (e.g., reading the left channel when data is on the right).
- **`total`**: Total samples (or frames for stereo) received.

| Phase | Config | What it validates |
|---|---|---|
| 1 | mono, 16 kHz, 16-bit, default slot | Basic mono data integrity |
| 2 | stereo TX → mono RX (right-slot), 16 kHz, 16-bit | `i2s_channel_read_16_stereo_to_mono_right` (HW v1) / native slot_mask |
| 3 | stereo TX → mono RX (left-slot), 16 kHz, 16-bit | `i2s_channel_read_16_stereo_to_mono_left` (HW v1) / native slot_mask |
| 4 | mono RIGHT TX → RIGHT RX, 16 kHz, 16-bit | TX-side slot_mask correctness |
| 5–16 | rate × width × mode bounds matrix (see below) | Data integrity at all boundary combinations |
| 17–20 | mono slot_mask matrix (see below) | Explicit LEFT, RIGHT slot selection with data verification |
| 21–22 | stereo slot_mask TX (see below) | Stereo + LEFT/RIGHT TX routing with data verification |
| 23–24 | cross-slot TX=BOTH → RX=LEFT/RIGHT (see below) | Independent TX/RX slot_mask with data verification |
| 25–27 | `configureRX` slot change (see below) | Runtime slot reconfiguration via `configureRX(... slot_mask)` |
| 28 | `configureRX` default slot preservation (see below) | Default `slot_mask` preserves current slot across rate changes |
| 29–30 ² | HW v1 BOTH averaging (see below) | `_both` function selection in workaround path (ESP32 only) |
| 31 ¹ | TDM mono, SLOT0, 16 kHz, 16-bit | TDM mode data integrity (skipped on non-TDM targets) |

#### Phases 5–16: bounds matrix

The parameterized `TX <rate> <bits> <ch>` / `RX <rate> <bits> <ch>` commands loop over all combinations of lower/upper sample rates and bit widths with mono and stereo modes:

| Rates | Widths | Modes | Total |
|---|---|---|---|
| 8 kHz, 96 kHz | 8-bit, 16-bit, 32-bit | mono, stereo | 2 × 3 × 2 = 12 phases |

8-bit on ESP32 (HW v1) is now supported: the Arduino library transparently handles the `uint16_t` high-byte packing required by the hardware, plus the stereo workaround for mono 8-bit. See the [IDF I2S STD TX Mode documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html#std-tx-mode) for background.

Note: 24-bit is excluded because the IDF requires `mclk_multiple` to be a multiple of 3 for 24-bit data, and the Arduino `ESP_I2S` library does not currently expose this setting.

Each phase verifies data integrity with the same sliding-window ramp verification used in Phases 1–4. The repeat count adapts to the sample rate to ensure at least 1 second of data flows on the wire.

#### Phases 17–20: slot_mask matrix

Tests mono mode with each explicit slot_mask at two bit widths:

| Widths | Slots | Total |
|---|---|---|
| 16-bit, 32-bit | LEFT, RIGHT | 2 × 2 = 4 phases |

All at 16 kHz. The 16-bit phases exercise the ESP32 HW v1 workaround path with each slot; the 32-bit phases verify slot selection without the workaround. BOTH is excluded because on non-HW v1 targets it duplicates samples in the DMA buffer, scrambling the ramp pattern; `begin()` with BOTH is covered by single-device tests.

#### Phases 21–22: stereo slot_mask TX

Tests stereo mode with explicit LEFT/RIGHT slot_mask on the TX side. When stereo + LEFT is configured, L data from the buffer is transmitted on the LEFT wire slot (RIGHT is muted). When stereo + RIGHT, R data goes on the RIGHT wire slot (LEFT is muted). The receiver listens on the matching slot.

| Phase | TX mode | TX slot | RX mode | RX slot | Ramp verified |
|---|---|---|---|---|---|
| 21 | stereo | LEFT (1) | mono | LEFT (1) | L ramp (`j × 100`) |
| 22 | stereo | RIGHT (2) | mono | RIGHT (2) | R ramp (`10000 + j × 100`) |

Both at 16 kHz 16-bit. The receiver uses `ramp_type=1` for Phase 22 to verify against the R ramp pattern.

#### Phases 23–24: cross-slot TX/RX

Verifies that TX and RX can use different slot_mask values independently:

| Phase | TX slot | RX slot | What it validates |
|---|---|---|---|
| 23 | BOTH (3) | LEFT (1) | RX extracts left channel from TX broadcasting on both |
| 24 | BOTH (3) | RIGHT (2) | RX extracts right channel from TX broadcasting on both |

Both at mono 16 kHz 16-bit. This tests the real use case of a mono source sending to both speaker channels while a receiver selectively listens to one.

#### Phases 25–27: `configureRX` slot change

Tests the `configureRX(rate, bits, ch, transform, slot_mask)` API by starting with one slot and reconfiguring to another before reading data:

| Phase | Initial slot | Reconfigured slot | Bit width | What it validates |
|---|---|---|---|---|
| 25 | LEFT (1) | RIGHT (2) | 16-bit | `configureRX` slot change with HW v1 workaround path |
| 26 | RIGHT (2) | LEFT (1) | 16-bit | Reverse slot change with HW v1 workaround path |
| 27 | LEFT (1) | RIGHT (2) | 32-bit | `configureRX` slot change without workaround |

All at mono 16 kHz. TX sends on the target (reconfigured) slot so the receiver should see valid data only after the `configureRX` call.

#### Phase 28: `configureRX` default slot preservation

Regression test for a bug where `configureRX()` with `slot_mask = -1` (default) would reset `_rx_slot_mask` to LEFT instead of preserving the current slot setting. The receiver begins with explicit RIGHT, then calls `configureRX` twice with rate changes but default `slot_mask`, and verifies the R ramp is still received.

| Phase | RX setup | What it catches |
|---|---|---|
| 28 | `begin(MONO, RIGHT)` → `configureRX(8kHz)` → `configureRX(16kHz)` | Bug: default `slot_mask` reset `_rx_slot_mask` to LEFT |

At 16 kHz 16-bit, paired with mono RIGHT TX. Failure would show zeros or wrong-channel data.

#### Phases 29–30: HW v1 BOTH averaging (ESP32 only)

Regression tests for bugs where the HW v1 workaround path selected `_left` extraction instead of `_both` averaging when `slot_mask = BOTH`. TX sends stereo data with distinct L and R ramps; the receiver verifies the output matches `(L + R) / 2`, not just L.

| Phase | RX setup | What it catches |
|---|---|---|
| 29 | `begin(MONO, BOTH, 16-bit)` | Bug in `initSTD()`: workaround used `_left` instead of `_both` |
| 30 | `begin(MONO, LEFT)` then `configureRX(..., BOTH)` | Bug in `configureRX()`: slot-only change to BOTH used `_left` |

Both at 16 kHz 16-bit. Skipped on non-HW v1 targets where mono + BOTH causes sample duplication instead of averaging.

² Compiled and run only on targets where `SOC_I2S_HW_VERSION_1=1` (ESP32).

#### `slot_mode × slot_mask` coverage summary

All 6 TX and all 6 RX combinations from the I2S slot configuration table are covered:

**TX side** (what goes on the wire):

| slot_mode | slot_mask | Covered by | Verification |
|---|---|---|---|
| mono | LEFT | Phase 1, 17, 19 | L ramp data integrity |
| mono | RIGHT | Phase 4, 18, 20 | L ramp data integrity |
| mono | BOTH | Phase 23–24 | L ramp via LEFT/RIGHT RX extraction |
| stereo | LEFT | Phase 21 | L ramp via mono RX |
| stereo | RIGHT | Phase 22 | R ramp via mono RX (`ramp_type=1`) |
| stereo | BOTH | Phase 2–3, 5–16 | Stereo ramp (L+R) data integrity |

**RX side** (what the buffer receives):

| slot_mode | slot_mask | Covered by | Verification |
|---|---|---|---|
| mono | LEFT | Phase 1, 3, 17, 19 | L ramp data integrity |
| mono | RIGHT | Phase 2, 4, 18, 20 | L/R ramp data integrity |
| mono | BOTH | Phase 29–30 ² + single-device | Averaged ramp on HW v1; `begin()` succeeds on all targets |
| stereo | LEFT/RIGHT/BOTH | Phase 5–16 | Library forces BOTH ³; stereo ramp (L+R) data integrity |

² On HW v1, mono + BOTH activates the workaround which averages L+R — verified by phases 29–30. On non-HW v1, mono + BOTH causes the IDF to duplicate samples in the DMA buffer, making ramp verification impossible; the single-device test `test_i2s_configure_rx_mono_both_slot` confirms `configureRX` with BOTH succeeds on all targets.

³ The IDF does not reliably ignore `slot_mask` in stereo RX mode (despite the docs claiming "any"). On some targets, LEFT/RIGHT causes channel duplication instead of normal interleaved data. The library forces `slot_mask = BOTH` for stereo RX to guarantee correct interleaved `[L, R]` output on all targets. Single-device tests (`test_i2s_stereo_rx_left_slot`, `test_i2s_stereo_rx_right_slot`) confirm `begin()` succeeds (the user's value is accepted but overridden internally).

## Data Integrity Verification

The Python test asserts three conditions for every loopback phase:

1. **`matches >= 48`** — at least 48 out of 64 consecutive ramp samples aligned in the best window.
2. **`full_ramps >= 1`** — at least one position with a perfect 64/64 match. Catches bit-level corruption that partial matching would miss.
3. **`zeros <= 50%`** — fewer than half the samples are zero. If the receiver extracts the wrong channel (e.g., left instead of right), the buffer would be predominantly zeros. This catches wrong-channel bugs that ramp matching alone would not detect.

## Requirements

- **Hardware**: Two ESP32 devkits connected pin-to-pin on the `generic_multi_device` runner
- **Wokwi**: Disabled — I2S is only partially simulated
- **QEMU**: Disabled
- **CI Runner**: `generic_multi_device`
- **SoC Config**: `CONFIG_SOC_I2S_SUPPORTED=y`

## Pin Configuration

Per-target GPIO numbers are in `pins_config.h`. They follow [CI_README — DevKit GPIO Reference](../../../.github/CI_README.md#devkit-gpio-reference): strapping and other never-safe pins are avoided; default I2C/SPI/Touch pins may be used because this sketch only drives I2S (no `Wire`, `SPI`, or `touchRead`).

Both devices use the same GPIO numbers. On the `generic_multi_device` runner, wire BCLK↔BCLK, WS↔WS, and DOUT↔DIN.

| SoC | BCLK | WS | DOUT / DIN |
|---|---|---|---|
| ESP32 | GPIO4 | GPIO13 | GPIO18 |
| ESP32-S2 | GPIO4 | GPIO5 | GPIO19 |
| ESP32-S3 | GPIO4 | GPIO5 | GPIO21 |
| ESP32-C3 | GPIO4 | GPIO5 | GPIO6 |
| ESP32-C5 | GPIO1 | GPIO9 | GPIO10 |
| ESP32-C6 | GPIO6 | GPIO7 | GPIO10 |
| ESP32-H2 | GPIO4 | GPIO5 | GPIO10 |
| ESP32-P4 | GPIO20 | GPIO21 | GPIO22 |

## Serial Protocol

After Unity tests, the sender prints `[SENDER] TX_READY`. The Python orchestrator then coordinates loopback phases. In all phases, the master (sender) must start **before** the slave (receiver). After any `end()`, the BCLK pin can retain residual state that causes the slave to clock in zeros instead of blocking for the real master clock.

**Receiver output format** (all phases):

```
[RECEIVER] <TAG> matches=<N> full_ramps=<N> zeros=<N> total=<N>
```

## Ramp Patterns

Phases 1–4 use the mono 16-bit ramp (`j × 100`). Phases 5–30 use the generic parameterized ramp:

| Width | Mono L (or mono) | Stereo R | Sample type |
|---|---|---|---|
| 8-bit | `j × 2` (0, 2, 4, …, 126) | `j × 2 + 1` (1, 3, 5, …, 127) | `int8_t` |
| 16-bit | `j × 100` (0, 100, …, 6300) | `10000 + j × 100` (10000, 10100, …, 16300) | `int16_t` |
| 32-bit | `j × 100000` (0, …, 6300000) | `5000000 + j × 100000` (5000000, …, 11300000) | `int32_t` |

All ramps are 64 frames long. The sender adapts the repeat count to the sample rate (at least `rate / 64 + 32` writes, minimum 128) to ensure ~1 second of data on the wire.

## Notes

- **Physical wiring is mandatory** for the loopback phases (see pin table above). Without BCLK/WS/DOUT↔DIN and common GND, the receiver reads only zeros.
- The IDF I2S TX DMA has 6 × 240 = 1440 zero-frames in its auto-clear pipeline after `begin()`. The sender pre-fills the DMA by writing 32 ramp repetitions **before** signaling `TX_CLOCKING`, ensuring the bus carries real data when the slave starts reading.
- For slot-specific phases (1–4), the sender repeats the ramp 128–256 times. For parameterized phases (5–30), the repeat count adapts to the sample rate (`rate/64 + 32`, minimum 128) to ensure ~1 second of data. The receiver reads up to 8 KB and uses a sliding-window search to find the ramp pattern.
- The sender runs as I2S master, the receiver as I2S slave, since both share the same BCLK/WS lines.
- Single-device Unity tests on the sender use TX-only mode (`DIN=-1`) to avoid needing a loopback connection during that phase.
- ESP32-S2 uses GPIO19 for DOUT instead of GPIO18 (`LED_BUILTIN` / `DAC2`).
- ESP32 uses GPIO13 for WS instead of GPIO5 (strapping pin).
