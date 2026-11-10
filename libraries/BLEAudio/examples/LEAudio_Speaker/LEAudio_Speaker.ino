/*
 * LE Audio -- Smart speaker
 *
 * A mains-powered speaker that plays unicast audio from a phone, PC or the
 * LEAudio_Phone example:
 *
 *   - CAP acceptor with a CSIS set member (a set of one)
 *   - BAP unicast server: one stereo-capable sink ASE
 *   - VCP volume renderer
 *   - TMAP identity: unicast media receiver
 *   - LC3 player decoding the sink stream to an I2S DAC
 *
 * One sink ASE accepts mono or interleaved stereo, so a phone can send both
 * channels over one CIS. Without the LC3 codec the speaker still publishes
 * its services and accepts streams, but does not decode them.
 *
 * Wire an I2S DAC (e.g. PCM5102 or MAX98357A) to the pins below. Pair it with
 * LEAudio_Phone on a second LE-Audio-capable board, or stream from a phone.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>

#if BLE_AUDIO_SUPPORTED

// ---- I2S DAC pinout (edit for your board) ----
#define DAC_BCLK 5  // bit clock   (BCLK / SCK)
#define DAC_WS   6  // word select (LRCLK / WS)
#define DAC_DOUT 7  // data out    (DIN on the DAC)

static const char *DEVICE_NAME = "LE Audio Speaker";

BLEAudio audio;
BLEAudioCapAcceptor capAcceptor;
BLEAudioUnicastServer unicastServer;
BLEAudioVolumeRenderer volumeRenderer;
BLEAudioTmap tmap;

#if BLE_AUDIO_LC3_SUPPORTED
BLEAudioPlayer player;
#endif

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// The phone configured the sink ASE: hand the stream to the decoder.
void onSinkConfigured(BLEAudioStream &stream) {
  BLEAudioCodecConfig codec = stream.codecConfig();
  Serial.printf("[sink] configured: %lu Hz, %u us, %u octets x %u ch\n", (unsigned long)codec.samplingRateHz, codec.frameDurationUs,
                codec.octetsPerFrame, codec.channels());
#if BLE_AUDIO_LC3_SUPPORTED
  player.stop();  // no-op unless a previous stream was attached
  BTStatus st = player.attach(stream);
  if (st) {
    st = player.start();
  }
  if (!st) {
    Serial.printf("player: %s\n", st.toString());
  }
#endif
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Smart Speaker ===");

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

  capAcceptor = audio.createCapAcceptor();
  if (!capAcceptor) {
    halt("createCapAcceptor", BTStatus::NotSupported);
  }
  capAcceptor.setSetSize(1).setRank(1);

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

  volumeRenderer = audio.createVolumeRenderer();
  volumeRenderer.setInitialVolume(160).onStateChanged([](uint8_t volume, bool muted) {
    Serial.printf("[VCP] volume=%u%s\n", volume, muted ? " (muted)" : "");
  });

  tmap = audio.createTmap();
  tmap.setRoles(BLEAudioTmapRole::UnicastMediaReceiver);

#if BLE_AUDIO_LC3_SUPPORTED
  BLEAudioI2sConfig dac;
  dac.bclk = DAC_BCLK;
  dac.ws = DAC_WS;
  dac.dout = DAC_DOUT;
  st = player.begin(dac);
  if (!st) {
    halt("player.begin", st);
  }
#else
  Serial.println("LC3 codec not compiled in: streams run without decode.");
#endif

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  BLEAdvertising adv = BLE.getAdvertising();
  adv.setType(BLEAdvType::ConnectableScannable);
  adv.setName(DEVICE_NAME);
  adv.setAppearance(0x0840);  // Generic Audio Sink
  st = adv.start();
  if (!st) {
    halt("advertising", st);
  }

  Serial.printf("Ready. Connect a phone or LEAudio_Phone to \"%s\".\n", DEVICE_NAME);
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 5000) {
    last = millis();
#if BLE_AUDIO_LC3_SUPPORTED
    if (player.sampleRate()) {
      Serial.printf("[player] %lu Hz x %u ch, %lu SDUs, %lu lost, %lu PLC, %lu underruns\n", (unsigned long)player.sampleRate(),
                    player.channels(), (unsigned long)player.sdusReceived(), (unsigned long)player.sdusLost(),
                    (unsigned long)player.plcFrames(), (unsigned long)player.underruns());
    }
#endif
  }
  delay(100);
}

#else  // !BLE_AUDIO_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_Speaker requires the LE Audio library (<BLEAudio.h>) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_SUPPORTED */
