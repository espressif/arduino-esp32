# LE Audio Unicast USB Transmitter

Unicast client like `UnicastMic`, but fed by a USB speaker instead of an I2S mic. Whatever the PC plays is streamed over LE Audio. No I2S hardware needed.

## Pair with

`UnicastUSBReceiver` (`TARGET_NAME = "LE Audio USB Rx"` in `UnicastUSBTransmitter.ino:25`) for all-USB loopback with two boards. Point at `"LE Audio Speaker"` to drive the I2S DAC speaker instead.

## Hardware

Any LE Audio-capable board with native USB. Fixed 48 kHz on both transports: `kUsbRateHz = 48000`, `USBAudioCard(48000, UAC_BPS_16, UAC_SPK_STEREO, UAC_MIC_NONE)`, `setPreset(LC3_48_2_1).setSinkStreams(1)`.

On ESP32-S31 Function CoreBoard 1 the TinyUSB audio device is on the USB-A (USB-HS) port, not the Type-C DBG/UART ports. With `USBMode=default,CDCOnBoot=cdc`, `Serial` is also on USB-A. For logs on Type-C UART while audio stays on USB-A, use `CDCOnBoot=default`. A direct USB-A male to USB-C male cable to a Mac does not enumerate (CC `Rp` forces the Mac to device mode). The Mac must stay host via a C-to-A host adapter plus A-to-A cable.

## Flash / run checklist

1. **Board:** `esp32s31`, **USBMode:** `default`, **PartitionScheme:** `huge_app`.
2. Connect the native USB port (not the UART bridge) to the PC and pick it as playback device.
3. Start the receiver first, then flash this sketch and open Serial at 115200 on the USB-A CDC port (or use `CDCOnBoot=default` and monitor Type-C UART).
4. Expected log: `Scanning for "LE Audio USB Rx"...`, `Found`, `Link X ready, opening the stream...`, `Streaming to the sink`. Link loss prints `Link X lost` and restarts the scan.

## Sketch options

* **`TARGET_NAME`**: sink name to find. Default is the matching USB receiver.
* Data path: `uac.onData(onUsbPlayback)` fills a 32 KB ring on the USB task (oldest dropped when full); `recorder.setPCMSource(onUsbPcmSource)` downmixes stereo USB frames to mono and pads underruns with silence. `recorder.begin()` with no pins, `attach(Tx)` + `startCapture()` from `loop()` on `onStarted`.
