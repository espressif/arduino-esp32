#include <Arduino.h>
#include "BLEDevice.h"
#include "BLESecurity.h"
#include "nvs_flash.h"
#include <esp_heap_caps.h>

static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID insecureCharUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID secureCharUUID("ff1d2614-e2d6-4c87-9154-6625d39ca7f8");
static BLEUUID writeNrCharUUID("d7c1aa01-36e1-4688-b7f5-ea07361b26a8");

static const int RECONNECT_CYCLES = 10;
static const uint16_t APPID_PRESEED_GATT_IF_NONE = 250;  // crosses ESP_GATT_IF_NONE (0xFF on Bluedroid)
static const uint8_t WRITE_NR_BURST[3][3] = {
  {0xAA, 0xAA, 0xAA},
  {0xBB, 0xBB, 0xBB},
  {0xCC, 0xCC, 0xCC},
};

// Service UUIDs the server only advertises inside malformed AD structures. A
// fixed parser must never surface them (see the payloads in server.ino).
static BLEUUID badUuid16((uint16_t)0x180D);
static BLEUUID badUuid32((uint32_t)0x04030201);

String targetServerName = "";
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static boolean testCompleted = false;

// Static passkeys for the Passkey Entry phase. The wrong one must be rejected.
static const uint32_t PASSKEY_CORRECT = 123456;
static const uint32_t PASSKEY_WRONG = 654321;

// Outcome of the most recent pairing attempt, written from the security callbacks.
enum AuthResult {
  AUTH_PENDING,
  AUTH_SUCCESS,
  AUTH_FAILURE
};
static volatile AuthResult authResult = AUTH_PENDING;

static int adPhase = 1;
static boolean adPhase2Reported = false;
static BLEClient *pClient = nullptr;
static BLERemoteCharacteristic *pRemoteInsecureCharacteristic = nullptr;
static BLERemoteCharacteristic *pRemoteSecureCharacteristic = nullptr;
static BLERemoteCharacteristic *pRemoteWriteNrCharacteristic = nullptr;
static BLEAdvertisedDevice *myDevice = nullptr;

static void printHeapIntegrity(const char *when) {
  bool clean = heap_caps_check_integrity_all(true);
  Serial.printf("[CLIENT] Heap %s: %s\n", when, clean ? "clean" : "CORRUPT");
}

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

  // Passkey Entry phase only. With KeyboardOnly capability the stack asks us for the
  // passkey the peer displays. A static passkey is configured, so this only runs if
  // that lookup ever fails.
  uint32_t onPassKeyRequest() override {
    uint32_t passkey = BLESecurity::getPassKey();
    Serial.printf("[CLIENT] Passkey request, injecting: %06lu\n", (unsigned long)passkey);
    return passkey;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t desc) override {
    // Bluedroid reports both outcomes through this callback, so the result has to be
    // checked here. Without it a rejected pairing looks exactly like a successful one.
    if (!desc.success) {
      authResult = AUTH_FAILURE;
      Serial.printf("[CLIENT] Authentication failed: reason=%u (0x%02X)\n", desc.fail_reason, desc.fail_reason);
      return;
    }

    authResult = AUTH_SUCCESS;
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
  // The status-aware overload is used so a rejected pairing is reported instead of
  // silently looking like a successful one.
  void onAuthenticationComplete(ble_gap_conn_desc *desc, int status) override {
    if (status != 0) {
      authResult = AUTH_FAILURE;
      Serial.printf("[CLIENT] Authentication failed: status=%d\n", status);
      return;
    }

    authResult = AUTH_SUCCESS;
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

  pRemoteWriteNrCharacteristic = pRemoteService->getCharacteristic(writeNrCharUUID);
  if (pRemoteWriteNrCharacteristic == nullptr) {
    Serial.println("[CLIENT] ERROR: Failed to find Write-NR characteristic");
    pClient->disconnect();
    return false;
  }
  Serial.println("[CLIENT] Found Write-NR characteristic");

  // Read insecure characteristic
  if (pRemoteInsecureCharacteristic->canRead()) {
    String value = pRemoteInsecureCharacteristic->readValue();
    Serial.print("[CLIENT] Insecure characteristic value: ");
    Serial.println(value.c_str());
  }

  // Set auth requirement for secure characteristic (Bluedroid)
  pRemoteSecureCharacteristic->setAuth(ESP_GATT_AUTH_REQ_MITM);

  // Read secure characteristic (triggers on-demand authentication for both stacks)
  authResult = AUTH_PENDING;
  if (pRemoteSecureCharacteristic->canRead()) {
    Serial.println("[CLIENT] Reading secure characteristic...");
    String value = pRemoteSecureCharacteristic->readValue();
    Serial.print("[CLIENT] Secure characteristic value: ");
    Serial.println(value.c_str());
  }

  // The read above only triggers pairing; the outcome arrives asynchronously through
  // the security callbacks. Reporting success without checking it is exactly what made
  // the failure in issue #12860 look like a working pairing.
  unsigned long authStart = millis();
  while (authResult == AUTH_PENDING && (millis() - authStart) < 10000) {
    delay(50);
  }

  if (authResult != AUTH_SUCCESS) {
    Serial.println("[CLIENT] ERROR: Authentication did not complete successfully");
    pClient->disconnect();
    return false;
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

// Drops every bond this device holds. Without this the next attempt would re-encrypt
// the link with the stored LTK and never run Passkey Entry again.
static void forgetBonds() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  int count = esp_ble_get_bond_device_num();
  if (count > 0) {
    esp_ble_bond_dev_t *list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * count);
    if (list != nullptr) {
      esp_ble_get_bond_device_list(&count, list);
      for (int i = 0; i < count; i++) {
        esp_ble_remove_bond_device(list[i].bd_addr);
      }
      free(list);
    }
  }
#elif defined(CONFIG_NIMBLE_ENABLED)
  ble_store_clear();
#endif
}

// One Passkey Entry attempt: connect, discover, then read the secure characteristic
// to trigger on-demand pairing with the given passkey. Reports whether the peer
// accepted it. A pairing that never completes is reported as a rejection so the test
// cannot hang on a stack that goes silent.
static bool runPasskeyAttempt(const char *label, uint32_t passkey) {
  forgetBonds();
  BLESecurity::setPassKey(true, passkey);

  authResult = AUTH_PENDING;

  BLEClient *pPasskeyClient = BLEDevice::createClient();
  if (pPasskeyClient == nullptr) {
    Serial.printf("[CLIENT] %s FAILED: createClient returned null\n", label);
    return false;
  }

  if (!pPasskeyClient->connect(myDevice->getAddress(), myDevice->getAddressType(), 10000)) {
    Serial.printf("[CLIENT] %s FAILED: connect failed\n", label);
    delete pPasskeyClient;
    return false;
  }

  bool paired = false;
  BLERemoteService *pSvc = pPasskeyClient->getService(serviceUUID);
  if (pSvc == nullptr) {
    Serial.printf("[CLIENT] %s FAILED: service not found\n", label);
  } else {
    BLERemoteCharacteristic *pChr = pSvc->getCharacteristic(secureCharUUID);
    if (pChr == nullptr) {
      Serial.printf("[CLIENT] %s FAILED: secure characteristic not found\n", label);
    } else {
      pChr->setAuth(ESP_GATT_AUTH_REQ_MITM);
      Serial.printf("[CLIENT] %s reading secure characteristic\n", label);
      pChr->readValue();

      unsigned long start = millis();
      while (authResult == AUTH_PENDING && (millis() - start) < 30000) {
        delay(50);
      }
      paired = (authResult == AUTH_SUCCESS);
    }
  }

  pPasskeyClient->disconnect();
  delete pPasskeyClient;
  // Give both sides time to tear the link down and restart advertising.
  delay(2000);

  Serial.printf("[CLIENT] %s result: %s\n", label, paired ? "PAIRED" : "REJECTED");
  return paired;
}

// Passkey Entry coverage. The correct passkey runs first: a peer that has just
// rejected a pairing answers the next attempt with "Repeated Attempts" (SMP reason 9)
// rather than running Passkey Entry again, which would mask the real result.
void runPasskeyPhase() {
  Serial.println("[CLIENT] Starting passkey entry phase");
  BLESecurity::setCapability(ESP_IO_CAP_IN);

  bool correctPaired = runPasskeyAttempt("Passkey correct", PASSKEY_CORRECT);
  bool wrongPaired = runPasskeyAttempt("Passkey wrong", PASSKEY_WRONG);

  if (correctPaired && !wrongPaired) {
    Serial.println("[CLIENT] Passkey entry phase PASSED");
  } else {
    Serial.println("[CLIENT] Passkey entry phase FAILED");
  }
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

  // Issue #12821: BLEDevice::init() must not overflow a heap block / destroy the tail canary.
  printHeapIntegrity("before BLEDevice::init()");
  BLEDevice::init("BLE_Test_Client");
  printHeapIntegrity("after BLEDevice::init()");

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
    } else if (command == "PSKPHASE") {
      runPasskeyPhase();
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

    // --- Write-Without-Response burst (issue #12815) ---
    // Three back-to-back Write Commands with distinct payloads. The server must
    // deliver each original packet to onWrite(), not three copies of the last one.
    Serial.println("[CLIENT] Starting Write-NR burst");
    if (pRemoteWriteNrCharacteristic && pRemoteWriteNrCharacteristic->canWriteNoResponse()) {
      for (int i = 0; i < 3; i++) {
        uint8_t buf[3];
        memcpy(buf, WRITE_NR_BURST[i], sizeof(buf));
        bool ok = pRemoteWriteNrCharacteristic->writeValue(buf, sizeof(buf), false);
        Serial.printf("[CLIENT] Write-NR packet %d/3 %s\n", i + 1, ok ? "queued" : "FAILED");
      }
      Serial.println("[CLIENT] Write-NR burst sent");
    } else {
      Serial.println("[CLIENT] ERROR: Write-NR characteristic missing or cannot write without response");
    }
    delay(1000);

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
