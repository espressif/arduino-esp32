#include "BLEDevice.h"
#include "BLESecurity.h"
#include "nvs_flash.h"

static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID insecureCharUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID secureCharUUID("ff1d2614-e2d6-4c87-9154-6625d39ca7f8");

String targetServerName = "";
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static boolean testCompleted = false;
static BLEClient *pClient = nullptr;
static BLERemoteCharacteristic *pRemoteInsecureCharacteristic = nullptr;
static BLERemoteCharacteristic *pRemoteSecureCharacteristic = nullptr;
static BLEAdvertisedDevice *myDevice = nullptr;
static bool serviceDiscovered = false;
static bool pinPending = false;
static bool pinLogged = false;
static uint32_t pendingPin = 0;

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient *pclient) {
    Serial.println("[CLIENT] Connected to server");
  }

  void onDisconnect(BLEClient *pclient) {
    connected = false;
    Serial.println("[CLIENT] Disconnected from server");
  }
};

class MySecurityCallbacks : public BLESecurityCallbacks {
  // Numeric Comparison callback - both devices display the same PIN
  bool onConfirmPIN(uint32_t pin) override {
    pendingPin = pin;
    pinPending = true;
    if (serviceDiscovered && !pinLogged) {
      Serial.printf("[CLIENT] Numeric comparison PIN: %lu\n", (unsigned long)pendingPin);
      Serial.println("[CLIENT] Confirming PIN match");
      pinLogged = true;
    }
    // Automatically confirm for testing
    return true;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t desc) override {
    BLEAddress peerAddr(desc.bd_addr);
    Serial.println("[CLIENT] Authentication complete");

    uint8_t irk[16];
    if (BLEDevice::getPeerIRK(peerAddr, irk)) {
      Serial.print("[CLIENT] Successfully retrieved peer IRK: ");
      for (int i = 0; i < 16; i++) {
        if (irk[i] < 0x10) Serial.print("0");
        Serial.print(irk[i], HEX);
        if (i < 15) Serial.print(":");
      }
      Serial.println();
    }
  }
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
    BLEAddress peerAddr(desc->peer_id_addr.val, desc->peer_id_addr.type);
    Serial.println("[CLIENT] Authentication complete");

    uint8_t irk[16];
    if (BLEDevice::getPeerIRK(peerAddr, irk)) {
      Serial.print("[CLIENT] Successfully retrieved peer IRK: ");
      for (int i = 0; i < 16; i++) {
        if (irk[i] < 0x10) Serial.print("0");
        Serial.print(irk[i], HEX);
        if (i < 15) Serial.print(":");
      }
      Serial.println();
    }
  }
#endif
};

bool connectToServer() {
  Serial.printf("[CLIENT] Connecting to %s\n", myDevice->getAddress().toString().c_str());

  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  pClient->connect(myDevice);
  Serial.println("[CLIENT] Physical connection established");

  pClient->setMTU(517);

  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("[CLIENT] ERROR: Failed to find service");
    pClient->disconnect();
    return false;
  }
  Serial.println("[CLIENT] Found service");
  serviceDiscovered = true;
  if (pinPending && !pinLogged) {
    Serial.printf("[CLIENT] Numeric comparison PIN: %lu\n", (unsigned long)pendingPin);
    Serial.println("[CLIENT] Confirming PIN match");
    pinLogged = true;
  }

  pRemoteInsecureCharacteristic = pRemoteService->getCharacteristic(insecureCharUUID);
  if (pRemoteInsecureCharacteristic == nullptr) {
    Serial.println("[CLIENT] ERROR: Failed to find insecure characteristic");
    pClient->disconnect();
    return false;
  }
  Serial.println("[CLIENT] Found insecure characteristic");

  pRemoteSecureCharacteristic = pRemoteService->getCharacteristic(secureCharUUID);
  if (pRemoteSecureCharacteristic == nullptr) {
    Serial.println("[CLIENT] ERROR: Failed to find secure characteristic");
    pClient->disconnect();
    return false;
  }
  Serial.println("[CLIENT] Found secure characteristic");

  // Read insecure characteristic
  if (pRemoteInsecureCharacteristic->canRead()) {
    String value = pRemoteInsecureCharacteristic->readValue();
    Serial.print("[CLIENT] Insecure characteristic value: ");
    Serial.println(value.c_str());
  }

  // Set auth requirement for secure characteristic (Bluedroid)
  pRemoteSecureCharacteristic->setAuth(ESP_GATT_AUTH_REQ_MITM);

  // Read secure characteristic (triggers authentication in NimBLE)
  if (pRemoteSecureCharacteristic->canRead()) {
    Serial.println("[CLIENT] Reading secure characteristic...");
    String value = pRemoteSecureCharacteristic->readValue();
    Serial.print("[CLIENT] Secure characteristic value: ");
    Serial.println(value.c_str());
  }

  connected = true;
  Serial.println("[CLIENT] Connection and authentication successful");
  return true;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("[CLIENT] Found device: ");
    Serial.println(advertisedDevice.toString().c_str());

    // Check if device has the correct service UUID and name
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      String deviceName = advertisedDevice.getName().c_str();
      Serial.print("[CLIENT] Device has matching service UUID, name: ");
      Serial.println(deviceName);

      if (deviceName == targetServerName) {
        Serial.println("[CLIENT] Found target server!");
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;
      }
    }
  }
};

void readServerName() {
  Serial.println("[CLIENT] Waiting for server name...");
  Serial.println("[CLIENT] Send server name:");

  // Wait for server name
  while (targetServerName.length() == 0) {
    if (Serial.available()) {
      targetServerName = Serial.readStringUntil('\n');
      targetServerName.trim();
    }
    delay(100);
  }

  Serial.print("[CLIENT] Target server name: ");
  Serial.println(targetServerName);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  Serial.println("[CLIENT] Device ready for server name");

  // Read server name from serial
  readServerName();

  Serial.println("[CLIENT] Starting BLE Secure Client");

  // Clear NVS to ensure fresh pairing for testing
  nvs_flash_erase();
  nvs_flash_init();

  BLEDevice::init("BLE_Test_Client");

  // Configure security for Numeric Comparison
  BLESecurity *pSecurity = new BLESecurity();
  // Use DisplayYesNo capability for Numeric Comparison pairing
  pSecurity->setCapability(ESP_IO_CAP_IO);
  // Enable bonding, MITM (required for Numeric Comparison), and secure connection
  pSecurity->setAuthenticationMode(true, true, true);
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());

  Serial.print("[CLIENT] Scanning for server: ");
  Serial.println(targetServerName);

  // Start scanning
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30, false);
}

void loop() {
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("[CLIENT] Successfully connected and authenticated");
    } else {
      Serial.println("[CLIENT] Connection failed");
    }
    doConnect = false;
  }

  if (connected && !testCompleted) {
    // Test write and read operations once
    testCompleted = true;

    // Write to insecure characteristic
    String insecureWriteValue = "Test Insecure Write";
    if (pRemoteInsecureCharacteristic && pRemoteInsecureCharacteristic->canWrite()) {
      pRemoteInsecureCharacteristic->writeValue(insecureWriteValue.c_str(), insecureWriteValue.length());
      Serial.printf("[CLIENT] Wrote to insecure characteristic: %s\n", insecureWriteValue.c_str());
    }

    // Read back insecure characteristic
    if (pRemoteInsecureCharacteristic && pRemoteInsecureCharacteristic->canRead()) {
      String insecureReadValue = pRemoteInsecureCharacteristic->readValue();
      Serial.printf("[CLIENT] Read from insecure characteristic: %s\n", insecureReadValue.c_str());
    }

    // Write to secure characteristic
    String secureWriteValue = "Test Secure Write";
    if (pRemoteSecureCharacteristic && pRemoteSecureCharacteristic->canWrite()) {
      pRemoteSecureCharacteristic->writeValue(secureWriteValue.c_str(), secureWriteValue.length());
      Serial.printf("[CLIENT] Wrote to secure characteristic: %s\n", secureWriteValue.c_str());
    }

    // Read back secure characteristic
    if (pRemoteSecureCharacteristic && pRemoteSecureCharacteristic->canRead()) {
      String secureReadValue = pRemoteSecureCharacteristic->readValue();
      Serial.printf("[CLIENT] Read from secure characteristic: %s\n", secureReadValue.c_str());
    }

    Serial.println("[CLIENT] Test operations completed");
  }

  delay(1000);
}

