#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <nvs_flash.h>

#define SERVICE_UUID                 "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define INSECURE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SECURE_CHARACTERISTIC_UUID   "ff1d2614-e2d6-4c87-9154-6625d39ca7f8"

String serverName = "";
static bool deviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("[SERVER] Client connected");
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("[SERVER] Client disconnected");
    // Restart advertising after disconnect
    BLEDevice::startAdvertising();
    Serial.println("[SERVER] Advertising restarted");
  }
};

class MySecurityCallbacks : public BLESecurityCallbacks {
  // Numeric Comparison callback - both devices display the same PIN
  bool onConfirmPIN(uint32_t pin) override {
    Serial.printf("[SERVER] Numeric comparison PIN: %lu\n", (unsigned long)pin);
    Serial.println("[SERVER] Confirming PIN match");
    // Automatically confirm for testing
    return true;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t desc) override {
    BLEAddress peerAddr(desc.bd_addr);
    Serial.println("[SERVER] Authentication complete");

    uint8_t irk[16];
    if (BLEDevice::getPeerIRK(peerAddr, irk)) {
      Serial.print("[SERVER] Successfully retrieved peer IRK: ");
      for (int i = 0; i < 16; i++) {
        if (irk[i] < 0x10) {
          Serial.print("0");
        }
        Serial.print(irk[i], HEX);
        if (i < 15) {
          Serial.print(":");
        }
      }
      Serial.println();
    }
  }
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
    BLEAddress peerAddr(desc->peer_id_addr.val, desc->peer_id_addr.type);
    Serial.println("[SERVER] Authentication complete");

    uint8_t irk[16];
    if (BLEDevice::getPeerIRK(peerAddr, irk)) {
      Serial.print("[SERVER] Successfully retrieved peer IRK: ");
      for (int i = 0; i < 16; i++) {
        if (irk[i] < 0x10) {
          Serial.print("0");
        }
        Serial.print(irk[i], HEX);
        if (i < 15) {
          Serial.print(":");
        }
      }
      Serial.println();
    }
  }
#endif
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();
    if (value.length() > 0) {
      Serial.printf("[SERVER] Received write: %s\n", value.c_str());
    }
  }

  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.printf("[SERVER] Characteristic read: %s\n", pCharacteristic->getUUID().toString().c_str());
  }
};

void readServerName() {
  Serial.println("[SERVER] Waiting for server name...");
  Serial.println("[SERVER] Send server name:");

  // Wait for server name
  while (serverName.length() == 0) {
    if (Serial.available()) {
      serverName = Serial.readStringUntil('\n');
      serverName.trim();
    }
    delay(100);
  }

  Serial.printf("[SERVER] Server name: %s\n", serverName.c_str());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  Serial.println("[SERVER] Device ready for server name");

  // Read server name from serial
  readServerName();

  Serial.println("[SERVER] Starting BLE Secure Server");

  // Clear NVS to ensure fresh pairing for testing
  nvs_flash_erase();
  nvs_flash_init();

  Serial.printf("[SERVER] BLE stack: %s\n", BLEDevice::getBLEStackString().c_str());

  BLEDevice::init(serverName.c_str());

  // Configure security for Numeric Comparison
  BLESecurity *pSecurity = new BLESecurity();
  // Use DisplayYesNo capability for Numeric Comparison pairing
  pSecurity->setCapability(ESP_IO_CAP_IO);
  // Enable bonding, MITM (required for Numeric Comparison), and secure connection
  pSecurity->setAuthenticationMode(true, true, true);
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());

  // Create server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pServer->advertiseOnDisconnect(true);

  // Create service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create characteristics
  uint32_t insecure_properties = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE;
  uint32_t secure_properties = insecure_properties | BLECharacteristic::PROPERTY_READ_AUTHEN | BLECharacteristic::PROPERTY_WRITE_AUTHEN;

  BLECharacteristic *pSecureChar = pService->createCharacteristic(SECURE_CHARACTERISTIC_UUID, secure_properties);
  BLECharacteristic *pInsecureChar = pService->createCharacteristic(INSECURE_CHARACTERISTIC_UUID, insecure_properties);

  // Set permissions for Bluedroid
  pSecureChar->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
  pInsecureChar->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);

  // Set callbacks
  pSecureChar->setCallbacks(new MyCharacteristicCallbacks());
  pInsecureChar->setCallbacks(new MyCharacteristicCallbacks());

  // Set initial values
  pSecureChar->setValue("Secure Hello World!");
  pInsecureChar->setValue("Insecure Hello World!");

  Serial.println("[SERVER] Characteristics configured");

  // Start service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.printf("[SERVER] Advertising started with name: %s\n", serverName.c_str());
  Serial.printf("[SERVER] Service UUID: %s\n", SERVICE_UUID);
}

void loop() {
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 3000) {
    lastStatus = millis();
    Serial.printf("[SERVER] Status: %s\n", deviceConnected ? "Connected" : "Waiting for connection");
  }
  delay(100);
}
