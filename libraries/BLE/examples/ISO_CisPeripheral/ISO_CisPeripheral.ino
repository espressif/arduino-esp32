/*
 * BLE Isochronous Channels -- CIS Peripheral
 *
 * Accepts a Connected Isochronous Stream (CIS) from the ISO_CisCentral example
 * and exchanges transparent SDUs with it in both directions. A CIS delivers
 * one SDU per SDU interval (here 10 ms) with a bounded latency and a sequence
 * number, which suits time-synchronized sensor data, control loops or custom
 * audio formats. For LC3 audio with BAP/CAP interoperability use the
 * LEAudio_* examples instead.
 *
 * Flow: advertise connectably, let the Central open the ACL link, then the
 * Central creates the CIS on that link and this side accepts it.
 *
 * Every SDU received from the Central carries its send counter; this side
 * answers every SDU interval with its own counter and the last value it got.
 *
 * Pair with the ISO_CisCentral example on another board with LE isochronous
 * channel support (e.g. ESP32-S31).
 *
 * Callback style: named functions.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_ISO_CIS_PERIPHERAL_SUPPORTED

static const char *DEVICE_NAME = "ISO CIS Peripheral";
static const BLEUUID SVC_UUID("6c1e0001-5f6b-4d5a-9f0e-0a7d6f1a2b3c");

// Must match the Central: both SDU sizes define the two directions of the CIS.
static const uint16_t SDU_SIZE = 8;         // Central to Peripheral
static const uint16_t RETURN_SDU_SIZE = 8;  // Peripheral to Central

BLEIso::Channel *cis = nullptr;

// Written from the ISO host task, read from loop().
volatile bool cisUp = false;
volatile uint32_t rxCount = 0;
volatile uint32_t rxLost = 0;
volatile uint32_t lastPeerCounter = 0;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void onCisConnected(BLEIso::Channel &ch) {
  Serial.printf("CIS up on channel %d\n", ch.slot());
  rxCount = 0;
  rxLost = 0;
  cisUp = true;
}

void onCisDisconnected(BLEIso::Channel &ch, uint8_t reason) {
  (void)ch;
  Serial.printf("CIS down (reason 0x%02X); still listening\n", reason);
  cisUp = false;
}

// ISO host task, once per SDU interval: keep it short.
void onCisReceive(BLEIso::Channel &ch, const BLEIso::SduInfo &info, const uint8_t *sdu, uint16_t len) {
  (void)ch;
  if (!info.valid || len < 4) {
    rxLost = rxLost + 1;  // lost or damaged SDU
    return;
  }
  uint32_t counter;
  memcpy(&counter, sdu, sizeof(counter));
  lastPeerCounter = counter;
  rxCount = rxCount + 1;
}

void onClientDisconnect(BLEServer server, const BLEConnInfo &conn, uint8_t reason) {
  (void)conn;
  Serial.printf("ACL link down (reason 0x%02X), advertising again\n", reason);
  server.startAdvertising();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== BLE ISO CIS Peripheral ===");

  BTStatus st = BLE.begin(DEVICE_NAME);
  if (!st) {
    halt("BLE.begin", st);
  }

  // Minimal GATT server so the Central can open the ACL link the CIS rides on.
  BLEServer server = BLE.createServer();
  server.onDisconnect(onClientDisconnect);
  server.createService(SVC_UUID);
  st = server.start();
  if (!st) {
    halt("server.start", st);
  }

  st = BLEIso::begin();
  if (!st) {
    halt("BLEIso::begin", st);
  }

  // The Central picks the interval, latency, PHY and retransmissions; this
  // side only sizes its two directions.
  BLEIso::CisParams qos;
  qos.sduSize = SDU_SIZE;
  qos.returnSduSize = RETURN_SDU_SIZE;
  cis = BLEIso::listenCis(qos);
  if (!cis) {
    halt("BLEIso::listenCis", BTStatus::Fail);
  }
  cis->onConnected(onCisConnected).onDisconnected(onCisDisconnected).onReceive(onCisReceive);

  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(SVC_UUID);
  adv.setName(DEVICE_NAME);
  st = adv.start();
  if (!st) {
    halt("advertising", st);
  }
  Serial.println("Advertising; waiting for ISO_CisCentral to connect the CIS.");
}

void loop() {
  static uint32_t counter = 0;
  static uint32_t lastPrint = 0;

  // One SDU per 10 ms SDU interval on the Peripheral-to-Central direction:
  // [own counter][last counter received from the Central].
  if (cisUp) {
    uint8_t sdu[RETURN_SDU_SIZE];
    uint32_t peer = lastPeerCounter;
    memcpy(sdu, &counter, 4);
    memcpy(sdu + 4, &peer, 4);
    if (cis->send(sdu, sizeof(sdu))) {
      counter++;
    }
  }

  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();
    if (cisUp) {
      Serial.printf(
        "rx %lu SDUs (%lu lost), last Central counter %lu, sent %lu\n", (unsigned long)rxCount, (unsigned long)rxLost, (unsigned long)lastPeerCounter,
        (unsigned long)counter
      );
    }
  }
  delay(10);
}

#else  // !BLE_ISO_CIS_PERIPHERAL_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("ISO_CisPeripheral requires LE isochronous channels with CONFIG_BT_ISO_PERIPHERAL in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_ISO_CIS_PERIPHERAL_SUPPORTED */
