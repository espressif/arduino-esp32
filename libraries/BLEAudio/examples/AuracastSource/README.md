# LE Audio Auracast Source

Public broadcast transmitter. Streams the onboard mic to every receiver in range. No connections.

## Pair with

`AuracastSink`, or a phone/PC with Auracast.

## Hardware

ESP32-S31 Function CoreBoard 1 (onboard mic through ES8311 ADC). The `I2S_*` pins and `es8311Begin()` come from the board variant.

## Flash / run checklist

1. **Board:** `esp32s31`, **PartitionScheme:** `huge_app`.
2. Flash this sketch, open Serial at 115200.
3. Start the sink so it can PA-sync and join the BIG.
4. Expected log: `"ESP32 Auracast" on air (N channel(s), open/encrypted)`, `Broadcasting...` on start.

## Sketch options

* **`BROADCAST_NAME`** (`"ESP32 Auracast"`): shown by receivers. **`BROADCAST_CODE`** (`""`): empty is open, otherwise up to 16 chars and the sink needs the same code. **`CHANNELS`** (`2`): 1 is 1 BIS mono, 2 is 2 BIS stereo.
* Roles: `createBroadcastSource` + `configureAuracast(name, channels)` (`LC3_48_2_1`) + optional `setBroadcastCode`.
* Data path: `recorder.begin(I2S_SCLK, I2S_LRCK, I2S_DIN, I2S_MCLK)`, `attach(stream)` for mono or `attach(left, right)` for stereo, then `startCapture()` before `source.start()`.
