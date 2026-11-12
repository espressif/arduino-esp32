// BLE dual-role validation test — DUT1 (connects second)
// Both DUTs run as server + client. Each creates a GATT server, advertises,
// scans for the peer, then connects as a client to read the peer's
// characteristic. DUT1 waits for DUT0 to finish its client phase first.

#include <Arduino.h>
#include <BLE.h>
#include <algorithm>

static const BLEUUID SVC_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static const BLEUUID CHR_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

String myName;
String peerName;
BTAddress peerAddr;
volatile bool foundPeer = false;
volatile bool peerConnected = false;
BLEConnInfo peerConnInfo;

void readNames() {
  Serial.println("[DUT1] Device ready for names");
  Serial.println("[DUT1] Send names:");
  String input;
  while (input.length() == 0) {
    if (Serial.available()) {
      input = Serial.readStringUntil('\n');
      input.trim();
    }
    delay(100);
  }
  int comma = input.indexOf(',');
  peerName = input.substring(0, comma);
  myName = input.substring(comma + 1);
  Serial.printf("[DUT1] My name: %s, Peer: %s\n", myName.c_str(), peerName.c_str());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  readNames();

  BTStatus status = BLE.begin(myName);
  if (!status) {
    Serial.printf("[DUT1] Init FAILED: %s\n", status.toString());
    return;
  }
  Serial.println("[DUT1] Init OK");

  // --- Server role ---
  BLEServer server = BLE.createServer();
  server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
    peerConnected = true;
    peerConnInfo = conn;
    Serial.printf("[DUT1] Peer connected to our server handle=%u\n", (unsigned)conn.getHandle());
  });
  server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
    peerConnected = false;
    Serial.printf("[DUT1] Peer disconnected from our server handle=%u\n", (unsigned)conn.getHandle());
  });

  BLEService svc = server.createService(SVC_UUID);
  BLECharacteristic chr = svc.createCharacteristic(CHR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
  chr.setValue("DUT1 data");
  server.start();
  Serial.println("[DUT1] Server started");

  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(SVC_UUID);
  adv.start();
  Serial.println("[DUT1] Advertising started");

  delay(2000);

  // --- Scan for peer address ---
  BLEScan scan = BLE.getScan();
  scan.onResult([&](BLEAdvertisedDevice dev) {
    if (dev.getName() == peerName) {
      peerAddr = dev.getAddress();
      foundPeer = true;
      BLE.getScan().stop();
    }
  });

  for (int attempt = 1; attempt <= 5 && !foundPeer; attempt++) {
    Serial.printf("[DUT1] Scanning for peer (attempt %d)...\n", attempt);
    scan.start(5000);
    if (!foundPeer) {
      delay(1000);
    }
  }

  if (!foundPeer) {
    Serial.println("[DUT1] Peer not found");
    return;
  }
  Serial.println("[DUT1] Found peer");

  // --- Wait for DUT0's client link, then open one or two client links back ---
  Serial.println("[DUT1] Waiting for DUT0 client...");
  unsigned long waitStart = millis();
  while (!peerConnected && (millis() - waitStart) < 30000) {
    delay(100);
  }

  if (!peerConnected) {
    Serial.println("[DUT1] Timeout waiting for DUT0 client");
    return;
  }
  Serial.println("[DUT1] DUT0 client connected, opening reverse client");
  delay(1000);

  BLEClient reverse1 = BLE.createClient();
  BTStatus rs1 = reverse1.connect(peerAddr);
  int reverseClients = rs1 ? 1 : 0;
  if (rs1) {
    Serial.println("[DUT1] Reverse client 1 connected");
  } else {
    Serial.printf("[DUT1] Reverse client 1 failed: %s\n", rs1.toString());
  }

  BLEClient reverse2 = BLE.createClient();
  BTStatus rs2 = reverseClients ? reverse2.connect(peerAddr) : BTStatus::Fail;
  if (rs2) {
    reverseClients++;
    Serial.println("[DUT1] Reverse client 2 connected");
  } else if (reverseClients) {
    Serial.printf("[DUT1] Reverse client 2 failed: %s\n", rs2.toString());
  }

  std::vector<BLEConnInfo> conns = BLE.getConnections(peerAddr);
  int clientEntries = 0;
  bool hasServer = false;
  for (const auto &ci : conns) {
    if (ci.isCentral()) {
      clientEntries++;
    }
    if (ci.getHandle() == peerConnInfo.getHandle()) {
      hasServer = true;
    }
  }
  Serial.printf(
    "[DUT1] Lookup stack=%s conns=%u clients=%d hasServer=%d\n", BLE.getStackName(), (unsigned)conns.size(), clientEntries, (int)hasServer
  );

  // Give DUT0 time to sample its table, then tear the reverse links down.
  delay(2000);
  if (reverseClients > 0) {
    reverse2.disconnect();
    reverse1.disconnect();
  }
  Serial.println("[DUT1] Dual role OK");
}

void loop() {
  delay(1000);
}
