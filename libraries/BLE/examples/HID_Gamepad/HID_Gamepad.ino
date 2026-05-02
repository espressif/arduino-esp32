/*
 * BLE HID Gamepad Example -- New API
 *
 * Creates a BLE HID gamepad with 8 buttons and 2 axes (X, Y).
 * The device appears as a standard game controller on all major platforms.
 *
 * Features:
 *   - 8 buttons (mapped to buttons 1-8)
 *   - 2 axes (X, Y for joystick, -127 to 127)
 *   - Secure pairing with bonding ("Just Works")
 *   - Battery level reporting
 *   - Auto re-advertise on disconnect
 *
 * Usage:
 *   1. Upload to your ESP32
 *   2. Pair from Settings > Bluetooth on your PC / phone
 *   3. Open https://hardwaretester.com/gamepad to see live input
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <cstring>
#include <cmath>
#include <BLE.h>
#include <HIDTypes.h>

static const uint8_t kReportMap[] = {
  0x05, 0x01,  // Usage Page (Generic Desktop)
  0x09, 0x05,  // Usage (Gamepad)
  0xA1, 0x01,  // Collection (Application)
  0x85, 0x01,  //   Report ID (1)
  // 2 axes: X and Y
  0x09, 0x01,  //   Usage (Pointer)
  0xA1, 0x00,  //   Collection (Physical)
  0x09, 0x30,  //     Usage (X)
  0x09, 0x31,  //     Usage (Y)
  0x15, 0x81,  //     Logical Minimum (-127)
  0x25, 0x7F,  //     Logical Maximum (127)
  0x75, 0x08,  //     Report Size (8)
  0x95, 0x02,  //     Report Count (2)
  0x81, 0x02,  //     Input (Data, Variable, Absolute)
  0xC0,        //   End Collection
  // 8 buttons
  0x05, 0x09,  //   Usage Page (Button)
  0x19, 0x01,  //   Usage Minimum (Button 1)
  0x29, 0x08,  //   Usage Maximum (Button 8)
  0x15, 0x00,  //   Logical Minimum (0)
  0x25, 0x01,  //   Logical Maximum (1)
  0x75, 0x01,  //   Report Size (1)
  0x95, 0x08,  //   Report Count (8)
  0x81, 0x02,  //   Input (Data, Variable, Absolute)
  0xC0         // End Collection
};

struct __attribute__((packed)) GamepadReport {
  uint8_t reportId;
  int8_t x;
  int8_t y;
  uint8_t buttons;
};

BLECharacteristic inputGamepad;
volatile bool deviceConnected = false;

void sendReport(int8_t x, int8_t y, uint8_t buttons) {
  GamepadReport rpt;
  rpt.reportId = 1;
  rpt.x = x;
  rpt.y = y;
  rpt.buttons = buttons;
  inputGamepad.setValue(reinterpret_cast<uint8_t *>(&rpt), sizeof(rpt));
  inputGamepad.notify();
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE HID Gamepad");

  BLE.begin("ESP32-Gamepad");

  BLESecurity sec = BLE.getSecurity();
  sec.setIOCapability(BLESecurity::NoInputNoOutput);
  sec.setAuthenticationMode(true, false, true);

  BLEServer server = BLE.createServer();
  server.onConnect([](BLEServer, const BLEConnInfo &conn) {
    Serial.printf("Client connected: %s\n", conn.getAddress().toString().c_str());
    deviceConnected = true;
  });
  server.onDisconnect([](BLEServer, const BLEConnInfo &, uint8_t) {
    Serial.println("Client disconnected");
    deviceConnected = false;
    BLE.startAdvertising();
  });

  BLEHIDDevice hid(server);
  hid.manufacturer("Espressif");
  hid.pnp(0x02, 0x05AC, 0x820A, 0x0110);
  hid.hidInfo(0x00, 0x01);
  hid.reportMap(kReportMap, sizeof(kReportMap));
  hid.setBatteryLevel(100);

  inputGamepad = hid.inputReport(1);
  server.start();

  BLEAdvertising adv = BLE.getAdvertising();
  adv.setAppearance(BLE_APPEARANCE_HID_GAMEPAD);
  adv.setScanResponse(true);
  adv.start();

  Serial.println("Advertising as gamepad. Pair from your device.");
}

void loop() {
  if (!deviceConnected) {
    delay(100);
    return;
  }

  static uint32_t step = 0;
  step++;

  int8_t x = (int8_t)(127.0f * sinf(step * 0.1f));
  int8_t y = (int8_t)(127.0f * cosf(step * 0.1f));
  uint8_t buttons = (step % 40 < 20) ? 0x01 : 0x00;

  sendReport(x, y, buttons);

  if (step % 40 == 0) {
    Serial.printf("X=%4d Y=%4d Buttons=0x%02X\n", x, y, buttons);
  }

  delay(50);
}
