/*
 * BLE HID Client Example -- New API
 *
 * Connects to a BLE HID peripheral (gamepad or keyboard) and reads
 * input report notifications. Compatible with the HID_Gamepad example
 * or any standard BLE HID device.
 *
 * Features:
 *   - Scans for devices advertising the HID Service (0x1812)
 *   - Connects and discovers HID characteristics
 *   - Reads the Report Map and identifies input report characteristics
 *   - Subscribes to input report notifications
 *   - Parses and displays gamepad input (buttons + axes)
 *   - Auto-reconnect on disconnect
 *
 * Usage:
 *   1. Run HID_Gamepad on a second ESP32 (or turn on any BLE gamepad)
 *   2. Upload this sketch
 *   3. Open Serial Monitor to see live input
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

static BLEUUID hidServiceUUID((uint16_t)0x1812);
static BLEUUID reportCharUUID((uint16_t)0x2A4D);
static BLEUUID reportRefDescUUID((uint16_t)0x2908);

struct __attribute__((packed)) GamepadReport {
  uint8_t reportId;
  int8_t x;
  int8_t y;
  uint8_t buttons;
};

BTAddress targetAddr;
BLEClient client;
volatile bool targetFound = false;
volatile bool isConnected = false;

static void onNotify(BLERemoteCharacteristic chr, const uint8_t *data, size_t len, bool isNotify) {
  if (len == sizeof(GamepadReport)) {
    auto *rpt = reinterpret_cast<const GamepadReport *>(data);
    Serial.printf("X=%4d  Y=%4d  Buttons=0x%02X [", rpt->x, rpt->y, rpt->buttons);
    for (int i = 0; i < 8; i++) {
      if (rpt->buttons & (1 << i)) {
        Serial.printf(" %d", i + 1);
      }
    }
    Serial.println(" ]");
  } else {
    Serial.printf("Report (%u bytes): ", len);
    for (size_t i = 0; i < len; i++) {
      Serial.printf("%02X ", data[i]);
    }
    Serial.println();
  }
}

static bool connectAndSubscribe() {
  Serial.printf("Connecting to %s...\n", targetAddr.toString().c_str());
  BTStatus status = client.connect(targetAddr, 10000);
  if (!status) {
    Serial.printf("Connect failed: %s\n", status.toString());
    return false;
  }
  Serial.println("Connected!");

  BLERemoteService hidSvc = client.getService(hidServiceUUID);
  if (!hidSvc) {
    Serial.println("HID service not found");
    client.disconnect();
    return false;
  }

  auto chars = hidSvc.getCharacteristics();
  bool subscribed = false;

  for (auto &chr : chars) {
    if (chr.getUUID() != reportCharUUID || !chr.canNotify()) {
      continue;
    }

    BLERemoteDescriptor refDesc = chr.getDescriptor(reportRefDescUUID);
    if (refDesc) {
      String val = refDesc.readValue();
      if (val.length() >= 2) {
        uint8_t reportId = (uint8_t)val[0];
        uint8_t reportType = (uint8_t)val[1];
        Serial.printf("Found Report ID=%u Type=%u", reportId, reportType);
        if (reportType != 1) {
          Serial.println(" (not Input, skipping)");
          continue;
        }
        Serial.println(" (Input)");
      }
    }

    BTStatus sub = chr.subscribe(true, onNotify);
    if (sub) {
      Serial.printf("Subscribed to handle 0x%04X\n", chr.getHandle());
      subscribed = true;
    }
  }

  if (!subscribed) {
    Serial.println("No input report characteristics found");
    client.disconnect();
    return false;
  }

  isConnected = true;
  Serial.println("Ready to receive HID input!\n");
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE HID Client");

  if (!BLE.begin("ESP32-HID-Client")) {
    Serial.println("BLE init failed!");
    return;
  }

  BLESecurity sec = BLE.getSecurity();
  sec.setIOCapability(BLESecurity::NoInputNoOutput);
  sec.setAuthenticationMode(true, false, true);

  client = BLE.createClient();
  client.setMTU(185);
  client.onDisconnect([](BLEClient, const BLEConnInfo &, uint8_t reason) {
    Serial.printf("Disconnected (reason 0x%02X)\n", reason);
    isConnected = false;
  });

  BLEScan scan = BLE.getScan();
  scan.setActiveScan(true);
  scan.setInterval(100);
  scan.setWindow(99);

  scan.onResult([](BLEAdvertisedDevice dev) {
    if (dev.isAdvertisingService(hidServiceUUID)) {
      Serial.printf("Found HID device: %s", dev.getAddress().toString().c_str());
      if (dev.haveName()) {
        Serial.printf(" \"%s\"", dev.getName().c_str());
      }
      if (dev.haveAppearance()) {
        Serial.printf(" (appearance 0x%04X)", dev.getAppearance());
      }
      Serial.println();
      targetAddr = dev.getAddress();
      targetFound = true;
      BLE.getScan().stop();
    }
  });

  Serial.println("Scanning for BLE HID devices...\n");
  scan.start(10000);
}

void loop() {
  if (targetFound && !isConnected) {
    targetFound = false;
    if (!connectAndSubscribe()) {
      Serial.println("Retrying scan in 5s...");
      delay(5000);
      BLE.getScan().start(10000);
    }
  }

  if (!isConnected && !targetFound) {
    static uint32_t lastScan = 0;
    if (millis() - lastScan > 15000) {
      lastScan = millis();
      Serial.println("Scanning...");
      BLE.getScan().start(10000);
    }
  }

  delay(100);
}
