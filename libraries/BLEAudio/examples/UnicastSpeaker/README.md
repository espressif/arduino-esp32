# LE Audio Unicast Speaker

Stereo-capable unicast sink that plays audio sent by a unicast client. This sketch never starts a stream itself.

## Pair with

`UnicastMic`, or a phone/PC with LE Audio.

## Hardware

ESP32-S31 Function CoreBoard 1 (ES8311 DAC + NS4150B amp). The `I2S_*` and `PA_CTRL` pins plus `es8311Begin()` / `es8311SetVolume()` come from the board variant, no extra include needed.

## Flash / run checklist

1. **Board:** `esp32s31`, **PartitionScheme:** `huge_app`.
2. Flash this sketch, open Serial at 115200.
3. Flash `UnicastMic` on a second board (or connect a phone). The speaker advertises as `LE Audio Speaker` (`DEVICE_NAME` in `UnicastSpeaker.ino:22`).
4. Expected log: `Ready. Connect to "LE Audio Speaker"...`, then `Sender connected`, `Sender configured <Hz>, <ch>`, `Playing (link X)`.

## Sketch options

* **`DEVICE_NAME`**: BLE device + advertising name.
* Roles: `createCapAcceptor` (set of 1) + `createUnicastServer` + `configureMediaSink` (1 stereo-capable sink ASE) + `createVolumeRenderer`. VCP volume drives the ES8311.
* Data path: `player.begin(I2S_SCLK, I2S_LRCK, I2S_DOUT, I2S_MCLK)`. `onConfigured` only sets `sinkReady`. `attach(Rx)` + `startPlayback()` run from `loop()`.
