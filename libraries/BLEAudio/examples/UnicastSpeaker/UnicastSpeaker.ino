/*
 * LE Audio Unicast Speaker
 *
 * A speaker that plays audio sent by a phone/PC or by UnicastMic.
 * The remote side (unicast client) finds this speaker, configures one stream
 * and starts it. This sketch never starts a stream itself.
 *
 * Pair with: UnicastMic, or a phone/PC with LE Audio.
 * Hardware: ESP32-S31 Function CoreBoard 1 (ES8311 DAC + NS4150B amp).
 * The I2S and PA_CTRL pins plus es8311Begin()/es8311SetVolume() come from the board variant (no include needed).
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>

#ifndef I2S_SCLK
#error "This example requires the I2S pins defined in the board files."
#endif

// --- Tweakables -----------------------------------------------------------
static const char *DEVICE_NAME = "LE Audio Speaker";  // BLE device + adv name

// --- LE Audio roles -------------------------------------------------------
BLEAudio audio;
BLEAudioCapAcceptor capAcceptor;
BLEAudioUnicastServer unicastServer;
BLEAudioVolumeRenderer volumeRenderer;
BLEAudioPlayer player;

// Set on the Bluetooth task when the client configures our sink stream.
// The LC3 task is attached/started from loop(), never from the callback.
static volatile bool sinkReady = false;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// Runs on the Bluetooth task: just remember the stream is ready.
void onSinkConfigured(BLEAudioStream &stream) {
  // codecConfig() = the LC3 codec the client picked for this stream.
  BLEAudioCodecConfig codec = stream.codecConfig();
  Serial.printf("Sender configured %lu Hz, %u ch\n", (unsigned long)codec.samplingRateHz, codec.channels());
  sinkReady = true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Unicast Speaker ===");

  // 1. Bring up the Bluetooth host.

  // BLE.begin() powers up the controller + host and sets the GAP device name.
  BTStatus st = BLE.begin(DEVICE_NAME);
  if (!st) {
    halt("BLE.begin", st);
  }

  // 2. Create the roles (engine starts lazily on the first create*()).

  // getAudioController() returns the handle to the shared LE Audio engine.
  audio = BLE.getAudioController();
  if (!audio) {
    halt("getAudioController", BTStatus::InvalidState);
  }

  // onLinkReady() fires on the Bluetooth task once a peer link is encrypted and usable.
  audio.onLinkReady([](uint16_t conn) {
    Serial.printf("Sender connected (link %u)\n", conn);
  });

  // onDisconnected() fires when the link drops (or a connect() attempt fails).
  audio.onDisconnected([](uint16_t conn) {
    Serial.printf("Sender disconnected (link %u)\n", conn);
  });

  // A standalone speaker is a coordinated set of one. Publishing CAS (+ CSIS)
  // lets a phone's CAP initiator discover and drive it like any set member.
  // Without it, only raw BAP works and phones drive through CAP.

  // createCapAcceptor() publishes CAS (+ included CSIS): the CAP identity of this device.
  capAcceptor = audio.createCapAcceptor();
  if (!capAcceptor) {
    halt("createCapAcceptor", BTStatus::InvalidState);
  }

  // setSetSize()/setRank() publish our coordinated-set identity: a set of 1, we are rank 1.
  // Rank is 1-based (1..set size).
  if (!capAcceptor.setSetSize(1)) {
    halt("setSetSize", BTStatus::InvalidState);
  }
  if (!capAcceptor.setRank(1)) {
    halt("setRank", BTStatus::InvalidState);
  }

  // One sink stream: the sender puts mono or stereo on a single CIS.
  // createUnicastServer() publishes PACS + ASCS: it accepts streams a client configures.
  unicastServer = audio.createUnicastServer();
  if (!unicastServer) {
    halt("createUnicastServer", BTStatus::InvalidState);
  }

  // configureMediaSink() helper: 1 stereo-capable sink ASE, media + conversational contexts.
  st = unicastServer.configureMediaSink();
  if (!st) {
    halt("configureMediaSink", st);
  }

  // stream(0) is our only ASE. onConfigured() fires when the client picks a codec for it.
  unicastServer.stream(0).onConfigured(onSinkConfigured);

  // onStarted() fires when the CIS starts streaming. connHandle() is the ACL link it runs on.
  unicastServer.stream(0).onStarted([](BLEAudioStream &s) {
    Serial.printf("Playing (link %u)\n", s.connHandle());
  });

  // onStopped() fires when streaming ends. Reason is the HCI/ASCS code.
  unicastServer.stream(0).onStopped([](BLEAudioStream &, uint8_t reason) {
    Serial.printf("Stopped (reason 0x%02X)\n", reason);
  });

  // Let the sender change our volume over VCP.
  // createVolumeRenderer() publishes VCS: the volume service the peer drives.
  volumeRenderer = audio.createVolumeRenderer();
  if (!volumeRenderer) {
    halt("createVolumeRenderer", BTStatus::InvalidState);
  }

  // setInitialVolume() sets the volume 0..255 advertised before any peer connects.
  st = volumeRenderer.setInitialVolume(160);
  if (!st) {
    halt("setInitialVolume", st);
  }

  // onStateChanged() fires on every volume/mute change, local or from the peer.
  volumeRenderer.onStateChanged([](uint8_t volume, bool muted) {
    Serial.printf("Volume %u%s\n", volume, muted ? " (muted)" : "");
    es8311SetVolume(muted ? 0 : (uint8_t)((volume * 100u) / 255u));
  });

  // 3. Publish all roles into one GATT table (starts no audio by itself).

  // audio.begin() commits the staged roles and freezes the GATT table for this session.
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // 4. Open the local audio output: ES8311 codec + I2S fed by the player.

  if (!es8311Begin()) {
    Serial.println("ES8311 init failed. Check the codec I2C bus");
  }

  // Enable the speaker amplifier
  pinMode(PA_CTRL, OUTPUT);
  digitalWrite(PA_CTRL, HIGH);  // enable the speaker amplifier

  // player.begin() opens I2S output. The LC3 task is still stopped.
  st = player.begin(I2S_SCLK, I2S_LRCK, I2S_DOUT, I2S_MCLK);
  if (!st) {
    halt("player.begin", st);
  }

  // 5. Advertise so a sender can find and connect to us.

  // getAdvertising() returns the handle to the legacy advertising instance.
  BLEAdvertising adv = BLE.getAdvertising();

  // ConnectableScannable = peers may connect to us and read scan-response data.
  adv.setType(BLEAdvType::ConnectableScannable);

  // setName() puts DEVICE_NAME in the advertising payload.
  adv.setName(DEVICE_NAME);

  // setAppearance() publishes the Assigned Number: a standalone speaker.
  adv.setAppearance(BLE_APPEARANCE_AUDIO_SINK_STANDALONE_SPEAKER);

  // Start advertising.
  st = adv.start();
  if (!st) {
    halt("advertising", st);
  }

  Serial.printf("Ready. Connect to \"%s\" to play audio.\n", DEVICE_NAME);
}

void loop() {
  // The sink stream was configured on the Bluetooth task: bind it to the
  // player here. Direction reminder: Rx = we receive audio on this stream.
  if (sinkReady) {
    sinkReady = false;
    player.stopPlayback();  // no-op unless a previous stream was attached
    // attach() binds the Rx stream: the player will decode whatever the client sends on it.
    BTStatus st = player.attach(unicastServer.stream(BLEAudioStream::Direction::Rx));
    if (st) {
      // startPlayback() spawns the LC3 decode task. Playback follows the stream's start/stop.
      st = player.startPlayback();
    }
    if (!st) {
      Serial.printf("player: %s\n", st.toString());
    }
  }
  delay(100);
}
