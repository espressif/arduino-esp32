# LE Audio Wireless Microphone

Unicast client that captures the onboard mic and sends it to a speaker. It scans, connects, and asks the server to open one sink stream.

## Pair with

`UnicastSpeaker` (`TARGET_NAME = "LE Audio Speaker"` in `UnicastMic.ino:22`), or any BAP unicast server such as a phone/PC.

## Hardware

ESP32-S31 Function CoreBoard 1 (onboard mic through ES8311 ADC). The `I2S_*` pins and `es8311Begin()` come from the board variant.

## Flash / run checklist

1. **Board:** `esp32s31`, **PartitionScheme:** `huge_app`.
2. Start the speaker first so it is advertising.
3. Flash this sketch, open Serial at 115200.
4. Expected log: `Scanning for "LE Audio Speaker"...`, `Found`, `Link X ready, opening the stream...`, `Streaming to the speaker`. Link loss prints `Link X lost` and restarts the scan.

## Sketch options

* **`TARGET_NAME`**: speaker name to find and connect to.
* Roles: `createUnicastClient`, `connectAndStart(conn)`. Stream setup failures report through `onError` with step + response/reason codes.
* Data path: `recorder.begin(I2S_SCLK, I2S_LRCK, I2S_DIN, I2S_MCLK)`. `attach(Tx)` + `startCapture()` run from `loop()` on `onStarted`, never from the Bluetooth callback.
