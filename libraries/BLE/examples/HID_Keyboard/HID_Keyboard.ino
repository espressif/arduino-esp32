#include <Arduino.h>
#include <cstring>
#include <BLE.h>
#include <HIDKeyboardTypes.h>

static const uint8_t kReportMap[] = {USAGE_PAGE(1),      0x01, USAGE(1),           0x06, COLLECTION(1),      0x01, REPORT_ID(1),       REPORT_ID_KEYBOARD,
                                     USAGE_PAGE(1),      0x07, USAGE_MINIMUM(1),   0xE0, USAGE_MAXIMUM(1),   0xE7, LOGICAL_MINIMUM(1), 0x00,
                                     LOGICAL_MAXIMUM(1), 0x01, REPORT_SIZE(1),     0x01, REPORT_COUNT(1),    0x08, HIDINPUT(1),        0x02,
                                     REPORT_COUNT(1),    0x01, REPORT_SIZE(1),     0x08, HIDINPUT(1),        0x01, REPORT_COUNT(1),    0x06,
                                     REPORT_SIZE(1),     0x08, LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x65, USAGE_MINIMUM(1),   0x00,
                                     USAGE_MAXIMUM(1),   0x65, HIDINPUT(1),        0x00, END_COLLECTION(0)};

BLECharacteristic inputReport;
volatile bool deviceConnected = false;

void setup() {
  Serial.begin(115200);
  Serial.println("BLE HID Keyboard Example");

  BLE.begin("ESP32-Keyboard");

  BLESecurity sec = BLE.getSecurity();
  sec.setAuthenticationMode(true, false, true);

  BLEServer server = BLE.createServer();
  server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
    Serial.println("Client connected");
    deviceConnected = true;
  });
  server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
    Serial.println("Client disconnected");
    deviceConnected = false;
    BLE.startAdvertising();
  });

  BLEHIDDevice hid(server);
  hid.manufacturer("Espressif");
  hid.pnp(0x02, 0x05AC, 0x820A, 0x0210);
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

void sendKey(uint8_t modifier, uint8_t keycode) {
  uint8_t report[8] = {0};
  report[0] = modifier;
  report[2] = keycode;
  inputReport.setValue(report, sizeof(report));
  inputReport.notify();
  delay(50);

  // Send key-release report (all zeroes)
  memset(report, 0, sizeof(report));
  inputReport.setValue(report, sizeof(report));
  inputReport.notify();
}

void loop() {
  if (deviceConnected && Serial.available()) {
    char c = Serial.read();
    if (c >= 'a' && c <= 'z') {
      sendKey(0, 0x04 + (c - 'a'));
    } else if (c >= 'A' && c <= 'Z') {
      sendKey(KEY_SHIFT, 0x04 + (c - 'A'));
    } else if (c == ' ') {
      sendKey(0, 0x2C);
    } else if (c == '\n') {
      sendKey(0, 0x28);
    }
  }
  delay(10);
}
