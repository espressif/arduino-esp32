/*
 * LE Audio -- Unicast speaker
 *
 * A speaker that receives audio over a Connected Isochronous Stream from a
 * phone/PC or from the LEAudio_WirelessMic example:
 *
 *   - CAP acceptor with a CSIS set member (a set of one)
 *   - BAP unicast server: one stereo-capable sink ASE
 *   - VCP volume renderer
 *   - LC3 player decoding the sink stream to an I2S DAC
 *
 * One sink ASE accepts mono or interleaved stereo, so the source can send
 * both channels over one CIS. The speaker advertises connectably and waits
 * for a unicast client; it never starts a stream itself.
 *
 * Requires an LE Audio-capable target (ESP32-S31) and the LC3 codec
 * (esp_audio_codec). Wire an I2S DAC (e.g. PCM5102 or MAX98357A) to the pins
 * below.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>

// ESP32-S31 Function CoreBoard 1: ES8311 codec -> NS4150B speaker amplifier.
// The pin macros come from the board variant.

static const char *DEVICE_NAME = "LE Audio Speaker";

BLEAudio audio;
BLEAudioCapAcceptor capAcceptor;
BLEAudioUnicastServer unicastServer;
BLEAudioVolumeRenderer volumeRenderer;
BLEAudioPlayer player;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// A client configured the sink ASE: hand the stream to the decoder.
void onSinkConfigured(BLEAudioStream &stream) {
  BLEAudioCodecConfig codec = stream.codecConfig();
  Serial.printf("[sink] configured: %lu Hz, %u us, %u octets x %u ch\n", (unsigned long)codec.samplingRateHz, codec.frameDurationUs,
                codec.octetsPerFrame, codec.channels());

  player.stopPlayback();  // no-op unless a previous stream was attached
  BTStatus st = player.attach(stream);
  if (st) {
    st = player.startPlayback();
  }
  if (!st) {
    Serial.printf("player: %s\n", st.toString());
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Unicast Speaker ===");

  BTStatus st = BLE.begin(DEVICE_NAME);
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }
  audio.onLinkReady([](uint16_t connHandle) {
    Serial.printf("Source connected (link %u)\n", connHandle);
  });
  audio.onDisconnected([](uint16_t connHandle) {
    Serial.printf("Source disconnected (link %u)\n", connHandle);
  });

  // CAP ties the profiles into one audio device; this speaker is a set of one.
  capAcceptor = audio.createCapAcceptor();
  if (!capAcceptor) {
    halt("createCapAcceptor", BTStatus::NotSupported);
  }
  capAcceptor.setSetSize(1).setRank(1);

  // One stereo-capable sink ASE for the incoming audio.
  unicastServer = audio.createUnicastServer();
  if (!unicastServer) {
    halt("createUnicastServer", BTStatus::NotSupported);
  }
  unicastServer.configureMediaSink();
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

  // The source can change volume over VCP.
  volumeRenderer = audio.createVolumeRenderer();
  volumeRenderer.setInitialVolume(160).onStateChanged([](uint8_t volume, bool muted) {
    Serial.printf("[VCP] volume=%u%s\n", volume, muted ? " (muted)" : "");
  });

  pinMode(PA_CTRL, OUTPUT);
  digitalWrite(PA_CTRL, HIGH);

  st = player.begin(I2S_SCLK, I2S_LRCK, I2S_DOUT, I2S_MCLK);
  if (!st) {
    halt("player.begin", st);
  }

  // One coordinated commit publishes CAS/CSIS, PACS/ASCS and VCS.
  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  BLEAdvertising adv = BLE.getAdvertising();
  adv.setType(BLEAdvType::ConnectableScannable);
  adv.setName(DEVICE_NAME);
  adv.setAppearance(BLE_APPEARANCE_AUDIO_SINK_STANDALONE_SPEAKER);
  st = adv.start();
  if (!st) {
    halt("advertising", st);
  }

  Serial.printf("Ready. Connect a phone/PC or LEAudio_WirelessMic to \"%s\".\n", DEVICE_NAME);
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 5000) {
    last = millis();
    if (player.sampleRate()) {
      Serial.printf("[player] %lu Hz x %u ch, %lu SDUs, %lu lost, %lu PLC, %lu underruns\n", (unsigned long)player.sampleRate(),
                    player.channels(), (unsigned long)player.sdusReceived(), (unsigned long)player.sdusLost(),
                    (unsigned long)player.plcFrames(), (unsigned long)player.underruns());
    }
  }
  delay(100);
}
