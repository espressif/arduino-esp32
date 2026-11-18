/*
 * LE Audio Unicast Receiver with USB microphone output
 *
 * The RECEIVER side of a unicast link: accepts one stream from a phone/PC (or
 * UnicastUSBTransmitter) and forwards the decoded audio to the PC over
 * USB, where this board shows up as a USB microphone. No I2S hardware needed.
 *
 * Pair with: a phone/PC with LE Audio, or UnicastUSBTransmitter.
 * Hardware: any LE Audio-capable board with native USB (e.g. ESP32-S31).
 * Wiring: connect the board's native USB port (not the UART bridge) to the PC
 * and open the "USB Audio" microphone in a recorder (or set it to "listen").
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>
#include "USB.h"
#include "USBAudioCard.h"

// --- Tweakables -----------------------------------------------------------
static const char *DEVICE_NAME = "LE Audio USB Rx";

// USB runs at a fixed rate, so keep LE Audio on the same rate: no resampling,
// but peers without 48 kHz LC3 cannot connect.
static constexpr uint32_t kUsbRateHz = 48000;
static constexpr size_t kScratchFrames = 960;  // >= one 48 kHz / 10 ms stereo frame

// --- LE Audio roles -------------------------------------------------------
BLEAudio audio;
BLEAudioCapAcceptor capAcceptor;
BLEAudioUnicastServer unicastServer;
BLEAudioVolumeRenderer volumeRenderer;
BLEAudioPlayer player;

// USBAudioCard() declares the USB device: 48 kHz, 16-bit, no speaker, stereo mic toward the PC.
USBAudioCard uac(kUsbRateHz, UAC_BPS_16, UAC_SPK_NONE, UAC_MIC_STEREO);

// Set on the Bluetooth task when the client configures our sink stream.
static volatile bool sinkReady = false;

// USB-mic state, touched from the USB event callback.
static volatile bool usbMicOn = false;    // PC opened the mic streaming interface
static volatile uint8_t usbVolume = 255;  // VCP volume 0..255 (255 = unity)
static volatile bool usbMuted = false;

static int16_t usbScratch[kScratchFrames * 2];

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// Decoded audio lands here (codec task): apply VCP volume, widen mono to
// stereo, and hand it to the USB IN endpoint. Never blocks: drops instead of
// stalling when the PC is not reading. `n` counts int16 samples, stereo arrives
// interleaved.
void onPlayerPcm(const int16_t *pcm, size_t n) {
  if (!usbMicOn || pcm == nullptr || n == 0) {
    return;
  }
  // player.channels() = channels of the streaming mix (0 while idle). Anything
  // but stereo goes out dual-mono so the fixed stereo USB descriptor still matches.
  bool stereo = (player.channels() == 2);
  size_t framesTotal = stereo ? (n / 2) : n;
  size_t done = 0;
  while (done < framesTotal) {
    size_t f = framesTotal - done;
    if (f > kScratchFrames) {
      f = kScratchFrames;
    }
    int16_t *out = usbScratch;
    if (stereo) {
      const int16_t *in = pcm + done * 2;
      for (size_t i = 0; i < f * 2; i++) {
        int32_t s = usbMuted ? 0 : ((int32_t)in[i] * usbVolume) / 255;
        *out++ = (int16_t)s;
      }
    } else {
      const int16_t *in = pcm + done;
      for (size_t i = 0; i < f; i++) {
        int32_t s = usbMuted ? 0 : ((int32_t)in[i] * usbVolume) / 255;
        *out++ = (int16_t)s;
        *out++ = (int16_t)s;
      }
    }
    // uac.write() queues PCM into the USB IN endpoint. 0 accepted = PC not reading.
    uint16_t bytes = (uint16_t)(f * 2 * sizeof(int16_t));
    if (uac.write(usbScratch, bytes) == 0) {
      return;  // drop this chunk, keep the codec paced
    }
    done += f;
  }
}

// Runs on the Bluetooth task: just remember the stream is ready.
void onSinkConfigured(BLEAudioStream &stream) {
  // codecConfig() = the LC3 codec the client picked for this stream.
  BLEAudioCodecConfig codec = stream.codecConfig();
  Serial.printf("Sender configured %lu Hz, %u ch\n", (unsigned long)codec.samplingRateHz, codec.channels());
  sinkReady = true;
}

static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  (void)arg;
  if (event_base == ARDUINO_USB_AUDIO_CARD_EVENTS) {
    auto *data = (arduino_usb_audio_card_event_data_t *)event_data;
    // Track whether the PC opened our mic interface. onPlayerPcm() drops PCM while closed.
    if (event_id == ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT && data->interface_enable.interface == UAC_INTERFACE_MIC) {
      usbMicOn = data->interface_enable.enable;
      Serial.printf("USB mic %s\n", usbMicOn ? "opened by PC" : "closed");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Unicast USB Receiver ===");

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

  // One standalone bridge is a coordinated set of one.
  // createCapAcceptor() publishes CAS (+ included CSIS): the CAP identity of this device.
  capAcceptor = audio.createCapAcceptor();
  if (!capAcceptor) {
    halt("createCapAcceptor", BTStatus::InvalidState);
  }

  // setSetSize()/setRank() publish our coordinated-set identity: a set of 1, we are rank 1.
  if (!capAcceptor.setSetSize(1)) {
    halt("setSetSize", BTStatus::InvalidState);
  }
  if (!capAcceptor.setRank(1)) {
    halt("setRank", BTStatus::InvalidState);
  }

  // One sink stream, 48 kHz presets only so PCM always matches the USB rate.
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

  // setSupportedPresets() narrows the published PACs to 48 kHz (plain presets OR together);
  // peers without 48 kHz LC3 then find no usable preset and cannot connect.
  st = unicastServer.setSupportedPresets(BLEAudioCodecPreset::LC3_48_1_1 | BLEAudioCodecPreset::LC3_48_2_1);
  if (!st) {
    halt("setSupportedPresets", st);
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

  // Let the sender change our volume. Applied as software gain in onPlayerPcm().
  // createVolumeRenderer() publishes VCS: the volume service the peer drives.
  volumeRenderer = audio.createVolumeRenderer();
  if (!volumeRenderer) {
    halt("createVolumeRenderer", BTStatus::InvalidState);
  }

  // Unity default: a sender that never touches volume leaves the USB gain at 1.0.
  st = volumeRenderer.setInitialVolume(255);
  if (!st) {
    halt("setInitialVolume", st);
  }

  // onStateChanged() fires on every volume/mute change, local or from the peer.
  volumeRenderer.onStateChanged([](uint8_t volume, bool muted) {
    usbVolume = volume;
    usbMuted = muted;
    Serial.printf("Volume %u%s\n", volume, muted ? " (muted)" : "");
  });

  // 3. Publish all roles into one GATT table (starts no audio by itself).

  // audio.begin() commits the staged roles and freezes the GATT table for this session.
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // 4. Send decoded PCM to USB instead of I2S: no I2S hardware needed.

  // setPCMSink() delivers decoded frames to our callback instead of I2S pins (before begin()).
  st = player.setPCMSink(onPlayerPcm);
  if (!st) {
    halt("setPCMSink", st);
  }

  // player.begin() with no pins is only valid after setPCMSink(). The LC3 task is still stopped.
  st = player.begin();
  if (!st) {
    halt("player.begin", st);
  }

  // uac.onEvent() registers for USB audio events (interface open/close, sample rate).
  uac.onEvent(usbEventCallback);

  // uac.begin() starts the USB audio device (descriptors + endpoints).
  if (!uac.begin()) {
    Serial.println("USB audio begin failed. Check USB-OTG support");
  }

  // USB.onEvent()/USB.begin() register stack events and start the native USB stack.
  USB.onEvent(usbEventCallback);
  USB.begin();

  // 5. Advertise so a sender can find and connect to us.

  // getAdvertising() returns the handle to the legacy advertising instance.
  BLEAdvertising adv = BLE.getAdvertising();

  // ConnectableScannable = peers may connect to us and read scan-response data.
  adv.setType(BLEAdvType::ConnectableScannable);

  // setName() puts DEVICE_NAME in the advertising payload.
  adv.setName(DEVICE_NAME);

  // setAppearance() publishes the Assigned Number: a generic audio sink (USB mic, no speaker).
  adv.setAppearance(BLE_APPEARANCE_GENERIC_AUDIO_SINK);
  st = adv.start();
  if (!st) {
    halt("advertising", st);
  }

  Serial.printf("Ready. Connect to \"%s\", then open the USB microphone on the PC.\n", DEVICE_NAME);
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
