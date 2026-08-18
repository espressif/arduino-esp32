#include <Arduino.h>
#include "BLEDevice.h"
#include "BLESecurity.h"
#include "nvs_flash.h"

static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID insecureCharUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID secureCharUUID("ff1d2614-e2d6-4c87-9154-6625d39ca7f8");

static const int RECONNECT_CYCLES = 10;
static const uint16_t APPID_PRESEED_GATT_IF_NONE = 250;  // crosses ESP_GATT_IF_NONE (0xFF on Bluedroid)

// Service UUIDs the server only advertises inside malformed AD structures. A
// fixed parser must never surface them (see the payloads in server.ino).
static BLEUUID badUuid16((uint16_t)0x180D);
static BLEUUID badUuid32((uint32_t)0x04030201);

String targetServerName = "";
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static boolean testCompleted = false;
static int adPhase = 1;
static boolean adPhase2Reported = false;
static BLEClient *pClient = nullptr;
static BLERemoteCharacteristic *pRemoteInsecureCharacteristic = nullptr;
static BLERemoteCharacteristic *pRemoteSecureCharacteristic = nullptr;
static BLEAdvertisedDevice *myDevice = nullptr;

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
    Serial.printf("[CLIENT] Numeric comparison PIN: %lu\n", (unsigned long)pin);
    Serial.println("[CLIENT] Confirming PIN match");
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
    Serial.println("[CLIENT] Authentication complete");

    uint8_t irk[16];
    if (BLEDevice::getPeerIRK(peerAddr, irk)) {
      Serial.print("[CLIENT] Successfully retrieved peer IRK: ");
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

  // Read secure characteristic (triggers on-demand authentication for both stacks)
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
        if (adPhase == 2) {
          if (!adPhase2Reported) {
            adPhase2Reported = true;
            // Behind the zero-length terminator the server hides manufacturer data,
            // and the 32-bit UUID list it advertises has a trailing partial group.
            Serial.printf(
              "[CLIENT] ADCHK2 mfg=%d svc32=%d\n", advertisedDevice.haveManufacturerData() ? 1 : 0, advertisedDevice.isAdvertisingService(badUuid32) ? 1 : 0
            );
          }
          return;
        }

        // The scan response hides an oversize AD structure, a 16-bit UUID list with
        // a trailing partial octet, and a 128-bit UUID structure shorter than 16
        // bytes. Only the service UUID from the primary advertisement is valid, so
        // a fixed parser reports exactly one service UUID and no manufacturer data.
        Serial.printf(
          "[CLIENT] ADCHK1 mfg=%d svc16=%d svcCount=%d\n", advertisedDevice.haveManufacturerData() ? 1 : 0,
          advertisedDevice.isAdvertisingService(badUuid16) ? 1 : 0, advertisedDevice.getServiceUUIDCount()
        );
        Serial.println("[CLIENT] Found target server!");
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;
      }
    }
  }
};

static MyAdvertisedDeviceCallbacks *pScanCallbacks = nullptr;

// Rescan while the server advertises the window 2 payload (see server.ino).
void runAdPhase2Scan() {
  adPhase = 2;
  adPhase2Reported = false;

  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->stop();
  pBLEScan->clearResults();
  // Duplicates are wanted so the report is not suppressed by the earlier scan.
  pBLEScan->setAdvertisedDeviceCallbacks(pScanCallbacks, true);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(10, false);

  if (!adPhase2Reported) {
    Serial.println("[CLIENT] ADCHK2 server not found");
  }
}

bool runReconnectPhase(uint16_t preseed, const char *label) {
  Serial.printf("[CLIENT] Reconnect phase: crossing %s (preseed=%u)\n", label, preseed);
  BLEDevice::m_appId = preseed;

  for (int i = 0; i < RECONNECT_CYCLES; i++) {
    Serial.printf("[CLIENT] Reconnect cycle %d/%d starting\n", i + 1, RECONNECT_CYCLES);

    BLEClient *pReconnectClient = BLEDevice::createClient();
    if (pReconnectClient == nullptr) {
      Serial.printf("[CLIENT] Reconnect cycle %d/%d FAILED: createClient returned null\n", i + 1, RECONNECT_CYCLES);
      return false;
    }

    if (!pReconnectClient->connect(myDevice->getAddress(), myDevice->getAddressType(), 10000)) {
      Serial.printf("[CLIENT] Reconnect cycle %d/%d FAILED: connect failed\n", i + 1, RECONNECT_CYCLES);
      delete pReconnectClient;
      return false;
    }

    BLERemoteService *pSvc = pReconnectClient->getService(serviceUUID);
    if (pSvc != nullptr) {
      BLERemoteCharacteristic *pChr = pSvc->getCharacteristic(insecureCharUUID);
      if (pChr != nullptr && pChr->canRead()) {
        String val = pChr->readValue();
        Serial.printf("[CLIENT] Reconnect cycle %d/%d read: %s\n", i + 1, RECONNECT_CYCLES, val.c_str());
      }
    }

    pReconnectClient->disconnect();
    delete pReconnectClient;
    Serial.printf("[CLIENT] Reconnect cycle %d/%d OK\n", i + 1, RECONNECT_CYCLES);
  }

  Serial.printf("[CLIENT] Reconnect phase %s PASSED\n", label);
  return true;
}

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
  // Disable forced auth on connect so authentication is triggered on-demand
  // when reading the secure characteristic (consistent ordering for testing)
  pSecurity->setForceAuthentication(false);
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());

  Serial.print("[CLIENT] Scanning for server: ");
  Serial.println(targetServerName);

  // Start scanning
  BLEScan *pBLEScan = BLEDevice::getScan();
  pScanCallbacks = new MyAdvertisedDeviceCallbacks();
  pBLEScan->setAdvertisedDeviceCallbacks(pScanCallbacks);
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30, false);
}

void loop() {
  if (testCompleted && Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "ADPHASE2") {
      runAdPhase2Scan();
    }
  }

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

    // --- Reconnection stress test (app_id collision regression) ---
    // Disconnect from the current connection first
    Serial.println("[CLIENT] Starting reconnection stress test");
    pClient->disconnect();
    delay(1000);

    // Cross ESP_GATT_IF_NONE boundary (0xFF on Bluedroid, 0xFFFF on NimBLE).
    // Each new BLEClient gets its ID from the global counter, so pre-seeding the
    // counter to 250 forces the first few clients to cross the 0xFF boundary.
    bool passed = runReconnectPhase(APPID_PRESEED_GATT_IF_NONE, "ESP_GATT_IF_NONE");

    if (passed) {
      Serial.println("[CLIENT] Reconnection stress test PASSED");
    } else {
      Serial.println("[CLIENT] Reconnection stress test FAILED");
    }
  }

  delay(1000);
}
