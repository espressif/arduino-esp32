/*
 * LE Audio unicast transmitter fed by a USB speaker.
 *
 * The LE Audio TRANSMITTER side of a unicast link: the board shows up on the
 * PC as a USB speaker (UAC, 16-bit, 48 kHz). Audio the PC plays is captured
 * through USB, downmixed to mono, and streamed over a Connected Isochronous
 * Stream to a unicast server (LEAudio_UnicastUSBReceiver,
 * LEAudio_UnicastSpeaker, or any BAP unicast server):
 *
 *   - BAP unicast client: scans, connects and starts one sink stream
 *   - USB Audio speaker (host -> device): PCM captured via onData
 *   - LC3 recorder: USB PCM -> LC3 -> CIS (mono)
 *
 * The USB side runs at a fixed 48 kHz, so the client requests the
 * LC3_48_2_1 preset; peers that cannot do 48 kHz cannot be used.
 * This mirrors LEAudio_WirelessMic, with the I2S microphone replaced by USB.
 *
 * Wiring: connect the board's native USB port (not the UART bridge) to the
 * PC and select it as the playback device. Requires an LE Audio-capable
 * target with native USB (ESP32-S31) and the LC3 codec (esp_audio_codec).
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>
#include "USB.h"
#include "USBAudioCard.h"

// Playback target: a board running a unicast sink. Defaults to the matching
// receiver example for the all-USB loopback (PC -> USB speaker -> LE Audio ->
// USB mic -> PC); use "LE Audio Speaker" to play through a DAC speaker instead.
static const char *TARGET_NAME = "LE Audio USB Rx";

static constexpr uint32_t kUsbRateHz = 48000;
static constexpr uint32_t kUsbRingBytes = 32768;
static constexpr uint32_t kUsbRingMask = kUsbRingBytes - 1;

BLEAudio audio;
BLEAudioUnicastClient audioClient;
BLEAudioRecorder recorder;
USBAudioCard uac(kUsbRateHz, UAC_BPS_16, UAC_SPK_STEREO, UAC_MIC_NONE);

BTAddress peerAddress;
volatile bool doConnect = false;
volatile bool doStartRecorder = false;
volatile bool doStopRecorder = false;

// Single-producer (USB task) / single-consumer (codec task) byte ring holding
// interleaved stereo int16 PCM from the host. Overruns drop the oldest data;
// underruns are zero-filled by the PCM source below.
static uint8_t usbRing[kUsbRingBytes];
static volatile uint32_t usbRingRead = 0;
static volatile uint32_t usbRingWrite = 0;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// Host playback data lands here (USB task). Never blocks: drops oldest data
// instead of stalling the USB stack when the recorder cannot keep up.
void onUsbPlayback(void *data, uint16_t len) {
  if (data == nullptr || len == 0) {
    return;
  }
  const uint8_t *src = (const uint8_t *)data;
  uint32_t w = usbRingWrite;
  uint32_t r = usbRingRead;
  if (len >= kUsbRingBytes) {
    // Larger than the whole ring: keep only the newest window.
    src += len - (kUsbRingBytes - 1);
    len = kUsbRingBytes - 1;
    r = w;
  } else {
    uint32_t used = (w - r) & kUsbRingMask;
    if (len > (kUsbRingBytes - 1 - used)) {
      // Full: drop oldest to make room (lowest latency).
      r = (w + len - (kUsbRingBytes - 1));
    }
  }
  uint32_t wp = w & kUsbRingMask;
  uint32_t first = kUsbRingBytes - wp;
  if (first > len) {
    first = len;
  }
  memcpy(&usbRing[wp], src, first);
  memcpy(&usbRing[0], src + first, len - first);
  usbRingWrite = w + len;
  usbRingRead = r;
}

// Recorder PCM source (codec task): downmix one stereo USB frame to mono.
// Always returns a full buffer; missing frames are silence.
size_t onUsbPcmSource(int16_t *pcm, size_t maxSamples) {
  uint32_t r = usbRingRead;
  uint32_t w = usbRingWrite;
  uint32_t availFrames = ((w - r) & kUsbRingMask) / 4;
  size_t n = maxSamples < availFrames ? maxSamples : (size_t)availFrames;
  for (size_t i = 0; i < n; i++) {
    uint32_t o = (r + i * 4) & kUsbRingMask;
    int16_t left = (int16_t)(usbRing[o] | (usbRing[(o + 1) & kUsbRingMask] << 8));
    int16_t right = (int16_t)(usbRing[(o + 2) & kUsbRingMask] | (usbRing[(o + 3) & kUsbRingMask] << 8));
    pcm[i] = (int16_t)(((int32_t)left + (int32_t)right) / 2);
  }
  if (n < maxSamples) {
    memset(pcm + n, 0, (maxSamples - n) * sizeof(int16_t));
  }
  usbRingRead = r + (uint32_t)(n * 4);
  return maxSamples;
}

void onDeviceFound(BLEAdvertisedDevice device) {
  if (device.getName() != TARGET_NAME) {
    return;
  }
  Serial.printf("Found \"%s\" at %s\n", TARGET_NAME, device.getAddress().toString().c_str());
  peerAddress = device.getAddress();
  doConnect = true;
  BLE.getScan().stop();
}

// The engine secured the link and its GATT database is known: start BAP.
void onLinkReady(uint16_t connHandle) {
  Serial.printf("Link %u ready, starting the audio stream\n", connHandle);
  BTStatus st = audioClient.connectAndStart(connHandle);
  if (!st) {
    Serial.printf("connectAndStart failed: %s\n", st.toString());
  }
}

void onLinkLost(uint16_t connHandle) {
  Serial.printf("Link %u lost, scanning again\n", connHandle);
  doStopRecorder = true;
  BLE.getScan().start(0);
}

void onPeerDiscovered(const BLEAudioUnicastPeerInfo &peer) {
  Serial.printf("Sink has %u sink / %u source ASEs\n", peer.sinkEndpoints, peer.sourceEndpoints);
}

void onStreamsStarted() {
  Serial.println("Stream is up");
  doStartRecorder = true;
}

void onStreamsStopped() {
  Serial.println("Stream released");
  doStopRecorder = true;
}

void onSetupError(BLEAudioUnicastClient::Step step, uint8_t responseCode, uint8_t reason) {
  Serial.printf("ASCS step %u failed: rsp 0x%02X reason 0x%02X\n", (unsigned)step, responseCode, reason);
}

static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  (void)arg;
  if (event_base == ARDUINO_USB_EVENTS) {
    if (event_id == ARDUINO_USB_STARTED_EVENT) {
      Serial.println("[USB] plugged");
    } else if (event_id == ARDUINO_USB_STOPPED_EVENT) {
      Serial.println("[USB] unplugged");
    }
  } else if (event_base == ARDUINO_USB_AUDIO_CARD_EVENTS) {
    arduino_usb_audio_card_event_data_t *data = (arduino_usb_audio_card_event_data_t *)event_data;
    if (event_id == ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT && data->interface_enable.interface == UAC_INTERFACE_SPK) {
      Serial.printf("[USB] speaker stream %s\n", data->interface_enable.enable ? "opened" : "closed");
    } else if (event_id == ARDUINO_USB_AUDIO_CARD_SAMPLE_RATE_EVENT) {
      Serial.printf("[USB] host sample rate: %lu Hz\n", (unsigned long)data->sample_rate.rate);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Unicast USB Speaker ===");

  BTStatus st = BLE.begin("LE Audio USB Tx");
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  // No explicit init: the engine initializes lazily on the first create*()
  // below. Create every role first, then call audio.begin() once to commit.
  audio.onLinkReady(onLinkReady);
  audio.onDisconnected(onLinkLost);

  audioClient = audio.createUnicastClient();
  if (!audioClient) {
    halt("createUnicastClient", BTStatus::NotSupported);
  }
  // Fixed 48 kHz to match the USB side (no resampling): one sink stream,
  // 10 ms LC3 frames.
  audioClient.setPreset(BLEAudioCodecPreset::LC3_48_2_1).setSinkStreams(1);
  audioClient.onDiscovered(onPeerDiscovered);
  audioClient.onStarted(onStreamsStarted);
  audioClient.onStopped(onStreamsStopped);
  audioClient.onError(onSetupError);

  // audio.begin() commits the client role. The USB PCM source is attached
  // afterwards, mirroring the player/recorder examples' ordering.
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // USB PCM feeds the recorder instead of I2S; onUsbPcmSource drains the
  // ring buffer filled by onUsbPlayback and zero-fills underruns.
  recorder.setPCMSource(onUsbPcmSource);
  st = recorder.begin();
  if (!st) {
    halt("recorder.begin", st);
  }

  uac.onEvent(usbEventCallback);
  uac.onData(onUsbPlayback);
  if (!uac.begin()) {
    Serial.println("USB audio begin failed; check USB-OTG support");
  }
  USB.onEvent(usbEventCallback);
  USB.begin();

  BLEScan scan = BLE.getScan();
  scan.setActiveScan(true);
  scan.onResult(onDeviceFound);
  scan.start(0);
  Serial.printf("Scanning for \"%s\"...\n", TARGET_NAME);
}

void loop() {
  if (doConnect) {
    doConnect = false;
    BTStatus st = audio.connect(peerAddress);
    if (!st) {
      Serial.printf("connect failed: %s\n", st.toString());
      BLE.getScan().start(0);
    }
  }

  if (doStartRecorder) {
    doStartRecorder = false;
    BLEAudioStream tx = audioClient.stream(BLEAudioStream::Direction::Tx);
    BTStatus st = recorder.attach(tx);
    if (st) {
      st = recorder.startCapture();
    }
    if (!st) {
      Serial.printf("recorder: %s\n", st.toString());
    }
  }

  if (doStopRecorder) {
    doStopRecorder = false;
    recorder.stopCapture();
  }

  static uint32_t last = 0;
  if (millis() - last >= 5000) {
    last = millis();
    if (recorder.sampleRate()) {
      Serial.printf("[usb] %lu Hz in, %lu Hz x %u ch out, %lu SDUs sent, %lu errors\n", (unsigned long)uac.sampleRate(),
                    (unsigned long)recorder.sampleRate(), recorder.channels(), (unsigned long)recorder.sdusSent(), (unsigned long)recorder.sendErrors());
    }
  }
  delay(100);
}
