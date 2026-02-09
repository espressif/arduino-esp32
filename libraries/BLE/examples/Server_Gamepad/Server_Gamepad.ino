/*
 * BLE HID Gamepad Example
 *
 * This example demonstrates how to create a BLE HID Gamepad device using ESP32.
 * The gamepad will appear as a standard HID game controller on Windows, macOS, Linux, Android, and iOS.
 *
 * Features:
 * - 8 buttons (mapped to buttons 1-8)
 * - 2 axes (X, Y for joystick movement)
 * - Secure pairing with bonding
 * - Battery level reporting
 * - Automatic reconnection after power cycle
 *
 * Usage:
 * 1. Upload this sketch to your ESP32
 * 2. Pair with your device (Windows: Settings > Bluetooth & devices)
 * 3. The gamepad will send test input (circular motion and button toggle)
 * 4. Test in any game or with gamepad testing tools like https://hardwaretester.com/gamepad
 *
 * Note: This example uses "Just Works" pairing for automatic connection without
 * PIN entry or confirmation, just like real gamepads. The bond is saved for future connections.
 *
 * Created by lucasssvaz
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>

// HID Report Descriptor for a gamepad with 8 buttons and 2 axes (X, Y)
// This descriptor defines the gamepad as having:
// - 8 buttons (usage buttons 1-8)
// - 2 8-bit axes (X, Y) with range -127 to 127
const uint8_t hidReportDescriptor[] = {
  0x05, 0x01,  // Usage Page (Generic Desktop)
  0x09, 0x05,  // Usage (Gamepad)
  0xA1, 0x01,  // Collection (Application)
  0x85, 0x01,  //   Report ID (1)
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

// Gamepad report structure (matches the HID descriptor)
struct GamepadReport {
  uint8_t reportId;  // Report ID (must be 1)
  int8_t x;          // X axis (-127 to 127)
  int8_t y;          // Y axis (-127 to 127)
  uint8_t buttons;   // 8 buttons (bit 0-7)
} __attribute__((packed));

BLEHIDDevice *hid;
BLECharacteristic *inputGamepad;
BLEServer *server;
bool deviceConnected = false;

// Server callbacks to track connection status
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("Client connected");
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("Client disconnected");
    // Restart advertising so we can reconnect
    BLEDevice::startAdvertising();
    Serial.println("Advertising restarted");
  }
};

// Security callbacks for "Just Works" pairing
class SecurityCallbacks : public BLESecurityCallbacks {
  bool onSecurityRequest() {
    return true;  // Accept all pairing requests
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) {
    if (auth_cmpl.success) {
      Serial.println("Pairing successful!");
    } else {
      Serial.printf("Pairing failed, status: %d\n", auth_cmpl.fail_reason);
    }
  }
#elif defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc *desc) {
    if (desc->sec_state.encrypted) {
      Serial.println("Pairing successful!");
    } else {
      Serial.println("Pairing failed");
    }
  }
#endif

  // These are not used with "Just Works" pairing (ESP_IO_CAP_NONE)
  uint32_t onPassKeyRequest() {
    return 0;
  }
  void onPassKeyNotify(uint32_t pass_key) {}
  bool onConfirmPIN(uint32_t pass_key) {
    return true;
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE HID Gamepad");

  // Initialize BLE
  BLEDevice::init("ESP32-Gamepad");

  // Configure BLE Security for pairing and bonding
  // Use "Just Works" pairing - no user interaction required (like a real gamepad)
  BLESecurity *pSecurity = new BLESecurity();

  // Set IO capability to NONE (no display, no keyboard - like a real gamepad)
  pSecurity->setCapability(ESP_IO_CAP_NONE);

  // Set authentication mode: bonding=true, MITM=false, secure connection=true
  // This enables "Just Works" pairing with bonding
  pSecurity->setAuthenticationMode(true, false, true);

  // Set security callbacks
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks());

  // Create BLE Server
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  // Create HID Device
  hid = new BLEHIDDevice(server);

  // Set HID device information
  hid->manufacturer()->setValue("Espressif");
  hid->pnp(0x02, 0x05ac, 0x820a, 0x0110);  // Vendor ID, Product ID, Product Version
  hid->hidInfo(0x00, 0x01);                // HID version, country code

  // Set Report Map (HID descriptor)
  hid->reportMap((uint8_t *)hidReportDescriptor, sizeof(hidReportDescriptor));

  // Create input report characteristic for gamepad
  inputGamepad = hid->inputReport(1);  // Report ID 1

  // Set battery level to 100%
  hid->setBatteryLevel(100);

  // Start HID services
  hid->startServices();

  // Setup advertising
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->setAppearance(0x03C4);  // HID Gamepad appearance
  advertising->addServiceUUID(hid->hidService()->getUUID());
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);  // Help with connection issues
  advertising->setMaxPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("BLE HID Gamepad ready!");
  Serial.println("Waiting for connection...");
}

void loop() {
  if (deviceConnected) {
    static uint32_t lastReportTime = 0;
    static uint32_t counter = 0;

    // Send a report every 50ms (20Hz)
    if (millis() - lastReportTime >= 50) {
      lastReportTime = millis();
      counter++;

      GamepadReport report;
      report.reportId = 1;

      // Simulate some movement (sine wave pattern)
      report.x = (int8_t)(127 * sin(counter * 0.1));       // X axis: -127 to 127
      report.y = (int8_t)(127 * cos(counter * 0.1));       // Y axis: -127 to 127
      report.buttons = (counter % 40 < 20) ? 0x01 : 0x00;  // Toggle first button every second

      // Send the report
      inputGamepad->setValue((uint8_t *)&report, sizeof(report));
      inputGamepad->notify();

      // Print status every 2 seconds
      if (counter % 40 == 0) {
        Serial.printf("Report #%lu: X=%d, Y=%d, Buttons=0x%02X\n", counter, report.x, report.y, report.buttons);
      }
    }
  }

  delay(10);
}
