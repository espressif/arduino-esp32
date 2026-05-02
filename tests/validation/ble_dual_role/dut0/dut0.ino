// BLE dual-role validation test — DUT0 (connects first)
// Both DUTs run as server + client. Each creates a GATT server, advertises,
// scans for the peer, then connects as a client to read the peer's
// characteristic. DUT0 connects first; DUT1 waits before connecting.

#include <Arduino.h>
#include <BLE.h>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"

String myName;
String peerName;
BTAddress peerAddr;
volatile bool foundPeer = false;

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
  BLEService svc = server.createService(BLEUUID(SERVICE_UUID));
  BLECharacteristic chr = svc.createCharacteristic(BLEUUID(CHAR_UUID), BLEProperty::Read, BLEPermissions::OpenRead);
  chr.setValue("DUT0 data");
  server.start();
  Serial.println("[DUT0] Server started");

  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(BLEUUID(SERVICE_UUID));
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

  BLERemoteService remoteSvc = client.getService(BLEUUID(SERVICE_UUID));
  if (!remoteSvc) {
    Serial.println("[DUT0] Remote service not found");
    client.disconnect();
    return;
  }

  BLERemoteCharacteristic remoteChr = remoteSvc.getCharacteristic(BLEUUID(CHAR_UUID));
  if (!remoteChr) {
    Serial.println("[DUT0] Remote characteristic not found");
    client.disconnect();
    return;
  }

  String val = remoteChr.readValue();
  Serial.printf("[DUT0] Remote value: %s\n", val.c_str());

  client.disconnect();
  Serial.println("[DUT0] Dual role OK");
}

void loop() {
  delay(1000);
}
