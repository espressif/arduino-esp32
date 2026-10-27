/*
 * LE Audio -- Player (Unicast Server sink -> LC3 decode -> I2S DAC)
 *
 * Publishes one sink ASE. When a Unicast Client configures it, the stream is
 * attached to a BLEAudioPlayer, which buffers the incoming LC3 SDUs, decodes
 * them (concealing lost ones) and plays the PCM on an I2S DAC. The player
 * follows the stream: it plays while the client streams and idles otherwise.
 * Use a phone/PC with LE Audio, or any LC3 Unicast Client, as the source.
 *
 * The same player works on a Broadcast Sink:
 *   sink.setStreams(2);  ...  player.attach(sink.stream(0), sink.stream(1));
 *
 * Requires the LC3 codec (managed component esp_audio_codec) in the core; on
 * builds without it the sketch reports that and idles.
 *
 * Wire an I2S DAC (e.g. PCM5102, MAX98357A) to the pins below.
 *
 * Callback style: lambdas and named functions.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_AUDIO_LC3_SUPPORTED

// ---- I2S DAC pinout (edit for your board) ----
// Named DAC_* because some board variants already define I2S_* pins.
#define DAC_BCLK 5  // bit clock   (BCLK / SCK)
#define DAC_WS   6  // word select (LRCLK / WS)
#define DAC_DOUT 7  // data out    (DIN on the DAC)

static const char *DEVICE_NAME = "LE Audio Player";

BLEAudio audio;
BLEAudioUnicastServer unicastServer;
BLEAudioPlayer player;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// Named-function callback.
void onSinkStopped(BLEAudioStream &, uint8_t reason) {
  Serial.printf("[sink] stopped (reason 0x%02X)\n", reason);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Player (LC3 -> I2S) ===");

  BTStatus st = BLE.begin(DEVICE_NAME);
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // The DAC is opened at the negotiated sample rate once a stream starts.
  BLEAudioI2sConfig i2s;
  i2s.bclk = DAC_BCLK;
  i2s.ws = DAC_WS;
  i2s.dout = DAC_DOUT;
  st = player.begin(i2s);
  if (!st) {
    halt("player.begin", st);
  }

  // One sink ASE that may carry both channels of a stereo stream.
  unicastServer = audio.createUnicastServer();
  unicastServer.setSinkStreams(1)
    .setSourceStreams(0)
    .setMaxChannelsPerStream(2)
    .setSupportedPresets(
      BLEAudioPresetBit(BLEAudioCodecPreset::LC3_16_2_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_24_2_1)
      | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_2_1)
    )
    .setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational)
    .setSinkLocation(BLEAudioLocation::FrontLeft | BLEAudioLocation::FrontRight);

  for (size_t i = 0; i < unicastServer.streamCount(); i++) {
    BLEAudioStream s = unicastServer.stream(i);
    // Lambda callback: the direction is known once the client configures the ASE.
    s.onConfigured([](BLEAudioStream &stream) {
      if (stream.direction() != BLEAudioStream::Direction::Rx) {
        return;
      }
      BLEAudioCodecConfig c = stream.codecConfig();
      Serial.printf("[sink] configured: %lu Hz, %u us, %u octets x %u ch\n", (unsigned long)c.samplingRateHz, c.frameDurationUs, c.octetsPerFrame, c.channels());
      player.stop();  // no-op unless a previous stream was attached
      BTStatus r = player.attach(stream);
      if (r) {
        r = player.start();
      }
      if (!r) {
        Serial.printf("player: %s\n", r.toString());
      }
    });
    s.onStopped(onSinkStopped);
  }

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  BLEAdvertising adv = BLE.getAdvertising();
  adv.setName(DEVICE_NAME);
  st = adv.start();
  if (!st) {
    halt("advertising", st);
  }

  Serial.println("Ready. Waiting for a Unicast Client to stream audio...");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 5000) {
    last = millis();
    if (player.sampleRate()) {
      Serial.printf(
        "[player] %lu Hz  rx %lu  dropped %lu  lost %lu  plc %lu  underruns %lu\n", (unsigned long)player.sampleRate(),
        (unsigned long)player.sdusReceived(), (unsigned long)player.sdusDropped(), (unsigned long)player.sdusLost(), (unsigned long)player.plcFrames(),
        (unsigned long)player.underruns()
      );
    }
  }
  delay(100);
}

#else  // !BLE_AUDIO_LC3_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println("\nLEAudio_Player requires the LC3 codec (esp_audio_codec) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_LC3_SUPPORTED */
