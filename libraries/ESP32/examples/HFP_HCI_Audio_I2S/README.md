# ESP32 HFP HCI Audio Bridge

Demonstrates Bluetooth Hands-Free Profile (HFP) audio routing on the ESP32.
The sketch connects to a paired phone, answers and ends calls with a button,
and bridges the SCO audio stream to external audio hardware:

- **Playback** (phone → speaker): BT SCO RX → I2S TX → PCM5102A DAC
- **Capture** (microphone → phone): PCM1808 ADC → I2S RX → BT SCO TX

Both codecs are supported: CVSD (8 kHz narrowband) and mSBC (16 kHz wideband).
The codec is negotiated automatically with the phone.

## Hardware

| Component | Description |
|-----------|-------------|
| PCM5102A  | I2S DAC — reproduces the caller's voice on a speaker or headphone |
| PCM1808   | I2S ADC — captures the local microphone and sends it to the phone |
| Button    | Momentary push-button between GPIO 16 and GND (answer / hang up)  |

### Wiring

#### I2S bus (shared by both codecs)

| ESP32 GPIO | Signal | PCM5102A pin | PCM1808 pin |
|------------|--------|--------------|-------------|
| 25         | BCK    | BCK          | BCK         |
| 26         | LRCK   | LCK          | LRCK        |
| 27         | DOUT   | DIN          | —           |
| 32         | DIN    | —            | DOUT        |
| 0          | MCLK   | —            | SCKI        |

> **GPIO 0 (MCLK):** The ESP32 I2S0 master clock output is hardware-tied to
> GPIO 0 (CLK\_OUT1). This pin must be free at boot (do not pull it low).

#### PCM5102A static pin strapping

| PCM5102A pin | Connection | Purpose                        |
|--------------|------------|--------------------------------|
| FMT          | GND        | I2S format                     |
| XSMT         | 3.3 V      | Soft mute disabled (audio on)  |
| SCK          | GND        | System clock from BCK          |

#### PCM1808 static pin strapping

| PCM1808 pin | Connection | Purpose                     |
|-------------|------------|-----------------------------|
| FMT         | GND        | I2S format                  |
| MD0         | GND        | Slave mode, I2S format (LSB) |
| MD1         | GND        | Slave mode, I2S format (MSB) |

#### Answer / hang-up button

| ESP32 GPIO | Connection              |
|------------|-------------------------|
| 16         | One side of the button  |
| GND        | Other side of the button |

The internal pull-up is enabled in software; no external resistor is needed.

## Build requirements

This example requires two sdkconfig options that enable SCO audio over HCI:
```
CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=y
CONFIG_BTDM_CTRL_BR_EDR_SCO_DATA_PATH_HCI=y
```

These options are enabled in the precompiled ESP32 Bluetooth libraries via
[espressif/esp32-arduino-lib-builder#360](https://github.com/espressif/esp32-arduino-lib-builder/pull/360),
which has been accepted and merged. They are set automatically when building
with an arduino-esp32 board package that includes the updated libraries.

## Usage

1. Upload the sketch and open the Serial monitor at 115 200 baud.
2. On the phone, pair with **ESP32\_HFP\_BRIDGE** (appears in Bluetooth scan).
   The ESP32 connects automatically and remembers the phone across power cycles.
3. Call the phone from another number (or call out from the phone).
4. **Press the button** to answer. Press again to hang up.

The ADC peak level is printed every second while a call is active, confirming
that microphone audio is reaching the phone:

```
I2S started: mSBC  fs=16000 Hz
ADC peak: 20480  drop: 0
ADC peak: 20614  drop: 0
```

A peak near zero with signal present indicates a wiring or strapping issue on
the PCM1808.

## Notes

- **32-bit I2S slots** are required even though the audio is 16-bit. The
  PCM1808 needs BCK = 64 × fs; 16-bit slots give BCK = 32 × fs which is too
  slow and produces silence on the capture path.
- **mSBC (wideband)** is negotiated automatically when both the phone and the
  ESP32 support HFP 1.6. If the phone falls back to CVSD the sketch adapts
  without any change.
- The sketch reconnects automatically if the HFP link or SCO channel drops
  during a call.
- **Other I2S hardware** should work provided it operates in slave mode with
  32-bit slots. Two codec-specific details may need adjusting:
  - **ADC bit extraction** (`adcCaptureTask`): the `<< 1 >> 16` shift assumes
    the PCM1808's 24-bit MSB-first packing with a 1-bit I2S delay. A different
    ADC may place its bits differently within the 32-bit slot — check its
    datasheet and update the extraction formula accordingly.
  - **DAC alignment** (`hfIncomingDataCb`): the `<< 16` left-shift assumes the
    DAC reads audio from the MSBs of the 32-bit slot, which is standard for
    most I2S DACs (including the PCM5102A). If your DAC is LSB-aligned, adjust
    the shift accordingly.
