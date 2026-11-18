/*
 * LE Audio Wireless Microphone
 *
 * Captures the onboard mic and sends it over one unicast stream to a speaker.
 * This is the client side: it scans for the speaker, connects, and asks the
 * speaker to open one sink stream, then feeds it from the LC3 recorder.
 *
 * Pair with: UnicastSpeaker, or any BAP unicast server (e.g. phone/PC).
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
static const char *TARGET_NAME = "LE Audio Speaker";  // speaker to connect to

// --- LE Audio roles -------------------------------------------------------
BLEAudio audio;
BLEAudioUnicastClient audioClient;
BLEAudioRecorder recorder;

static BTAddress peerAddress;
// Bluetooth-task callbacks only set these. loop() does the real work.
static volatile bool pendingConnect = false;
static volatile bool pendingStart = false;
static volatile bool pendingStop = false;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// Scan result (Bluetooth task): remember the speaker, connect from loop().
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
  // connectAndStart() discovers the server's PACS/ASCS on this link, then runs
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

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Wireless Microphone ===");

  // 1. Bring up the Bluetooth host.

  // BLE.begin() powers up the controller + host and sets the GAP device name.
  BTStatus st = BLE.begin("LE Audio Wireless Mic");
  if (!st) {
    halt("BLE.begin", st);
  }

  // 2. Create the roles (engine starts lazily on the first create*()).
  // Defaults fit a mic: 1 stream we send (Tx), 16 kHz LC3, Media context.

  // getAudioController() returns the handle to the shared LE Audio engine.
  audio = BLE.getAudioController();
  if (!audio) {
    halt("getAudioController", BTStatus::InvalidState);
  }

  // onLinkReady() fires on the Bluetooth task once a peer link is encrypted and usable.
  audio.onLinkReady(onLinkReady);

  // onDisconnected() fires when the link drops (or a connect() attempt fails).
  audio.onDisconnected(onLinkLost);

  // createUnicastClient() makes the initiator side: it discovers the server and sets up streams on it.
  audioClient = audio.createUnicastClient();
  if (!audioClient) {
    halt("createUnicastClient", BTStatus::InvalidState);
  }

  // onStarted() fires once every requested stream is streaming.
  audioClient.onStarted([] {
    Serial.println("Streaming to the speaker");
    pendingStart = true;
  });

  // onStopped() fires once all streams are released (after stop() or a failure).
  audioClient.onStopped([] {
    Serial.println("Stream released");
    pendingStop = true;
  });

  // onError() reports the failed ASCS setup step plus the server's response/reason codes.
  audioClient.onError([](BLEAudioUnicastClient::Step step, uint8_t rsp, uint8_t reason) {
    Serial.printf("Stream setup failed at step %u (0x%02X/0x%02X)\n", (unsigned)step, rsp, reason);
  });

  // 3. Publish the roles (starts no audio by itself).

  // audio.begin() commits the staged roles and freezes the GATT table for this session.
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // 4. Open the local audio input: ES8311 ADC + I2S fed into the recorder.

  if (!es8311Begin()) {
    Serial.println("ES8311 init failed. Check the codec I2C bus");
  }

  // recorder.begin() opens I2S input. The LC3 task is still stopped.
  st = recorder.begin(I2S_SCLK, I2S_LRCK, I2S_DIN, I2S_MCLK);
  if (!st) {
    halt("recorder.begin", st);
  }

  // 5. Find the speaker to send to.

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
    // attach() binds the Tx stream: the recorder will encode mic PCM into it.
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
