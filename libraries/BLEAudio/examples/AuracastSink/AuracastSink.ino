/*
 * LE Audio Auracast Sink (receiver)
 *
 * Scans for a broadcast, syncs to it and plays the audio on the speaker.
 * A phone app can also steer this sink remotely (BASS Scan Delegator).
 *
 * Pair with: AuracastSource, or any Auracast transmitter / phone.
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
static const char *TARGET_NAME = "ESP32 Auracast";  // empty = join the first broadcast found
static const char *BROADCAST_CODE = "";             // same code as the source, if encrypted
static const uint8_t CHANNELS = 2;                  // 1 = mono (1 BIS), 2 = stereo (2 BIS)

// --- LE Audio roles -------------------------------------------------------
BLEAudio audio;
BLEAudioBroadcastSink sink;
BLEAudioPlayer player;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Auracast Sink ===");

  // 1. Bring up the Bluetooth host.

  // BLE.begin() powers up the controller + host and sets the GAP device name.
  BTStatus st = BLE.begin("LE Audio Auracast Sink");
  if (!st) {
    halt("BLE.begin", st);
  }

  // 2. Create the receiver: scan for TARGET_NAME and auto-join it.

  // getAudioController() returns the handle to the shared LE Audio engine.
  audio = BLE.getAudioController();
  if (!audio) {
    halt("getAudioController", BTStatus::InvalidState);
  }

  // createBroadcastSink() makes the receiver role: scan + PA sync + BIG sync (BASS Scan Delegator included).
  sink = audio.createBroadcastSink();
  if (!sink) {
    halt("createBroadcastSink", BTStatus::InvalidState);
  }

  // configureAuracast() helper: auto-join broadcasts named targetName with 1-2 BISes, Media context.
  st = sink.configureAuracast(TARGET_NAME, CHANNELS);
  if (!st) {
    halt("configureAuracast", st);
  }
  if (BROADCAST_CODE[0] != '\0') {
    // setBroadcastCode() is the decode key for encrypted broadcasts. empty = open only.
    st = sink.setBroadcastCode(BROADCAST_CODE);
    if (!st) {
      halt("setBroadcastCode", st);
    }
  }

  // onSourceFound() fires per matching advertising report while scanning (repeats, so keep it short).
  sink.onSourceFound([](const BLEAudioBroadcastSourceInfo &info) {
    Serial.printf("Found \"%s\" (%d dBm)\n", info.name.c_str(), info.rssi);
  });

  // onStarted() fires once the BIG is joined and the streams carry audio.
  sink.onStarted([] {
    Serial.println("Joined: playing");
  });

  // onStopped() fires when we leave the BIG (stop(), source gone, or sync lost). Reason is the HCI code.
  sink.onStopped([](uint8_t reason) {
    Serial.printf("Left (reason 0x%02X)\n", reason);
  });

  // onSyncFailed() fires when the BIG cannot be joined: NotFound (no BIS fits our PAC) or
  // PermissionDenied (encrypted and no/wrong Broadcast Code).
  sink.onSyncFailed([](BTStatus status) {
    Serial.printf("Join failed: %s\n", status.toString());
  });

  // 3. Commit the roles (starts no audio by itself).

  // audio.begin() commits the staged roles (PACS/BASS) and freezes the GATT table for this session.
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // 4. Open the local audio output and bind it to the broadcast streams.

  // One BIS carries mono. Two BISes carry left + right as separate mono streams.
  if (!es8311Begin()) {
    Serial.println("ES8311 init failed. Check the codec I2C bus");
  }

  // Enable the speaker amplifier
  pinMode(PA_CTRL, OUTPUT);
  digitalWrite(PA_CTRL, HIGH);

  // player.begin() opens I2S output. The LC3 task is still stopped.
  st = player.begin(I2S_SCLK, I2S_LRCK, I2S_DOUT, I2S_MCLK);
  if (!st) {
    halt("player.begin", st);
  }

  // stream(i) is BIS i in join order: 0 = mono/front left, 1 = front right.
  if (CHANNELS == 2) {
    // attach(left, right) plays two mono Rx streams: left BIS + right BIS.
    st = player.attach(sink.stream(0), sink.stream(1));
  } else {
    // attach(stream) plays one Rx stream carrying mono (or interleaved stereo).
    st = player.attach(sink.stream(0));
  }

  if (!st) {
    halt("player.attach", st);
  }

  // startPlayback() spawns the LC3 decode task. Playback follows the BIG's start/stop.
  st = player.startPlayback();
  if (!st) {
    halt("player.startPlayback", st);
  }

  // 5. Stay connectable so a phone can steer us, and start scanning.

  // getAdvertising() returns the handle to the legacy advertising instance.
  BLEAdvertising adv = BLE.getAdvertising();

  // ConnectableScannable = a phone can connect (to reach BASS) and read scan-response data.
  adv.setType(BLEAdvType::ConnectableScannable);

  // setName() puts the name in the advertising payload.
  adv.setName("LE Audio Auracast Sink");

  // setAppearance() publishes the Assigned Number: a standalone speaker.
  adv.setAppearance(BLE_APPEARANCE_AUDIO_SINK_STANDALONE_SPEAKER);

  // Start advertising.
  st = adv.start();
  if (!st) {
    halt("advertising", st);
  }

  // sink.start() begins scanning. PA sync and BIG join then happen automatically.
  st = sink.start();
  if (!st) {
    halt("sink.start", st);
  }

  Serial.printf("Scanning for \"%s\"...\n", TARGET_NAME[0] ? TARGET_NAME : "any broadcast");
}

void loop() {
  delay(1000);  // nothing to do: the player follows the broadcast on its own
}
