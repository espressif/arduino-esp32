/*
 * BLE Isochronous Channels -- BIG Broadcaster
 *
 * Broadcasts transparent SDUs over a Broadcast Isochronous Group (BIG) with
 * one Broadcast Isochronous Stream (BIS). Any number of receivers can sync to
 * it without a connection, and they all get each SDU in the same SDU interval,
 * which makes a BIG a good fit for synchronized multi-device updates (lights,
 * displays, sensor time bases) or custom broadcast audio. For Auracast (LC3
 * with BAP/PBP interoperability) use LEAudio_BroadcastSource instead.
 *
 * The BIG rides on a periodic advertising train: this sketch starts extended
 * advertising (so receivers can find it by name) plus periodic advertising
 * on the same set, then creates the BIG on that set. Each SDU carries a
 * counter and the sender's millis().
 *
 * Set BROADCAST_CODE to encrypt the BIG; receivers then need the same code.
 *
 * Pair with the ISO_BigReceiver example on one or more boards with LE
 * isochronous channel support (e.g. ESP32-S31).
 *
 * Callback style: lambdas.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_ISO_BROADCASTER_SUPPORTED && BLE_PERIODIC_ADV_TX_SUPPORTED

static const char *DEVICE_NAME = "ISO BIG Broadcaster";
static const uint8_t ADV_INSTANCE = 0;  // advertising set that carries the BIG
static const uint8_t ADV_SID = 1;       // receivers sync to this SID
static const char *BROADCAST_CODE = "";  // up to 16 characters; empty = unencrypted
static const uint16_t SDU_SIZE = 8;

BLEIso::Channel *bis = nullptr;
volatile bool bisUp = false;  // written from the ISO host task

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== BLE ISO BIG Broadcaster ===");

  BTStatus st = BLE.begin(DEVICE_NAME);
  if (!st) {
    halt("BLE.begin", st);
  }

  // Extended advertising carries the name; the periodic train carries the BIGInfo.
  BLEAdvertising adv = BLE.getAdvertising();
  adv.setExtType(ADV_INSTANCE, BLEAdvType::NonConnectable);
  adv.setExtPhy(ADV_INSTANCE, BLEPhy::PHY_1M, BLEPhy::PHY_2M);
  adv.setExtSID(ADV_INSTANCE, ADV_SID);
  BLEAdvertisementData extData;
  extData.setName(DEVICE_NAME);
  st = adv.setExtAdvertisementData(ADV_INSTANCE, extData);
  if (!st) {
    halt("setExtAdvertisementData", st);
  }
  adv.setPeriodicAdvInterval(ADV_INSTANCE, 0x20, 0x40);  // 40-80 ms
  BLEAdvertisementData perData;
  perData.setName(DEVICE_NAME);
  st = adv.setPeriodicAdvData(ADV_INSTANCE, perData);
  if (!st) {
    halt("setPeriodicAdvData", st);
  }
  st = adv.startExtended(ADV_INSTANCE);
  if (!st) {
    halt("startExtended", st);
  }
  st = adv.startPeriodicAdv(ADV_INSTANCE);
  if (!st) {
    halt("startPeriodicAdv", st);
  }

  st = BLEIso::begin();
  if (!st) {
    halt("BLEIso::begin", st);
  }

  BLEIso::BigParams qos;
  qos.sduIntervalUs = 10000;  // one SDU every 10 ms
  qos.latencyMs = 10;
  qos.sduSize = SDU_SIZE;
  qos.phy = BLEIso::Phy::Phy2M;
  qos.rtn = 2;                // each SDU is sent up to 3 times
  qos.broadcastCode = BROADCAST_CODE;
  bis = BLEIso::createBig(ADV_INSTANCE, qos);
  if (!bis) {
    halt("BLEIso::createBig", BTStatus::Fail);
  }
  bis->onConnected([](BLEIso::Channel &ch) {
       Serial.printf("BIG up (BIS on channel %d)%s\n", ch.slot(), BROADCAST_CODE[0] ? ", encrypted" : "");
       bisUp = true;
     })
    .onDisconnected([](BLEIso::Channel &, uint8_t reason) {
      Serial.printf("BIG terminated (reason 0x%02X)\n", reason);
      bisUp = false;
    });
  Serial.printf("Broadcasting \"%s\" (SID %u)\n", DEVICE_NAME, ADV_SID);
}

void loop() {
  static uint32_t counter = 0;
  static uint32_t lastPrint = 0;

  // One SDU per 10 ms SDU interval: [counter][millis()].
  if (bisUp) {
    uint8_t sdu[SDU_SIZE];
    uint32_t now = millis();
    memcpy(sdu, &counter, 4);
    memcpy(sdu + 4, &now, 4);
    if (bis->send(sdu, sizeof(sdu))) {
      counter++;
    }
  }

  if (bisUp && millis() - lastPrint >= 1000) {
    lastPrint = millis();
    Serial.printf("sent %lu SDUs\n", (unsigned long)counter);
  }
  delay(10);
}

#else  // !(BLE_ISO_BROADCASTER_SUPPORTED && BLE_PERIODIC_ADV_TX_SUPPORTED)

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("ISO_BigBroadcaster requires LE isochronous channels with CONFIG_BT_ISO_BROADCASTER and periodic advertising in this build.");
}

void loop() {
  delay(1000);
}

#endif
