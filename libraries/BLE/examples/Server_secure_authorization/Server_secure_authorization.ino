/*
  Simple BLE Server Authorization Example

  This example demonstrates how to create a BLE server with authorization
  requirements. It shows the essential setup for:
  - Authorization with static passkey
  - Secure connection
  - MITM (Man-In-The-Middle) protection

  The server creates a single characteristic that requires authorization
  to access. Clients must provide the correct passkey (123456) to read
  or write to the characteristic.

  Note that ESP32 uses Bluedroid by default and the other SoCs use NimBLE.
  Bluedroid initiates security on-connect, while NimBLE initiates security on-demand.

  Due to a bug in ESP-IDF's Bluedroid, this example will currently not work with ESP32.

  IMPORTANT: MITM (Man-In-The-Middle protection) must be enabled for password prompts
  to work. Without MITM, the BLE stack assumes no user interaction is needed and will use
  "Just Works" pairing method (with encryption if secure connection is enabled).

  Created by lucasssvaz.
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <nvs_flash.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Example passkey - change this for production use
#define AUTH_PASSKEY 123456

static int s_readCount = 0;
static BLECharacteristic *s_pCharacteristic;

class MySecurityCallbacks : public BLESecurityCallbacks {
  bool onAuthorizationRequest(uint16_t connHandle, uint16_t attrHandle, bool isRead) {
    Serial.println("Authorization request received");
    if (isRead) {
      s_readCount++;
      // Keep value length <= (MTU - 1) to avoid a follow-up read request
      uint16_t maxLen = BLEDevice::getServer()->getPeerMTU(connHandle) - 1;
      String msg = "Authorized #" + String(s_readCount);
      if (msg.length() > maxLen) {
        msg = msg.substring(0, maxLen);
      }
      s_pCharacteristic->setValue(msg);
      // Grant authorization to the first 3 reads
      if (s_readCount <= 3) {
        Serial.println("Authorization granted");
        return true;
      } else {
        Serial.println("Authorization denied, read count exceeded");
        Serial.println("Please reset the read counter to continue");
        return false;
      }
    }
    // Fallback to deny
    Serial.println("Authorization denied");
    return false;
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE Authorization Example!");

  // Initialize the BOOT pin for resetting the read count
  pinMode(BOOT_PIN, INPUT_PULLUP);

  // Clear NVS to remove any cached pairing information
  // This ensures fresh authentication for testing
  Serial.println("Clearing NVS pairing data...");
  nvs_flash_erase();
  nvs_flash_init();

  Serial.print("Using BLE stack: ");
  Serial.println(BLEDevice::getBLEStackString());

  BLEDevice::init("BLE Auth Server");

  // Set MTU to 517 to avoid a follow-up read request
  BLEDevice::setMTU(517);

  // Configure BLE Security
  BLESecurity *pSecurity = new BLESecurity();

  // Set static passkey for authentication
  pSecurity->setPassKey(true, AUTH_PASSKEY);

  // Set IO capability to DisplayOnly for MITM authentication
  pSecurity->setCapability(ESP_IO_CAP_OUT);

  // Enable authorization requirements:
  // - bonding: true (for persistent storage of the keys)
  // - MITM: true (enables Man-In-The-Middle protection for password prompts)
  // - secure connection: true (enables secure connection for encryption)
  pSecurity->setAuthenticationMode(true, true, true);

  // Set the security callbacks
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());

  // Create BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->advertiseOnDisconnect(true);

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create characteristic with read and write properties
  uint32_t properties = BLECharacteristic::PROPERTY_READ;

  // For NimBLE: Add authentication properties
  // These properties ensure the characteristic requires authorization
  // (ignored by Bluedroid but harmless)
  properties |= BLECharacteristic::PROPERTY_READ_AUTHEN | BLECharacteristic::PROPERTY_READ_AUTHOR;

  s_pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, properties);

  // For Bluedroid: Set access permissions that require encryption and MITM
  // This ensures authorization is required (ignored by NimBLE)
  s_pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_READ_AUTHORIZATION);

  // Set initial value
  s_pCharacteristic->setValue("Hello! You needed authorization to read this!");

  // Start the service
  pService->start();

  // Configure and start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // helps with iPhone connections
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("BLE Server is running!");
  Serial.println("Authorization is required to access the characteristic.");
  Serial.printf("Use passkey: %d when prompted\n", AUTH_PASSKEY);
}

void loop() {
  // Reset the read count if the BOOT pin is pressed
  if (digitalRead(BOOT_PIN) == LOW) {
    s_readCount = 0;
    Serial.println("Read count reset");
  }

  delay(100);
}
