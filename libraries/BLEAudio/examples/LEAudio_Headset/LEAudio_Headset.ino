/*
 * LE Audio -- Headset
 *
 * A complete headset, the device a phone connects to:
 *
 *   - CAP acceptor with a CSIS set member (one earbud of a binaural pair)
 *   - BAP unicast server: one sink ASE (phone audio) + one source ASE (mic)
 *   - VCP volume renderer and MICP microphone device
 *   - MCP media player and CCP call server (on-headset buttons)
 *   - TMAP / GMAP identity (call terminal + media receiver + game terminal)
 *   - LC3 player on the sink stream and LC3 recorder on the source stream
 *
 * The codec tasks run only when the LC3 codec is compiled in; without it the
 * headset still advertises and accepts streams, and audio is not decoded.
 *
 * Wire an I2S DAC to DAC_* and an I2S microphone to MIC_* (edit the pins for
 * your board). Pair it with LEAudio_Phone on a second LE-Audio-capable board,
 * or connect from a phone/PC with LE Audio support.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>

#if BLE_AUDIO_SUPPORTED

// ---- I2S pinout (edit for your board) ----
#define DAC_BCLK 5   // bit clock   (BCLK / SCK)
#define DAC_WS   6   // word select (LRCLK / WS)
#define DAC_DOUT 7   // data out    (DIN on the DAC)
#define MIC_BCLK 15  // bit clock   (BCLK / SCK)
#define MIC_WS   16  // word select (LRCLK / WS)
#define MIC_DIN  17  // data in     (SD on the microphone)

static const char *DEVICE_NAME = "LE Audio Headset";

BLEAudio audio;
BLEAudioCapAcceptor capAcceptor;
BLEAudioUnicastServer unicastServer;
BLEAudioVolumeRenderer volumeRenderer;
BLEAudioMicDevice micDevice;
BLEAudioMediaPlayer mediaPlayer;
BLEAudioCallServer callServer;
BLEAudioTmap tmap;
BLEAudioGmap gmap;

#if BLE_AUDIO_LC3_SUPPORTED
BLEAudioPlayer player;
BLEAudioRecorder recorder;
#endif

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

const char *directionName(BLEAudioStream::Direction dir) {
  switch (dir) {
    case BLEAudioStream::Direction::Rx: return "sink";
    case BLEAudioStream::Direction::Tx: return "source";
    default: return "unknown";
  }
}

// The phone configured an ASE. Bind the codec task to the stream it selected.
void onStreamConfigured(BLEAudioStream &stream) {
  BLEAudioCodecConfig codec = stream.codecConfig();
  Serial.printf("[%s] configured: %lu Hz, %u us, %u octets x %u ch\n", directionName(stream.direction()),
                (unsigned long)codec.samplingRateHz, codec.frameDurationUs, codec.octetsPerFrame, codec.channels());
#if BLE_AUDIO_LC3_SUPPORTED
  BTStatus st;
  if (stream.direction() == BLEAudioStream::Direction::Rx) {
    player.stop();  // no-op unless a previous stream was attached
    st = player.attach(stream);
    if (st) {
      st = player.start();
    }
    if (!st) {
      Serial.printf("player: %s\n", st.toString());
    }
  } else if (stream.direction() == BLEAudioStream::Direction::Tx) {
    recorder.stop();
    st = recorder.attach(stream);
    if (st) {
      st = recorder.start();
    }
    if (!st) {
      Serial.printf("recorder: %s\n", st.toString());
    }
  }
#endif
}

void onStreamStarted(BLEAudioStream &stream) {
  Serial.printf("[%s] streaming on conn %u\n", directionName(stream.direction()), stream.connHandle());
}

void onStreamStopped(BLEAudioStream &stream, uint8_t reason) {
  Serial.printf("[%s] stopped (reason 0x%02X)\n", directionName(stream.direction()), reason);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Headset ===");

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
    Serial.printf("Phone connected (link %u)\n", connHandle);
  });
  audio.onDisconnected([](uint16_t connHandle) {
    Serial.printf("Phone disconnected (link %u)\n", connHandle);
  });

  // CAP ties the profiles below into one audio device. This is one earbud of
  // a binaural set; a pair uses the same set size with rank 1 and rank 2.
  capAcceptor = audio.createCapAcceptor();
  if (!capAcceptor) {
    halt("createCapAcceptor", BTStatus::NotSupported);
  }
  capAcceptor.setSetSize(2).setRank(1);

  // One sink ASE for phone audio and one source ASE for the microphone.
  unicastServer = audio.createUnicastServer();
  if (!unicastServer) {
    halt("createUnicastServer", BTStatus::NotSupported);
  }
  unicastServer.configureHeadset();
  for (size_t i = 0; i < unicastServer.streamCount(); i++) {
    BLEAudioStream stream = unicastServer.stream(i);
    stream.onConfigured(onStreamConfigured);
    stream.onStarted(onStreamStarted);
    stream.onStopped(onStreamStopped);
  }

  // Volume and microphone are controlled by the phone over VCP / MICP.
  volumeRenderer = audio.createVolumeRenderer();
  volumeRenderer.setInitialVolume(128).onStateChanged([](uint8_t volume, bool muted) {
    Serial.printf("[VCP] volume=%u%s\n", volume, muted ? " (muted)" : "");
  });

  micDevice = audio.createMicDevice();
  micDevice.onMuteChanged([](bool muted) {
    Serial.printf("[MICP] microphone %s\n", muted ? "muted" : "unmuted");
  });

  // On-headset media and call buttons are exposed through MCP / CCP.
  mediaPlayer = audio.createMediaPlayer();
  mediaPlayer.setPlayerName(DEVICE_NAME);
  callServer = audio.createCallServer();
  callServer.setProviderName(DEVICE_NAME).onOriginate([](uint8_t callIndex, const String &uri) -> bool {
    Serial.printf("[CCP] originating call %u to %s\n", callIndex, uri.c_str());
    return true;
  });

  // Identity: a call terminal and media receiver (TMAP), and the headset end
  // of a game-audio link (GMAP UGT with both sink and source).
  tmap = audio.createTmap();
  tmap.setRoles(BLEAudioTmapRole::CallTerminal | BLEAudioTmapRole::UnicastMediaReceiver);
  gmap = audio.createGmap();
  gmap.setRoles(BLEAudioGmapRole::UnicastGameTerminal);
  gmap.setFeatures(0, BLEAudioGmapFeatures::UgtSink | BLEAudioGmapFeatures::UgtSource, 0, 0);

#if BLE_AUDIO_LC3_SUPPORTED
  BLEAudioI2sConfig dac;
  dac.bclk = DAC_BCLK;
  dac.ws = DAC_WS;
  dac.dout = DAC_DOUT;
  st = player.begin(dac);
  if (!st) {
    halt("player.begin", st);
  }

  BLEAudioI2sConfig mic;
  mic.bclk = MIC_BCLK;
  mic.ws = MIC_WS;
  mic.din = MIC_DIN;
  st = recorder.begin(mic);
  if (!st) {
    halt("recorder.begin", st);
  }
#else
  Serial.println("LC3 codec not compiled in: streams run without decode/encode.");
#endif

  // One coordinated commit publishes CAS/CSIS, PACS/ASCS, VCS, MICS, GMCS,
  // GTBS, TMAS and GMAS.
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
      Serial.printf("[player] %lu Hz x %u ch, %lu SDUs, %lu lost, %lu PLC\n", (unsigned long)player.sampleRate(), player.channels(),
                    (unsigned long)player.sdusReceived(), (unsigned long)player.sdusLost(), (unsigned long)player.plcFrames());
    }
    if (recorder.sampleRate()) {
      Serial.printf("[recorder] %lu Hz x %u ch, %lu SDUs sent, %lu errors\n", (unsigned long)recorder.sampleRate(), recorder.channels(),
                    (unsigned long)recorder.sdusSent(), (unsigned long)recorder.sendErrors());
    }
#endif
  }
  delay(100);
}

#else  // !BLE_AUDIO_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_Headset requires the LE Audio library (<BLEAudio.h>) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_SUPPORTED */
