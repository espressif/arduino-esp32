// BLE dual-role validation test — DUT0 (connects first)
// Both DUTs run as server + client. Each creates a GATT server, advertises,
// scans for the peer, then connects as a client to read the peer's
// characteristic. DUT0 connects first; DUT1 waits before connecting.

#include <Arduino.h>
#include <BLE.h>

static const BLEUUID SVC_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static const BLEUUID CHR_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

String myName;
String peerName;
BTAddress peerAddr;
volatile bool foundPeer = false;
volatile int reverseConns = 0;
volatile bool readyForSequential = false;
volatile bool sequentialConnected = false;
volatile bool sequentialDisconnected = false;
BLEConnInfo reverseInfo;
BLEConnInfo sequentialInfo;

void readNames() {
  Serial.println("[DUT0] Device ready for names");
  Serial.println("[DUT0] Send names:");
  String input;
  while (input.length() == 0) {
    if (Serial.available()) {
      input = Serial.readStringUntil('\n');
      input.trim();
    }
    delay(100);
  }
  int comma = input.indexOf(',');
  myName = input.substring(0, comma);
  peerName = input.substring(comma + 1);
  Serial.printf("[DUT0] My name: %s, Peer: %s\n", myName.c_str(), peerName.c_str());
}

bool waitForToken(const char *token, uint32_t timeoutMs) {
  String line;
  unsigned long deadline = millis() + timeoutMs;
  while (millis() < deadline) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\n') {
        line.trim();
        if (line == token) {
          return true;
        }
        line = "";
      } else {
        line += c;
      }
    }
    delay(10);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  readNames();

  BTStatus status = BLE.begin(myName);
  if (!status) {
    Serial.printf("[DUT0] Init FAILED: %s\n", status.toString());
    return;
  }
  Serial.println("[DUT0] Init OK");

  // --- Server role ---
  BLEServer server = BLE.createServer();
  server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
    if (readyForSequential) {
      sequentialConnected = true;
      sequentialInfo = conn;
      Serial.printf("[DUT0] Sequential client connected handle=%u\n", (unsigned)conn.getHandle());
    } else {
      reverseConns++;
      reverseInfo = conn;
      Serial.printf("[DUT0] Reverse client connected to our server count=%d handle=%u\n", reverseConns, (unsigned)conn.getHandle());
    }
  });
  server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
    if (readyForSequential) {
      if (conn.getHandle() == sequentialInfo.getHandle()) {
        sequentialDisconnected = true;
      }
      Serial.printf("[DUT0] Sequential client disconnected handle=%u reason=%u\n", (unsigned)conn.getHandle(), (unsigned)reason);
    } else {
      if (reverseConns > 0) {
        reverseConns--;
      }
      Serial.printf("[DUT0] Reverse client disconnected handle=%u reason=%u remaining=%d\n", (unsigned)conn.getHandle(), (unsigned)reason, reverseConns);
    }
  });
  BLEService svc = server.createService(SVC_UUID);
  BLECharacteristic chr = svc.createCharacteristic(CHR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
  chr.setValue("DUT0 data");
  server.start();
  Serial.println("[DUT0] Server started");

  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(SVC_UUID);
  adv.start();
  Serial.println("[DUT0] Advertising started");

  delay(2000);

  // --- Client role (DUT0 goes first) ---
  BLEScan scan = BLE.getScan();
  scan.onResult([&](BLEAdvertisedDevice dev) {
    if (dev.getName() == peerName) {
      peerAddr = dev.getAddress();
      foundPeer = true;
      BLE.getScan().stop();
    }
  });

  for (int attempt = 1; attempt <= 5 && !foundPeer; attempt++) {
    Serial.printf("[DUT0] Scanning for peer (attempt %d)...\n", attempt);
    scan.start(5000);
    if (!foundPeer) {
      delay(1000);
    }
  }

  if (!foundPeer) {
    Serial.println("[DUT0] Peer not found");
    return;
  }
  Serial.println("[DUT0] Found peer");

  BLEClient client = BLE.createClient();
  status = client.connect(peerAddr);
  if (!status) {
    Serial.printf("[DUT0] Connect FAILED: %s\n", status.toString());
    return;
  }
  Serial.println("[DUT0] Connected as client to DUT1");

  BLERemoteService remoteSvc = client.getService(SVC_UUID);
  if (!remoteSvc) {
    Serial.println("[DUT0] Remote service not found");
    client.disconnect();
    return;
  }

  BLERemoteCharacteristic remoteChr = remoteSvc.getCharacteristic(CHR_UUID);
  if (!remoteChr) {
    Serial.println("[DUT0] Remote characteristic not found");
    client.disconnect();
    return;
  }

  String val = remoteChr.readValue();
  Serial.printf("[DUT0] Remote value: %s\n", val.c_str());

  // Keep our client link up while DUT1 opens one or more client links back to
  // our server. This is the simultaneous client+server case. DUT1 holds its
  // reverse links until the harness confirms this sample (REVERSE_GO), so no
  // fixed delay can drop a link under the lookup below.
  Serial.println("[DUT0] Ready for reverse connect");
  const bool isBluedroid = strcmp(BLE.getStackName(), "Bluedroid") == 0;
  unsigned long reverseWait = millis();
  if (isBluedroid) {
    while (reverseConns == 0 && millis() - reverseWait < 20000) {
      delay(100);
    }
    delay(500);  // let a second reverse client arrive if DUT1 opens one
  }

  BTAddress lookupAddr = (reverseConns > 0) ? reverseInfo.getAddress() : peerAddr;
  std::vector<BLEConnInfo> conns = BLE.getConnections(lookupAddr);
  bool lookupCentral = !conns.empty() && conns[0].isCentral();
  Serial.printf("[DUT0] Lookup stack=%s conns=%u reverseConns=%d central=%d\n", BLE.getStackName(), (unsigned)conns.size(), reverseConns, (int)lookupCentral);

  // The lookup above sampled while DUT1 holds the reverse links open (it only
  // tears them down after the harness sends REVERSE_GO), so the sample cannot
  // race a teardown.
  if (!waitForToken("DISC_GO", 20000)) {
    Serial.println("[DUT0] Timeout waiting for disconnect token");
    return;
  }

  client.disconnect();
  unsigned long cleanupWait = millis();
  while (reverseConns > 0 && millis() - cleanupWait < 5000) {
    delay(50);
  }
  readyForSequential = true;
  Serial.println("[DUT0] Ready for sequential client");

  unsigned long sequentialWait = millis();
  while (!sequentialConnected && millis() - sequentialWait < 20000) {
    delay(50);
  }
  if (sequentialConnected) {
    unsigned long disconnectWait = millis();
    while (!sequentialDisconnected && millis() - disconnectWait < 10000) {
      delay(50);
    }
  }
  Serial.printf("[DUT0] Sequential server ok=%d disconnected=%d\n", (int)sequentialConnected, (int)sequentialDisconnected);
  Serial.println("[DUT0] Dual role OK");
}

void loop() {
  delay(1000);
}
