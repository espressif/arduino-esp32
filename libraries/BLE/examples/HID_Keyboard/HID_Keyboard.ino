/*
 * BLE HID Keyboard Example -- New API
 *
 * Creates a BLE HID keyboard that sends keystrokes typed in Serial Monitor.
 * The device appears as a standard keyboard on all major platforms.
 *
 * Persistent bonding:
 *   The BLE stack automatically stores bond keys in NVS so that a paired
 *   host can reconnect without re-pairing after a reboot. To clear stored
 *   bonds, enable "Erase All Flash Before Sketch Upload" in the Arduino
 *   IDE Tools menu and re-upload.
 *
 * Features:
 *   - Full ASCII character support (a-z, A-Z, 0-9, common symbols)
 *   - Modifier key handling (Shift for uppercase/symbols)
 *   - Secure pairing with bonding (persists across reboots)
 *   - Auto re-advertise on disconnect
 *
 * Usage:
 *   1. Upload to your ESP32
 *   2. Pair from Settings > Bluetooth on your PC / phone
 *   3. Open a text editor on the host
 *   4. Type characters in the Serial Monitor to send as keystrokes
 *   5. Reboot the ESP32 -- the bonded host will reconnect automatically
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <cstring>
#include <BLE.h>
#include <HIDTypes.h>
// Provides keymap[] for ASCII-to-HID translation (default: UK layout; define
// US_KEYBOARD before this include to switch to US layout)
#include <HIDKeyboardTypes.h>
// HID Report Descriptor for a standard keyboard.
// Defines: 8 modifier bits (Ctrl, Shift, Alt, GUI x left/right),
//          1 reserved byte, and 6 simultaneous key slots
//          (standard USB HID boot keyboard layout).
//
// The macros below (USAGE_PAGE, COLLECTION, HIDINPUT, etc.) are defined
// in HIDTypes.h. Each macro produces the HID short-item prefix byte; the
// parameter is the number of data bytes that follow (usually 1). See
// HIDTypes.h for a full explanation of the encoding.
// clang-format off
static const uint8_t kReportMap[] = {
  USAGE_PAGE(1),      0x01,              // Generic Desktop
  USAGE(1),           0x06,              // Keyboard
  COLLECTION(1),      0x01,              // Application
  REPORT_ID(1),       REPORT_ID_KEYBOARD,
  // Modifier keys (8 bits: LCtrl, LShift, LAlt, LGUI, RCtrl, RShift, RAlt, RGUI)
  USAGE_PAGE(1),      0x07,              //   Keyboard/Keypad
  USAGE_MINIMUM(1),   0xE0,              //   Left Control
  USAGE_MAXIMUM(1),   0xE7,              //   Right GUI
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_SIZE(1),     0x01,              //   1 bit per modifier
  REPORT_COUNT(1),    0x08,              //   8 modifier keys
  HIDINPUT(1),        0x02,              //   Data, Variable, Absolute
  // Reserved byte (padding required by the HID boot keyboard spec)
  REPORT_COUNT(1),    0x01,
  REPORT_SIZE(1),     0x08,              //   8 bits
  HIDINPUT(1),        0x01,              //   Constant (reserved)
  // Key array (up to 6 simultaneous key codes)
  REPORT_COUNT(1),    0x06,              //   6 key slots
  REPORT_SIZE(1),     0x08,              //   8 bits per key
  LOGICAL_MINIMUM(1), 0x00,              //   0 = no key
  LOGICAL_MAXIMUM(1), 0x65,              //   101 = Keyboard Application
  USAGE_MINIMUM(1),   0x00,
  USAGE_MAXIMUM(1),   0x65,
  HIDINPUT(1),        0x00,              //   Data, Array
  END_COLLECTION(0)
};
// clang-format on

BLECharacteristic inputReport;
volatile bool deviceConnected = false;

// Send a single key press followed by a release (8-byte HID keyboard report).
// Report format: [modifier, reserved, key1, key2, key3, key4, key5, key6]
void sendKey(uint8_t modifier, uint8_t keycode) {
  uint8_t report[8] = {0};
  report[0] = modifier;
  report[2] = keycode;
  inputReport.setValue(report, sizeof(report));
  inputReport.notify();
  delay(50);

  memset(report, 0, sizeof(report));
  inputReport.setValue(report, sizeof(report));
  inputReport.notify();
}

// Type a single ASCII character (similar to USBHIDKeyboard::write).
// Uses the keymap[] lookup table from HIDKeyboardTypes.h, which maps each
// ASCII value to its HID usage code and modifier bitmask.
void write(char c) {
  uint8_t idx = (uint8_t)c;
  if (idx >= KEYMAP_SIZE) {
    return;
  }
  const KEYMAP &k = keymap[idx];
  if (k.usage) {
    sendKey(k.modifier, k.usage);
  }
}

// Type a string one character at a time (similar to USBHIDKeyboard::print)
void print(const char *str) {
  while (*str) {
    write(*str++);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE HID Keyboard Example");

  BTStatus status = BLE.begin("ESP32-Keyboard");
  if (!status) {
    Serial.printf("BLE init failed! (%s)\n", status.toString());
    return;
  }

  BLESecurity sec = BLE.getSecurity();
  // bonding = true (persist keys in NVS), MITM = false, Secure Connections = true
  sec.setAuthenticationMode(true, false, true);

  BLEServer server = BLE.createServer();
  server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
    Serial.printf("Client connected: %s\n", conn.getAddress().toString().c_str());
    deviceConnected = true;
  });
  server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
    Serial.printf("Client disconnected (reason 0x%02X)\n", reason);
    deviceConnected = false;
    BLE.startAdvertising();
  });

  BLEHIDDevice hid(server);
  hid.manufacturer("Espressif");
  // PnP: vendor source (0x02 = USB-IF), vendor ID, product ID, product version
  hid.pnp(0x02, 0x05AC, 0x820A, 0x0210);
  // HID Info: country code (0x00 = not localized), flags (0x01 = normally connectable)
  hid.hidInfo(0x00, 0x01);
  hid.reportMap(kReportMap, sizeof(kReportMap));

  inputReport = hid.inputReport(REPORT_ID_KEYBOARD);
  server.start();

  BLEAdvertising adv = BLE.getAdvertising();
  adv.setAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  adv.setScanResponse(true);
  adv.start();

  Serial.println("HID Keyboard advertising. Pair from your device.");
  Serial.println("Type characters in Serial Monitor to send as keystrokes.");
}

void loop() {
  if (deviceConnected && Serial.available()) {
    char c = Serial.read();
    write(c);
  }
  delay(10);
}
