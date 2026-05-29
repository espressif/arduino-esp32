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

// HID Report Descriptor for a gamepad with 2 axes (X, Y) and 8 buttons.
// The macros (USAGE_PAGE, COLLECTION, etc.) are defined in HIDTypes.h.
// Each macro produces the HID short-item prefix byte; the parameter is the
// number of data bytes that follow (usually 1). See HIDTypes.h for details.
// clang-format off
static const uint8_t kReportMap[] = {
  USAGE_PAGE(1),      0x01,  // Generic Desktop
  USAGE(1),           0x05,  // Gamepad
  COLLECTION(1),      0x01,  // Application
  REPORT_ID(1),       0x01,
  // 2 axes: X and Y
  USAGE(1),           0x01,  //   Pointer
  COLLECTION(1),      0x00,  //   Physical
  USAGE(1),           0x30,  //     X axis
  USAGE(1),           0x31,  //     Y axis
  LOGICAL_MINIMUM(1), 0x81,  //     -127
  LOGICAL_MAXIMUM(1), 0x7F,  //     127
  REPORT_SIZE(1),     0x08,  //     8 bits per axis
  REPORT_COUNT(1),    0x02,  //     2 axes
  HIDINPUT(1),        0x02,  //     Data, Variable, Absolute
  END_COLLECTION(0),         //   End Physical
  // 8 buttons
  USAGE_PAGE(1),      0x09,  //   Button
  USAGE_MINIMUM(1),   0x01,  //   Button 1
  USAGE_MAXIMUM(1),   0x08,  //   Button 8
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_SIZE(1),     0x01,  //   1 bit per button
  REPORT_COUNT(1),    0x08,  //   8 buttons
  HIDINPUT(1),        0x02,  //   Data, Variable, Absolute
  END_COLLECTION(0)          // End Application
};
// clang-format on

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

  BTStatus status = BLE.begin("ESP32-Gamepad");
  if (!status) {
    Serial.printf("BLE init failed! (%s)\n", status.toString());
    return;
  }

  BLESecurity sec = BLE.getSecurity();
  sec.setIOCapability(BLESecurity::NoInputNoOutput);
  // bonding = true, MITM protection = false, Secure Connections = true
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
  // PnP: vendor source (0x02 = USB-IF), vendor ID, product ID, product version
  hid.pnp(0x02, 0x05AC, 0x820A, 0x0110);
  // HID Info: country code (0x00 = not localized), flags (0x01 = normally connectable)
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
