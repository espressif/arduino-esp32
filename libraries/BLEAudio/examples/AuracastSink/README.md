# LE Audio Auracast Sink

Broadcast receiver. Scans, PA-syncs, joins the BIG, and plays it on the speaker. Includes the BASS Scan Delegator, so a phone assistant can steer it.

## Pair with

`AuracastSource`, or any Auracast transmitter / phone.

## Hardware

ESP32-S31 Function CoreBoard 1 (ES8311 DAC + NS4150B amp). The `I2S_*` and `PA_CTRL` pins plus `es8311Begin()` come from the board variant.

## Flash / run checklist

1. **Board:** `esp32s31`, **PartitionScheme:** `huge_app`.
2. Start the source first so the broadcast is on air.
3. Flash this sketch, open Serial at 115200.
4. Expected log: `Scanning for "ESP32 Auracast"...`, `Found ...`, `Joined: playing`. `Join failed` means no fitting BIS (`NotFound`) or wrong/missing code (`PermissionDenied`).

## Sketch options

* **`TARGET_NAME`** (`"ESP32 Auracast"`): empty joins the first broadcast found. **`BROADCAST_CODE`** (`""`): must match the source for encrypted broadcasts. **`CHANNELS`** (`2`): 1 is 1 BIS mono, 2 is 2 BIS stereo.
* Roles: `createBroadcastSink` + `configureAuracast(name, channels)`. The sketch stays connectable so a phone can reach BASS.
* Data path: `player.begin(I2S_SCLK, I2S_LRCK, I2S_DOUT, I2S_MCLK)` with `PA_CTRL HIGH`, `attach(stream)` or `attach(left, right)`, then `startPlayback()`. Playback follows the BIG on its own.
