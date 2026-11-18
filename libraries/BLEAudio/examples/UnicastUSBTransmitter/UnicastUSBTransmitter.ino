/*
 * LE Audio Unicast Transmitter fed by a USB speaker
 *
 * The TRANSMITTER side of a unicast link: this board shows up on the PC as a
 * USB speaker, and whatever the PC plays is streamed over LE Audio to a
 * unicast server (UnicastUSBReceiver, UnicastSpeaker, or any
 * BAP server). No I2S hardware needed.
 *
 * Pair with: UnicastUSBReceiver (all-USB loopback with two boards),
 * or UnicastSpeaker to hear PC audio on a DAC speaker.
 * Hardware: any LE Audio-capable board with native USB (e.g. ESP32-S31).
 * Wiring: connect the board's native USB port (not the UART bridge) to the PC
 * and pick it as the playback device.
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>
#include "USB.h"
#include "USBAudioCard.h"

// --- Tweakables -----------------------------------------------------------
// Default target is the matching receiver example (PC -> USB -> LE Audio ->
// USB mic -> PC loopback). Use "LE Audio Speaker" to play on a DAC speaker.
static const char *TARGET_NAME = "LE Audio USB Rx";

// USB runs at a fixed rate, so request the matching 48 kHz LC3 preset.
static constexpr uint32_t kUsbRateHz = 48000;
static constexpr uint32_t kUsbRingBytes = 32768;
static constexpr uint32_t kUsbRingMask = kUsbRingBytes - 1;

// --- LE Audio roles -------------------------------------------------------
BLEAudio audio;
BLEAudioUnicastClient audioClient;
BLEAudioRecorder recorder;
// USBAudioCard() declares the USB device: 48 kHz, 16-bit, stereo speaker from the PC, no mic.
USBAudioCard uac(kUsbRateHz, UAC_BPS_16, UAC_SPK_STEREO, UAC_MIC_NONE);

static BTAddress peerAddress;
// Bluetooth-task callbacks only set these. loop() does the real work.
static volatile bool pendingConnect = false;
static volatile bool pendingStart = false;
static volatile bool pendingStop = false;

// Byte ring: USB task writes stereo int16 PCM, codec task reads it.
// Single producer / single consumer. overruns drop the oldest data.
static uint8_t usbRing[kUsbRingBytes];
static volatile uint32_t usbRingRead = 0;
static volatile uint32_t usbRingWrite = 0;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// PC playback data (USB task). Never blocks: drops oldest audio when full.
void onUsbPlayback(void *data, uint16_t len) {
  if (data == nullptr || len == 0) {
    return;
  }
  const uint8_t *src = (const uint8_t *)data;
  uint32_t w = usbRingWrite;
  uint32_t r = usbRingRead;
  if (len >= kUsbRingBytes) {
    src += len - (kUsbRingBytes - 1);  // keep only the newest window
    len = kUsbRingBytes - 1;
    r = w;
  } else if (len > (kUsbRingBytes - 1 - ((w - r) & kUsbRingMask))) {
    r = w + len - (kUsbRingBytes - 1);  // full: drop oldest, keep latency low
  }
  uint32_t wp = w & kUsbRingMask;
  uint32_t first = kUsbRingBytes - wp;
  if (first > len) {
    first = len;
  }
  memcpy(&usbRing[wp], src, first);
  memcpy(&usbRing[0], src + first, len - first);
  usbRingWrite = w + len;
  usbRingRead = r;
}

// Recorder PCM source (codec task): downmix stereo USB frames to mono.
// Always returns a full buffer. Missing frames are silence.
size_t onUsbPcmSource(int16_t *pcm, size_t maxSamples) {
  uint32_t r = usbRingRead;
  // 4 bytes per stereo frame. availFrames = buffered frames the recorder may consume.
  uint32_t availFrames = ((usbRingWrite - r) & kUsbRingMask) / 4;
  size_t n = maxSamples < availFrames ? maxSamples : (size_t)availFrames;
  for (size_t i = 0; i < n; i++) {
    uint32_t o = (r + i * 4) & kUsbRingMask;
    int16_t left = (int16_t)(usbRing[o] | (usbRing[(o + 1) & kUsbRingMask] << 8));
    int16_t right = (int16_t)(usbRing[(o + 2) & kUsbRingMask] | (usbRing[(o + 3) & kUsbRingMask] << 8));
    pcm[i] = (int16_t)(((int32_t)left + (int32_t)right) / 2);
  }
  if (n < maxSamples) {
    // Underrun: pad with silence so the LC3 frame is always full.
    memset(pcm + n, 0, (maxSamples - n) * sizeof(int16_t));
  }
  usbRingRead = r + (uint32_t)(n * 4);
  // Always report full: the recorder encodes maxSamples every frame.
  return maxSamples;
}

// Scan result (Bluetooth task): remember the sink, connect from loop().
void onDeviceFound(BLEAdvertisedDevice device) {
  // getName() is the GAP/advertised name of the scanned device. Ignore the rest.
  if (device.getName() != TARGET_NAME) {
    return;
  }
  Serial.printf("Found \"%s\"\n", TARGET_NAME);
  // getAddress() is the peer address audio.connect() needs.
  peerAddress = device.getAddress();
  pendingConnect = true;
  // Stop scanning: we found our peer (restarts on link loss).
  BLE.getScan().stop();
}

// Link is encrypted and its GATT table is known: start the audio setup.
void onLinkReady(uint16_t connHandle) {
  Serial.printf("Link %u ready, opening the stream...\n", connHandle);
  // connectAndStart() discovers the sink's PACS/ASCS on this link, then runs
  // the ASCS setup (Config Codec/QoS, Enable, Receiver Start Ready) for our streams.
  BTStatus st = audioClient.connectAndStart(connHandle);
  if (!st) {
    Serial.printf("connectAndStart: %s\n", st.toString());
  }
}

void onLinkLost(uint16_t connHandle) {
  Serial.printf("Link %u lost, scanning again\n", connHandle);
  pendingStop = true;
  // start(0) scans until stopped. 0 = no timeout.
  BLE.getScan().start(0);
}

static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  (void)arg;
  (void)event_data;
  if (event_base == ARDUINO_USB_EVENTS) {
    if (event_id == ARDUINO_USB_STARTED_EVENT) {
      Serial.println("USB plugged");
    } else if (event_id == ARDUINO_USB_STOPPED_EVENT) {
      Serial.println("USB unplugged");
    }
  } else if (event_base == ARDUINO_USB_AUDIO_CARD_EVENTS) {
    auto *data = (arduino_usb_audio_card_event_data_t *)event_data;
    // Track whether the PC opened our speaker interface (Informational. The ring absorbs gaps).
    if (event_id == ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT && data->interface_enable.interface == UAC_INTERFACE_SPK) {
      Serial.printf("USB speaker %s\n", data->interface_enable.enable ? "opened by PC" : "closed");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Unicast USB Speaker ===");

  // 1. Bring up the Bluetooth host.

  // BLE.begin() powers up the controller + host and sets the GAP device name.
  BTStatus st = BLE.begin("LE Audio USB Tx");
  if (!st) {
    halt("BLE.begin", st);
  }

  // 2. Create the roles (engine starts lazily on the first create*()).
  // Fixed 48 kHz to match USB (no resampling): one stream we send (Tx).

  // getAudioController() returns the handle to the shared LE Audio engine.
  audio = BLE.getAudioController();
  if (!audio) {
    halt("getAudioController", BTStatus::InvalidState);
  }

  // onLinkReady() fires on the Bluetooth task once a peer link is encrypted and usable.
  audio.onLinkReady(onLinkReady);

  // onDisconnected() fires when the link drops (or a connect() attempt fails).
  audio.onDisconnected(onLinkLost);

  // createUnicastClient() makes the initiator side: it discovers the sink and sets up streams on it.
  audioClient = audio.createUnicastClient();
  if (!audioClient) {
    halt("createUnicastClient", BTStatus::InvalidState);
  }

  // setPreset() fixes the LC3 codec + QoS of our streams: 48 kHz, 10 ms frames (matches USB).
  // setSinkStreams(1) requests one stream we send, per sink (capped by the sink's ASE count).
  st = audioClient.setPreset(BLEAudioCodecPreset::LC3_48_2_1);
  if (!st) {
    halt("setPreset", st);
  }
  st = audioClient.setSinkStreams(1);
  if (!st) {
    halt("setSinkStreams", st);
  }

  // onStarted() fires once every requested stream is streaming.
  audioClient.onStarted([] {
    Serial.println("Streaming to the sink");
    pendingStart = true;
  });

  // onStopped() fires once all streams are released (after stop() or a failure).
  audioClient.onStopped([] {
    Serial.println("Stream released");
    pendingStop = true;
  });

  // onError() reports the failed ASCS setup step plus the sink's response/reason codes.
  audioClient.onError([](BLEAudioUnicastClient::Step step, uint8_t rsp, uint8_t reason) {
    Serial.printf("Stream setup failed at step %u (0x%02X/0x%02X)\n", (unsigned)step, rsp, reason);
  });

  // 3. Publish the roles (starts no audio by itself).

  // audio.begin() commits the staged roles and freezes the GATT table for this session.
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // 4. Feed the recorder from USB instead of I2S: no I2S hardware needed.

  // setPCMSource() takes PCM from our callback instead of I2S pins (before begin()).
  st = recorder.setPCMSource(onUsbPcmSource);
  if (!st) {
    halt("setPCMSource", st);
  }

  // recorder.begin() with no pins is only valid after setPCMSource(). The LC3 task is still stopped.
  st = recorder.begin();
  if (!st) {
    halt("recorder.begin", st);
  }

  // uac.onEvent() registers for USB audio events (interface open/close, sample rate).
  uac.onEvent(usbEventCallback);

  // uac.onData() registers the callback receiving each PC playback packet (USB task).
  uac.onData(onUsbPlayback);

  // uac.begin() starts the USB audio device (descriptors + endpoints).
  if (!uac.begin()) {
    Serial.println("USB audio begin failed. Check USB-OTG support");
  }

  // USB.onEvent()/USB.begin() register stack events and start the native USB stack.
  USB.onEvent(usbEventCallback);
  USB.begin();

  // 5. Find the sink to send to.

  // getScan() returns the handle to the scanner.
  BLEScan scan = BLE.getScan();

  // Active scan: also ask each advertiser for scan-response data.
  scan.setActiveScan(true);

  // onResult() fires per advertising report, on the Bluetooth task.
  scan.onResult(onDeviceFound);

  // start(0) scans until stopped. 0 = no timeout.
  scan.start(0);
  Serial.printf("Scanning for \"%s\"...\n", TARGET_NAME);
}

void loop() {
  // Connect from loop(): the scan callback runs on the Bluetooth task.
  if (pendingConnect) {
    pendingConnect = false;
    // audio.connect() opens the ACL through the audio engine (not BLEClient):
    // the engine then encrypts the link and discovers the peer's GATT table.
    if (!audio.connect(peerAddress)) {
      Serial.println("connect failed, scanning again");
      BLE.getScan().start(0);
    }
  }

  // Direction reminder: Tx = we send audio on this stream.
  if (pendingStart) {
    pendingStart = false;
    // attach() binds the Tx stream: the recorder will encode USB PCM into it.
    BTStatus st = recorder.attach(audioClient.stream(BLEAudioStream::Direction::Tx));
    if (st) {
      // startCapture() spawns the LC3 encode task. Sending follows the stream's start/stop.
      st = recorder.startCapture();
    }
    if (!st) {
      Serial.printf("recorder: %s\n", st.toString());
    }
  }

  if (pendingStop) {
    pendingStop = false;
    // stopCapture() stops encoding. The attachment is kept so startCapture() can resume.
    recorder.stopCapture();
  }
  delay(100);
}
