/*
 * LE Audio unicast receiver with USB microphone output.
 *
 * The LE Audio RECEIVER side of a unicast link: accepts a Connected
 * Isochronous Stream from a phone/PC (or from LEAudio_UnicastUSBTransmitter)
 * and forwards the decoded audio to the PC over USB, where the board shows
 * up as a USB microphone (no speaker or I2S codec needed):
 *
 *   - CAP acceptor with a CSIS set member (a set of one)
 *   - BAP unicast server: one stereo-capable sink ASE
 *   - VCP volume renderer (mapped to a software gain on the USB path)
 *   - LC3 player decoding the sink stream to a PCM callback
 *   - USB Audio Class mic-only device (16-bit, 48 kHz) toward the PC
 *
 * The USB side runs at a fixed 48 kHz, so the server only publishes 48 kHz
 * LC3 presets; a peer that cannot do 48 kHz cannot connect. Mono streams are
 * duplicated to both USB channels; stereo streams pass through interleaved.
 *
 * Wiring: connect the board's native USB port (not the UART bridge) to the
 * PC. The PC enumerates a "USB Audio" microphone; open it in a recorder or
 * set it to "listen" to hear the LE Audio stream. Requires an LE Audio-capable
 * target with native USB (ESP32-S31) and the LC3 codec (esp_audio_codec).
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>
#include "USB.h"
#include "USBAudioCard.h"

static const char *DEVICE_NAME = "LE Audio USB Rx";

// USB runs at a fixed rate; keep LE Audio on the same rate so no resampling
// is needed. Peers that cannot do 48 kHz LC3 will not find a usable preset.
static constexpr uint32_t kUsbRateHz = 48000;
static constexpr size_t kScratchFrames = 960;  // >= one 48 kHz / 10 ms stereo frame

BLEAudio audio;
BLEAudioCapAcceptor capAcceptor;
BLEAudioUnicastServer unicastServer;
BLEAudioVolumeRenderer volumeRenderer;
BLEAudioPlayer player;
USBAudioCard uac(kUsbRateHz, UAC_BPS_16, UAC_SPK_NONE, UAC_MIC_STEREO);

volatile bool doAttachPlayer = false;
volatile bool usbMicOn = false;     // host opened the USB mic streaming interface
volatile uint8_t usbVolume = 255;   // VCP absolute volume 0..255 (255 = unity)
volatile bool usbMuted = false;
volatile uint32_t usbDropped = 0;    // PCM bytes dropped when USB could not take them
int16_t usbScratch[kScratchFrames * 2];

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// Decoded PCM lands here on the codec task: apply the VCP volume, widen mono
// to stereo when needed, and hand the bytes to the USB IN endpoint. Dropping
// (instead of blocking) keeps the codec task paced when the host is not
// reading; tud_audio_write only queues, so this is safe off the USB task.
// n counts int16 samples (frames x channels); stereo arrives interleaved.
void onPlayerPcm(const int16_t *pcm, size_t n) {
  if (!usbMicOn || pcm == nullptr || n == 0) {
    return;
  }
  uint8_t vol = usbVolume;
  bool muted = usbMuted;
  // player.channels() is valid once streaming; anything but stereo goes out
  // dual-mono so the fixed stereo USB descriptor always matches.
  bool stereo = (player.channels() == 2);
  size_t framesTotal = stereo ? (n / 2) : n;
  size_t framesDone = 0;
  while (framesDone < framesTotal) {
    size_t f = framesTotal - framesDone;
    if (f > kScratchFrames) {
      f = kScratchFrames;
    }
    int16_t *out = usbScratch;
    if (stereo) {
      const int16_t *in = pcm + framesDone * 2;
      for (size_t i = 0; i < f * 2; i++) {
        int32_t s = muted ? 0 : ((int32_t)in[i] * vol) / 255;
        *out++ = (int16_t)s;
      }
    } else {
      const int16_t *in = pcm + framesDone;
      for (size_t i = 0; i < f; i++) {
        int32_t s = muted ? 0 : ((int32_t)in[i] * vol) / 255;
        *out++ = (int16_t)s;
        *out++ = (int16_t)s;
      }
    }
    uint16_t bytes = (uint16_t)(f * 2 * sizeof(int16_t));
    uint16_t accepted = uac.write(usbScratch, bytes);
    if (accepted < bytes) {
      usbDropped = usbDropped + (uint32_t)(bytes - accepted);
      return;
    }
    framesDone += f;
  }
}

// A client configured the sink ASE. This callback runs on the Bluetooth host
// task, so only record the request; attach/start the codec task from loop().
void onSinkConfigured(BLEAudioStream &stream) {
  BLEAudioCodecConfig codec = stream.codecConfig();
  Serial.printf("[sink] configured: %lu Hz, %u us, %u octets x %u ch\n", (unsigned long)codec.samplingRateHz, codec.frameDurationUs,
                codec.octetsPerFrame, codec.channels());
  doAttachPlayer = true;
}

static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  (void)arg;
  if (event_base == ARDUINO_USB_EVENTS) {
    arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)event_data;
    if (event_id == ARDUINO_USB_STARTED_EVENT) {
      Serial.println("[USB] plugged");
    } else if (event_id == ARDUINO_USB_STOPPED_EVENT) {
      Serial.println("[USB] unplugged");
    }
  } else if (event_base == ARDUINO_USB_AUDIO_CARD_EVENTS) {
    arduino_usb_audio_card_event_data_t *data = (arduino_usb_audio_card_event_data_t *)event_data;
    if (event_id == ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT && data->interface_enable.interface == UAC_INTERFACE_MIC) {
      usbMicOn = data->interface_enable.enable;
      Serial.printf("[USB] mic stream %s\n", usbMicOn ? "opened" : "closed");
    } else if (event_id == ARDUINO_USB_AUDIO_CARD_SAMPLE_RATE_EVENT) {
      Serial.printf("[USB] host sample rate: %lu Hz\n", (unsigned long)data->sample_rate.rate);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Unicast USB Receiver ===");

  BTStatus st = BLE.begin(DEVICE_NAME);
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  // No explicit init: the engine initializes lazily on the first create*()
  // below. Create every role first, then call audio.begin() once to commit.
  audio.onLinkReady([](uint16_t connHandle) {
    Serial.printf("Source connected (link %u)\n", connHandle);
  });
  audio.onDisconnected([](uint16_t connHandle) {
    Serial.printf("Source disconnected (link %u)\n", connHandle);
  });

  // CAP ties the profiles into one audio device. A standalone bridge is a
  // coordinated set of one.
  capAcceptor = audio.createCapAcceptor();
  if (!capAcceptor) {
    halt("createCapAcceptor", BTStatus::NotSupported);
  }
  capAcceptor.setSetSize(1);
  capAcceptor.setRank(1);

  // One stereo-capable sink ASE for the incoming audio, 48 kHz presets only
  // so the decoded PCM always matches the fixed USB rate (no resampling).
  unicastServer = audio.createUnicastServer();
  if (!unicastServer) {
    halt("createUnicastServer", BTStatus::NotSupported);
  }
  unicastServer.configureMediaSink();
  unicastServer.setSupportedPresets(BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_1_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_2_1));
  for (size_t i = 0; i < unicastServer.streamCount(); i++) {
    BLEAudioStream stream = unicastServer.stream(i);
    stream.onConfigured(onSinkConfigured);
    stream.onStarted([](BLEAudioStream &s) {
      Serial.printf("[sink] streaming on conn %u\n", s.connHandle());
    });
    stream.onStopped([](BLEAudioStream &, uint8_t reason) {
      Serial.printf("[sink] stopped (reason 0x%02X)\n", reason);
    });
  }

  // The source can change volume over VCP; applied as a software gain on the
  // USB path (255 = unity, so a source that never touches volume is unaffected).
  volumeRenderer = audio.createVolumeRenderer();
  volumeRenderer.setInitialVolume(255);
  volumeRenderer.onStateChanged([](uint8_t volume, bool muted) {
    usbVolume = volume;
    usbMuted = muted;
    Serial.printf("[VCP] volume=%u%s\n", volume, muted ? " (muted)" : "");
  });

  // audio.begin() commits the roles and publishes their GATT services.
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // PCM lands in onPlayerPcm() instead of I2S pins; no I2S hardware needed.
  player.setPCMSink(onPlayerPcm);
  st = player.begin();
  if (!st) {
    halt("player.begin", st);
  }

  uac.onEvent(usbEventCallback);
  if (!uac.begin()) {
    Serial.println("USB audio begin failed; check USB-OTG support");
  }
  USB.onEvent(usbEventCallback);
  USB.begin();

  BLEAdvertising adv = BLE.getAdvertising();
  adv.setType(BLEAdvType::ConnectableScannable);
  adv.setName(DEVICE_NAME);
  adv.setAppearance(BLE_APPEARANCE_GENERIC_AUDIO_SINK);
  st = adv.start();
  if (!st) {
    halt("advertising", st);
  }

  Serial.printf("Ready. Connect a phone/PC to \"%s\", then open the USB microphone on the PC.\n", DEVICE_NAME);
}

void loop() {
  if (doAttachPlayer) {
    doAttachPlayer = false;
    BLEAudioStream sink = unicastServer.stream(BLEAudioStream::Direction::Rx);
    player.stopPlayback();  // no-op unless a previous stream was attached
    BTStatus st = player.attach(sink);
    if (st) {
      st = player.startPlayback();
    }
    if (!st) {
      Serial.printf("player: %s\n", st.toString());
    }
  }

  static uint32_t last = 0;
  if (millis() - last >= 5000) {
    last = millis();
    if (player.sampleRate()) {
      Serial.printf("[player] %lu Hz x %u ch, %lu SDUs, %lu lost, %lu PLC, %lu underruns, %lu USB bytes dropped\n",
                    (unsigned long)player.sampleRate(), player.channels(), (unsigned long)player.sdusReceived(),
                    (unsigned long)player.sdusLost(), (unsigned long)player.plcFrames(), (unsigned long)player.underruns(), (unsigned long)usbDropped);
    }
  }
  delay(100);
}
