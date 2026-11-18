# LE Audio Unicast USB Receiver

Unicast sink like `UnicastSpeaker`, but decoded audio goes to the PC as a USB microphone. No I2S hardware needed.

## Pair with

`UnicastUSBTransmitter`, or a phone/PC with LE Audio. The PC records or listens to the USB microphone.

## Hardware

Any LE Audio-capable board with native USB. Fixed 48 kHz on both transports: `kUsbRateHz = 48000`, `USBAudioCard(48000, UAC_BPS_16, UAC_SPK_NONE, UAC_MIC_STEREO)`. Sink presets are narrowed to `LC3_48_1_1 | LC3_48_2_1`, so peers without 48 kHz LC3 cannot connect.

On ESP32-S31 Function CoreBoard 1 the TinyUSB audio device is on the USB-A (USB-HS) port, not the Type-C DBG/UART ports. With `USBMode=default,CDCOnBoot=cdc`, `Serial` is also on USB-A. For logs on Type-C UART while audio stays on USB-A, use `CDCOnBoot=default`. A direct USB-A male to USB-C male cable to a Mac does not enumerate (CC `Rp` forces the Mac to device mode). The Mac must stay host via a C-to-A host adapter plus A-to-A cable.

## Flash / run checklist

1. **Board:** `esp32s31`, **USBMode:** `default`, **PartitionScheme:** `huge_app`.
2. Connect the native USB port (not the UART bridge) to the PC.
3. Flash this sketch, open Serial at 115200 on the USB-A CDC port (or use `CDCOnBoot=default` and monitor Type-C UART).
4. Expected log: `Ready. Connect to "LE Audio USB Rx"...`, `Sender connected`, `Sender configured 48000 Hz, ...`, `Playing (link X)`, plus `USB mic opened by PC` once the PC opens the mic.

## Sketch options

* **`DEVICE_NAME`** (`"LE Audio USB Rx"`): must match the transmitter `TARGET_NAME`.
* Data path: `player.setPCMSink(onPlayerPcm)` before `player.begin()` (no pins), `attach(Rx)` + `startPlayback()` from `loop()` on `onConfigured`. The callback applies VCP volume/mute, widens mono to dual-mono, and `uac.write()` drops instead of blocking when the PC is not reading.
