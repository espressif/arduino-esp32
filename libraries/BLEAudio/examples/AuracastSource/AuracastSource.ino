/*
 * LE Audio Auracast Source (transmitter)
 *
 * Announces a public broadcast and streams the onboard mic to every receiver
 * in range. No connections: receivers just sync to the periodic advertising
 * and join the BIG.
 *
 * Pair with: AuracastSink, or a phone/PC with Auracast.
 * Hardware: ESP32-S31 Function CoreBoard 1 (onboard mic -> ES8311 ADC).
 * The I2S_* pins and es8311Begin() come from the board variant (no include needed).
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>

#ifndef I2S_SCLK
#error "This example requires the I2S pins defined in the board files."
#endif

// --- Tweakables -----------------------------------------------------------
static const char *BROADCAST_NAME = "ESP32 Auracast";  // shown by receivers
static const char *BROADCAST_CODE = "";                // empty = open, else up to 16 chars
static const uint8_t CHANNELS = 2;                     // 1 = mono (1 BIS), 2 = stereo (2 BIS)

// --- LE Audio roles -------------------------------------------------------
BLEAudio audio;
BLEAudioBroadcastSource source;
BLEAudioRecorder recorder;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Auracast Source ===");

  // 1. Bring up the Bluetooth host.

  // BLE.begin() powers up the controller + host and sets the GAP device name.
  BTStatus st = BLE.begin("LE Audio Auracast Source");
  if (!st) {
    halt("BLE.begin", st);
  }

  // 2. Create the broadcast: name + PBA announcement + 48 kHz stereo preset.

  // getAudioController() returns the handle to the shared LE Audio engine.
  audio = BLE.getAudioController();
  if (!audio) {
    halt("getAudioController", BTStatus::InvalidState);
  }

  // createBroadcastSource() makes the transmitter role: no connections, one BIG with 1-2 BISes.
  source = audio.createBroadcastSource();
  if (!source) {
    halt("createBroadcastSource", BTStatus::InvalidState);
  }

  // configureAuracast() helper: Broadcast Name + public broadcast announcement + LC3_48_2_1 preset.
  st = source.configureAuracast(BROADCAST_NAME, CHANNELS);
  if (!st) {
    halt("configureAuracast", st);
  }
  if (BROADCAST_CODE[0] != '\0') {
    // setBroadcastCode() encrypts the BIG. Receivers need the same code. Empty (default) = open.
    st = source.setBroadcastCode(BROADCAST_CODE);
    if (!st) {
      halt("setBroadcastCode", st);
    }
  }

  // onStarted() fires once the BIG is up and the BISes carry audio.
  source.onStarted([] {
    Serial.printf("Broadcasting \"%s\"\n", BROADCAST_NAME);
  });

  // onStopped() fires when the BIG ends. Reason is the HCI code (0x16 after stop()).
  source.onStopped([](uint8_t reason) {
    Serial.printf("Broadcast stopped (reason 0x%02X)\n", reason);
  });

  // 3. Commit the roles (starts no audio by itself).

  // audio.begin() commits the staged roles and freezes the GATT table for this session.
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // 4. Open the local audio input and bind it to the broadcast streams.

  // One BIS carries mono. Two BISes carry left + right as separate mono streams.
  if (!es8311Begin()) {
    Serial.println("ES8311 init failed. Check the codec I2C bus");
  }

  // recorder.begin(bclk, ws, din, mclk) opens I2S input. The LC3 task is still stopped.
  st = recorder.begin(I2S_SCLK, I2S_LRCK, I2S_DIN, I2S_MCLK);
  if (!st) {
    halt("recorder.begin", st);
  }

  // stream(i) is BIS i: 0 = mono/front left, 1 = front right.
  if (CHANNELS == 2) {
    // attach(left, right) feeds two mono Tx streams: left BIS + right BIS.
    st = recorder.attach(source.stream(0), source.stream(1));
  } else {
    // attach(stream) feeds one Tx stream carrying mono (or interleaved stereo).
    st = recorder.attach(source.stream(0));
  }

  if (!st) {
    halt("recorder.attach", st);
  }

  // startCapture() spawns the LC3 encode task. Sending follows the BIG's start/stop.
  st = recorder.startCapture();
  if (!st) {
    halt("recorder.startCapture", st);
  }

  // 5. Go on air.

  // source.start() brings up extended advertising (announcement + name), periodic advertising (BASE) and the BIG.
  st = source.start();
  if (!st) {
    halt("source.start", st);
  }

  Serial.printf("\"%s\" on air (%u channel(s), %s).\n", BROADCAST_NAME, CHANNELS, BROADCAST_CODE[0] ? "encrypted" : "open");
}

void loop() {
  delay(1000);  // nothing to do: the recorder feeds the BIG on its own
}
