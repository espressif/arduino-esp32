// BLE dual-role validation test — DUT1 (connects second)
// Both DUTs run as server + client. Each creates a GATT server, advertises,
// scans for the peer, then connects as a client to read the peer's
// characteristic. DUT1 waits for DUT0 to finish its client phase first.

#include <Arduino.h>
#include <BLE.h>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"

String myName;
String peerName;
BTAddress peerAddr;
volatile bool foundPeer = false;
volatile bool peerWasConnected = false;

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
    Serial.println("[DUT1] Peer connected to our server");
  });
  server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
    peerWasConnected = true;
    Serial.println("[DUT1] Peer disconnected from our server");
  });

  BLEService svc = server.createService(BLEUUID(SERVICE_UUID));
  BLECharacteristic chr = svc.createCharacteristic(BLEUUID(CHAR_UUID), BLEProperty::Read, BLEPermissions::OpenRead);
  chr.setValue("DUT1 data");
  server.start();
  Serial.println("[DUT1] Server started");

  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(BLEUUID(SERVICE_UUID));
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

  // --- Wait for DUT0 to finish its client phase ---
  Serial.println("[DUT1] Waiting for DUT0 to connect and disconnect first...");
  unsigned long waitStart = millis();
  while (!peerWasConnected && (millis() - waitStart) < 30000) {
    delay(100);
  }

  if (!peerWasConnected) {
    Serial.println("[DUT1] Timeout waiting for DUT0 client phase");
    return;
  }
  Serial.println("[DUT1] DUT0 finished, now connecting as client");
  delay(1000);

  // --- Client role (DUT1 goes second) ---
  BLEClient client = BLE.createClient();
  status = client.connect(peerAddr);
  if (!status) {
    Serial.printf("[DUT1] Connect FAILED: %s\n", status.toString());
    return;
  }
  Serial.println("[DUT1] Connected as client to DUT0");

  BLERemoteService remoteSvc = client.getService(BLEUUID(SERVICE_UUID));
  if (!remoteSvc) {
    Serial.println("[DUT1] Remote service not found");
    client.disconnect();
    return;
  }

  BLERemoteCharacteristic remoteChr = remoteSvc.getCharacteristic(BLEUUID(CHAR_UUID));
  if (!remoteChr) {
    Serial.println("[DUT1] Remote characteristic not found");
    client.disconnect();
    return;
  }

  String val = remoteChr.readValue();
  Serial.printf("[DUT1] Remote value: %s\n", val.c_str());

  client.disconnect();
  Serial.println("[DUT1] Dual role OK");
}

void loop() {
  delay(1000);
}
