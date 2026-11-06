/*
 * BLE Isochronous Channels -- BIG Receiver
 *
 * Receives the transparent SDUs of the ISO_BigBroadcaster example. No
 * connection is involved: the receiver finds the broadcaster with an extended
 * scan, syncs to its periodic advertising train, and the BIGInfo carried on
 * that train tells the controller how to sync to the BIG itself.
 *
 * BLEIso::syncBig() arms the sync before the scan starts; the actual BIG sync
 * is issued automatically when the first BIGInfo report arrives. While armed,
 * a lost sync is issued again on the next BIGInfo, so the receiver recovers
 * by itself when the broadcaster comes back.
 *
 * Each SDU carries the broadcaster's counter; the sketch prints how many SDUs
 * arrived, how many were lost or damaged, and the last counter per second.
 *
 * Set BROADCAST_CODE to the broadcaster's code for an encrypted BIG.
 *
 * Pair with the ISO_BigBroadcaster example (any number of receivers can sync).
 *
 * Callback style: lambdas.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_ISO_SYNC_RECEIVER_SUPPORTED && BLE5_SUPPORTED

static const char *TARGET_NAME = "ISO BIG Broadcaster";
static const uint8_t TARGET_SID = 1;     // advertising SID of the broadcaster's periodic train
static const uint8_t BIS_INDEX = 1;      // 1-based BIS within the BIG
static const char *BROADCAST_CODE = "";  // must match the broadcaster when it encrypts
static const uint16_t SDU_SIZE = 8;

BLEIso::Channel *bis = nullptr;
volatile bool paSyncRequested = false;

// Written from the ISO host task, read from loop().
volatile bool bisUp = false;
volatile uint32_t rxCount = 0;
volatile uint32_t rxLost = 0;
volatile uint32_t lastCounter = 0;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== BLE ISO BIG Receiver ===");

  BTStatus st = BLE.begin("ISO BIG Receiver");
  if (!st) {
    halt("BLE.begin", st);
  }
  st = BLEIso::begin();
  if (!st) {
    halt("BLEIso::begin", st);
  }

  // Arm the BIG sync first: it is issued on the first BIGInfo of the train.
  BLEIso::BigParams qos;
  qos.sduSize = SDU_SIZE;
  qos.broadcastCode = BROADCAST_CODE;
  bis = BLEIso::syncBig(BIS_INDEX, qos);
  if (!bis) {
    halt("BLEIso::syncBig", BTStatus::Fail);
  }
  bis->onConnected([](BLEIso::Channel &) {
       Serial.println("Synced to the BIG");
       bisUp = true;
     })
    .onDisconnected([](BLEIso::Channel &, uint8_t reason) {
      Serial.printf("BIG sync lost (reason 0x%02X); waiting for the next BIGInfo\n", reason);
      bisUp = false;
    })
    .onReceive([](BLEIso::Channel &, const BLEIso::SduInfo &info, const uint8_t *sdu, uint16_t len) {
      // ISO host task, once per SDU interval: keep it short.
      if (!info.valid || len < 4) {
        rxLost = rxLost + 1;
        return;
      }
      uint32_t counter;
      memcpy(&counter, sdu, sizeof(counter));
      lastCounter = counter;
      rxCount = rxCount + 1;
    });

  BLEScan scan = BLE.getScan();
  scan.onResult([](const BLEAdvertisedDevice &dev) {
    if (paSyncRequested || dev.getName() != TARGET_NAME) {
      return;
    }
    paSyncRequested = true;
    Serial.printf("Found \"%s\" at %s, syncing to its periodic train\n", TARGET_NAME, dev.getAddress().toString().c_str());
    BLE.getScan().createPeriodicSync(dev.getAddress(), TARGET_SID);
  });
  scan.onPeriodicSync([](uint16_t syncHandle, uint8_t sid, const BTAddress &, BLEPhy, uint16_t interval) {
    Serial.printf("Periodic sync 0x%04X established (SID %u, interval %u x 1.25 ms)\n", syncHandle, sid, interval);
  });
  scan.onPeriodicLost([](uint16_t syncHandle) {
    Serial.printf("Periodic sync 0x%04X lost, scanning again\n", syncHandle);
    paSyncRequested = false;
  });
  st = scan.startExtended(0);
  if (!st) {
    halt("startExtended", st);
  }
  Serial.printf("Scanning for \"%s\"...\n", TARGET_NAME);
}

void loop() {
  static uint32_t lastPrint = 0;
  if (bisUp && millis() - lastPrint >= 1000) {
    lastPrint = millis();
    Serial.printf("rx %lu SDUs (%lu lost), last counter %lu\n", (unsigned long)rxCount, (unsigned long)rxLost, (unsigned long)lastCounter);
  }
  delay(10);
}

#else  // !(BLE_ISO_SYNC_RECEIVER_SUPPORTED && BLE5_SUPPORTED)

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("ISO_BigReceiver requires LE isochronous channels with CONFIG_BT_ISO_SYNC_RECEIVER and BLE 5 in this build.");
}

void loop() {
  delay(1000);
}

#endif
