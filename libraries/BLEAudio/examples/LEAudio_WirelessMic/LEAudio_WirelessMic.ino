/*
 * LE Audio -- Wireless microphone
 *
 * An ESP32 microphone that sends mono LC3 audio over a unicast CIS to a
 * speaker (LEAudio_UnicastSpeaker) or any BAP unicast server.
 *
 *   - BAP unicast client: scans, connects and starts one sink stream
 *   - LC3 recorder: I2S microphone -> LC3 -> CIS
 *
 * This is the source side of a wireless microphone link. It does not render
 * audio and does not need CAP or the control profiles: the speaker is the
 * unicast server and the microphone is the client.
 *
 * Requires an LE Audio-capable target (ESP32-S31) and the LC3 codec
 * (esp_audio_codec). On the ESP32-S31 Function CoreBoard 1 the onboard
 * microphone feeds the ES8311 ADC; the board variant supplies the I2S pins.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>
#include "board_audio.h"

// ESP32-S31 Function CoreBoard 1: onboard microphone -> ES8311 ADC.
// The pin macros come from the board variant.

static const char *TARGET_NAME = "LE Audio Speaker";

BLEAudio audio;
BLEAudioUnicastClient audioClient;
BLEAudioRecorder recorder;

BTAddress peerAddress;
volatile bool doConnect = false;
volatile bool doStartRecorder = false;
volatile bool doStopRecorder = false;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
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
  Serial.printf("Speaker has %u sink / %u source ASEs\n", peer.sinkEndpoints, peer.sourceEndpoints);
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

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Wireless Microphone ===");

  BTStatus st = BLE.begin("LE Audio Wireless Mic");
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
  // The defaults fit a wireless microphone: one sink stream (audio sent to
  // the speaker), no source stream, LC3 16 kHz and the Media context.
  // Register callbacks separately so each role is easy to read.
  audioClient.onDiscovered(onPeerDiscovered);
  audioClient.onStarted(onStreamsStarted);
  audioClient.onStopped(onStreamsStopped);
  audioClient.onError(onSetupError);

  // audio.begin() registers the client role. The I2S input and LC3 recorder
  // are opened afterwards, so the order is the same as in the player examples.
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // Configure the onboard ES8311 ADC; it follows the I2S clocks, so it works
  // at every LC3 sample rate.
  if (!board_audio::es8311Begin()) {
    Serial.println("ES8311 init failed; check the codec I2C bus");
  }
  st = recorder.begin(I2S_SCLK, I2S_LRCK, I2S_DIN, I2S_MCLK);
  if (!st) {
    halt("recorder.begin", st);
  }

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
      Serial.printf("[mic] %lu Hz x %u ch, %lu SDUs sent, %lu errors\n", (unsigned long)recorder.sampleRate(), recorder.channels(),
                    (unsigned long)recorder.sdusSent(), (unsigned long)recorder.sendErrors());
    }
  }
  delay(100);
}
