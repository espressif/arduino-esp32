/*
 * BLE Isochronous Channels -- CIS Central
 *
 * Finds the ISO_CisPeripheral example, opens an ACL link to it and creates a
 * bidirectional Connected Isochronous Stream (CIS) on that link. Every SDU
 * interval (10 ms) it sends its counter and receives the Peripheral's answer,
 * then prints per-second statistics. If the CIS or the link drops, it scans
 * and connects again.
 *
 * The channel numbers the SDUs itself: send(sdu, len) uses the next sequence
 * number, so the sketch only has to call it once per SDU interval.
 *
 * Pair with the ISO_CisPeripheral example on another board with LE
 * isochronous channel support (e.g. ESP32-S31).
 *
 * Callback style: named functions.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_ISO_CIS_CENTRAL_SUPPORTED

static const char *TARGET_NAME = "ISO CIS Peripheral";

// Must match the Peripheral.
static const uint16_t SDU_SIZE = 8;         // Central to Peripheral
static const uint16_t RETURN_SDU_SIZE = 8;  // Peripheral to Central

BLEClient client;
BLEIso::Channel *cis = nullptr;
BTAddress peerAddress;
volatile bool doConnect = false;

// Written from the ISO host task, read from loop().
volatile bool cisUp = false;
volatile uint32_t rxCount = 0;
volatile uint32_t rxLost = 0;
volatile uint32_t lastEcho = 0;  // our counter as last seen by the Peripheral

void onCisConnected(BLEIso::Channel &ch) {
  Serial.printf("CIS up on channel %d\n", ch.slot());
  rxCount = 0;
  rxLost = 0;
  cisUp = true;
}

void onCisDisconnected(BLEIso::Channel &ch, uint8_t reason) {
  (void)ch;
  Serial.printf("CIS down (reason 0x%02X)\n", reason);
  cisUp = false;
}

// ISO host task, once per SDU interval: keep it short.
void onCisReceive(BLEIso::Channel &ch, const BLEIso::SduInfo &info, const uint8_t *sdu, uint16_t len) {
  (void)ch;
  if (!info.valid || len < 8) {
    rxLost = rxLost + 1;  // lost or damaged SDU
    return;
  }
  uint32_t echo;
  memcpy(&echo, sdu + 4, sizeof(echo));
  lastEcho = echo;
  rxCount = rxCount + 1;
}

// Keep the scan callback lightweight: record the address, connect from loop().
void onDeviceFound(BLEAdvertisedDevice device) {
  if (device.getName() != TARGET_NAME) {
    return;
  }
  Serial.printf("Found \"%s\" at %s\n", TARGET_NAME, device.getAddress().toString().c_str());
  peerAddress = device.getAddress();
  doConnect = true;
  BLE.getScan().stop();
}

bool openLinkAndCis() {
  Serial.print("ACL connecting... ");
  client = BLE.createClient();
  BTStatus st = client.connect(peerAddress);
  if (!st) {
    Serial.printf("FAILED (%s)\n", st.toString());
    return false;
  }
  Serial.printf("OK (handle %u)\n", client.getHandle());

  BLEIso::CisParams qos;
  qos.sduIntervalUs = 10000;  // one SDU every 10 ms, both directions
  qos.latencyMs = 10;
  qos.sduSize = SDU_SIZE;
  qos.returnSduSize = RETURN_SDU_SIZE;
  qos.phy = BLEIso::Phy::Phy2M;
  qos.rtn = 2;

  // Reconnecting reuses the same channel; its callbacks are registered again.
  cis = BLEIso::connectCis(client.getHandle(), qos);
  if (!cis) {
    Serial.println("CIS connect FAILED");
    client.disconnect();
    return false;
  }
  cis->onConnected(onCisConnected).onDisconnected(onCisDisconnected).onReceive(onCisReceive);

  // The CIS is established asynchronously.
  unsigned long deadline = millis() + 5000;
  while (!cisUp && millis() < deadline) {
    delay(20);
  }
  if (!cisUp) {
    Serial.println("CIS did not come up in time");
    client.disconnect();
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== BLE ISO CIS Central ===");

  BTStatus st = BLE.begin("ISO CIS Central");
  if (!st) {
    Serial.printf("BLE.begin failed: %s\n", st.toString());
    while (true) {
      delay(1000);
    }
  }
  st = BLEIso::begin();
  if (!st) {
    Serial.printf("BLEIso::begin failed: %s\n", st.toString());
    while (true) {
      delay(1000);
    }
  }

  BLEScan scan = BLE.getScan();
  scan.setActiveScan(true);
  scan.onResult(onDeviceFound);
  scan.start(0);
  Serial.printf("Scanning for \"%s\"...\n", TARGET_NAME);
}

void loop() {
  static uint32_t counter = 0;
  static uint32_t lastPrint = 0;
  static bool linked = false;

  if (doConnect) {
    doConnect = false;
    linked = openLinkAndCis();
    if (!linked) {
      Serial.println("Retrying scan...");
      BLE.getScan().start(0);
    }
  }

  // The link or the CIS dropped: start over.
  if (linked && (!client.isConnected() || !cisUp)) {
    Serial.println("Link lost, rescanning...");
    linked = false;
    if (client.isConnected()) {
      client.disconnect();
    }
    BLE.getScan().start(0);
  }

  // One SDU per 10 ms SDU interval: [counter][4 bytes of padding].
  if (linked && cisUp) {
    uint8_t sdu[SDU_SIZE] = {0};
    memcpy(sdu, &counter, 4);
    if (cis->send(sdu, sizeof(sdu))) {
      counter++;
    }
  }

  if (linked && millis() - lastPrint >= 1000) {
    lastPrint = millis();
    Serial.printf(
      "sent %lu, rx %lu answers (%lu lost), Peripheral last saw %lu\n", (unsigned long)counter, (unsigned long)rxCount, (unsigned long)rxLost,
      (unsigned long)lastEcho
    );
  }
  delay(10);
}

#else  // !BLE_ISO_CIS_CENTRAL_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("ISO_CisCentral requires LE isochronous channels with CONFIG_BT_ISO_CENTRAL in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_ISO_CIS_CENTRAL_SUPPORTED */
