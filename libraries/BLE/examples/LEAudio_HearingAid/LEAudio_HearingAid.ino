/*
 * LE Audio -- Hearing Aid (Hearing Access Service device)
 *
 * Publishes a Hearing Access Service (HAS) as a binaural hearing aid exposing a
 * short list of named "presets" (listening programs). A HAS controller -- a
 * phone app or a second board running the controller role -- discovers the
 * service, reads the presets, and switches the active one; this sketch reacts
 * to those switches and prints the active program.
 *
 * The device is also a CAP acceptor with a unicast server + volume renderer, so
 * the same connection can carry a call/media stream and volume control -- the
 * usual shape of a real hearing aid. HAS itself only manages the preset list.
 *
 * Requires the HAS server (CONFIG_BT_HAS with a non-zero preset count); on
 * builds without it the sketch self-reports and idles.
 *
 * Callback style: a named function for preset selection, a lambda for volume.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_AUDIO_HAS_SUPPORTED

static const char *DEVICE_NAME = "LE Audio Hearing Aid";

// Preset list (index -> program name). Index 1 is the initial active preset.
struct Program {
  uint8_t index;
  const char *name;
};
static const Program PROGRAMS[] = {
  {1, "Universal"},
  {2, "Outdoor"},
  {3, "Restaurant"},
  {4, "Music"},
};

BLEAudio audio;
BLEAudioCapAcceptor capAcceptor;
BLEAudioUnicastServer unicastServer;
BLEAudioVolumeRenderer volumeRenderer;
BLEAudioHearingAidDevice hearingAid;

const char *programName(uint8_t index) {
  for (const auto &p : PROGRAMS) {
    if (p.index == index) {
      return p.name;
    }
  }
  return "?";
}

void onPresetSelected(uint8_t index, bool sync) {
  Serial.printf("[HAS] controller selected preset %u (%s)%s\n", index, programName(index), sync ? " [sync]" : "");
  // The engine activates the requested preset; reflect it locally if desired.
}

void haltWith(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== LE Audio Hearing Aid (HAS device) ===");

  BTStatus st = BLE.begin(DEVICE_NAME);
  if (!st) {
    haltWith("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    haltWith("audio.begin", st);
  }

  // Underlying acceptor roles: CAS + PACS/ASCS + VCS. A binaural aid forms a
  // coordinated set of two, so advertise a set size of 2 (rank 1 = this ear).
  capAcceptor = audio.createCapAcceptor();
  capAcceptor.setSetSize(2).setRank(1);

  unicastServer = audio.createUnicastServer();
  unicastServer.setSinkStreams(1).setSourceStreams(0).setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);

  volumeRenderer = audio.createVolumeRenderer();
  volumeRenderer.setInitialVolume(128).onStateChanged([](uint8_t volume, bool muted) {
    Serial.printf("[VCP] volume=%u muted=%d\n", volume, muted);
  });

  // The Hearing Access Service itself: a binaural aid with the preset list.
  hearingAid = audio.createHearingAidDevice();
  hearingAid.setType(BLEHearingAidType::Binaural).setPresetSync(true).onPresetSelected(onPresetSelected);
  for (const auto &p : PROGRAMS) {
    hearingAid.addPreset(p.index, p.name);
  }

  // Single coordinated commit publishes CAS + PACS/ASCS + VCS + HAS.
  st = audio.start();
  if (!st) {
    haltWith("audio.start", st);
  }

  hearingAid.setActivePreset(1);

  BLEAdvertising adv = BLE.getAdvertising();
  adv.setName(DEVICE_NAME);
  st = adv.start();
  if (!st) {
    haltWith("advertising", st);
  }

  Serial.printf("Ready. Advertising HAS with %u presets; active = 1 (%s).\n", (unsigned)(sizeof(PROGRAMS) / sizeof(PROGRAMS[0])), programName(1));
  Serial.println("Connect a HAS controller to read the presets and switch programs.");
}

void loop() {
  delay(2000);
  Serial.printf("Active preset: %u (%s)\n", hearingAid.getActivePreset(), programName(hearingAid.getActivePreset()));
}

#else  // !BLE_AUDIO_HAS_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_HearingAid requires the HAS server (CONFIG_BT_HAS with a non-zero preset count) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_HAS_SUPPORTED */
