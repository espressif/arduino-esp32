#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <nvs_flash.h>
#include <esp_heap_caps.h>

#define SERVICE_UUID                 "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define INSECURE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SECURE_CHARACTERISTIC_UUID   "ff1d2614-e2d6-4c87-9154-6625d39ca7f8"
#define WRITE_NR_CHARACTERISTIC_UUID "d7c1aa01-36e1-4688-b7f5-ea07361b26a8"

// Static passkey used by the Passkey Entry phase.
#define PASSKEY_PIN 123456

static const int WRITE_NR_BURST_COUNT = 3;
static const uint8_t WRITE_NR_EXPECTED[WRITE_NR_BURST_COUNT][3] = {
  {0xAA, 0xAA, 0xAA},
  {0xBB, 0xBB, 0xBB},
  {0xCC, 0xCC, 0xCC},
};

String serverName = "";
static bool deviceConnected = false;
static int connectionCount = 0;
static uint8_t writeNrReceived[WRITE_NR_BURST_COUNT][3] = {};
static size_t writeNrReceivedLen[WRITE_NR_BURST_COUNT] = {};
static volatile int writeNrCount = 0;
static volatile bool writeNrBurstDone = false;
static bool writeNrBurstReported = false;

static void printHeapIntegrity(const char *when) {
  bool clean = heap_caps_check_integrity_all(true);
  Serial.printf("[SERVER] Heap %s: %s\n", when, clean ? "clean" : "CORRUPT");
}

static void printHexBytes(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
    if (i + 1 < len) {
      Serial.print(' ');
    }
  }
}

// Malformed AD structures used to regression-test BLEAdvertisedDevice parsing.
// They are split across two advertising windows because the terminator and the
// oversize guard both stop the parse loop, so each one has to be the last
// structure in its own payload to be observable.
//
// Window 1: guards that skip a single structure, followed by the oversize guard.
// 16-bit UUID list with a trailing partial octet (3 data bytes, not a multiple of 2).
static char badUuid16List[] = {0x04, 0x03, 0x0D, 0x18, 0x99};
// 128-bit UUID structure carrying fewer than 16 data bytes.
static char badUuid128[] = {0x03, 0x07, 0x11, 0x22};
// AD length claims 20 bytes (type + data) but only 3 bytes follow.
static char oversizeAd[] = {20, (char)0xFF, 0x01, 0x02};
//
// Window 2: the 32-bit guard, then a terminator that must end the parse.
// 32-bit UUID list with a trailing partial group (5 data bytes, not a multiple of 4).
static char badUuid32List[] = {0x06, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05};
// Zero-length AD ends the payload; the manufacturer data behind it must be ignored.
static char terminatedAd[] = {0x00, 0x03, (char)0xFF, (char)0xAA, (char)0xBB};

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    connectionCount++;
    Serial.printf("[SERVER] Client connected (count: %d)\n", connectionCount);
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.printf("[SERVER] Client disconnected (count: %d)\n", connectionCount);
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

  // Passkey Entry phase only. With DisplayOnly capability the stack asks us to show
  // the passkey the peer has to enter, which is how the test tells the two pairing
  // methods apart: Numeric Comparison would call onConfirmPIN() instead.
  void onPassKeyNotify(uint32_t passkey) override {
    Serial.printf("[SERVER] Passkey notify: %06lu\n", (unsigned long)passkey);
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t desc) override {
    // Bluedroid reports both outcomes through this callback, so the result has to be
    // checked here. Without it a rejected pairing looks exactly like a successful one.
    if (!desc.success) {
      Serial.printf("[SERVER] Authentication failed: reason=%u (0x%02X)\n", desc.fail_reason, desc.fail_reason);
      return;
    }

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
  // The status-aware overload is used so a rejected pairing is reported instead of
  // silently looking like a successful one.
  void onAuthenticationComplete(ble_gap_conn_desc *desc, int status) override {
    if (status != 0) {
      Serial.printf("[SERVER] Authentication failed: status=%d\n", status);
      return;
    }

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
    // Copy getValue() immediately. Issue #12815: a Write-Without-Response burst can
    // make later onWrite() callbacks all observe the last packet if the characteristic
    // buffer is overwritten before the previous callback finishes reading it.
    if (pCharacteristic->getUUID().equals(BLEUUID(WRITE_NR_CHARACTERISTIC_UUID))) {
      String rxValue = pCharacteristic->getValue();
      int idx = writeNrCount;
      if (idx < WRITE_NR_BURST_COUNT) {
        size_t len = rxValue.length();
        if (len > sizeof(writeNrReceived[idx])) {
          len = sizeof(writeNrReceived[idx]);
        }
        writeNrReceivedLen[idx] = len;
        for (size_t i = 0; i < len; i++) {
          writeNrReceived[idx][i] = static_cast<uint8_t>(rxValue[i]);
        }
        Serial.printf("[SERVER] Write-NR packet %d/%d: ", idx + 1, WRITE_NR_BURST_COUNT);
        printHexBytes(writeNrReceived[idx], len);
        Serial.println();
        writeNrCount = idx + 1;
        if (writeNrCount >= WRITE_NR_BURST_COUNT) {
          writeNrBurstDone = true;
        }
      }
      return;
    }

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

  // Issue #12821: BLEDevice::init() must not overflow a heap block / destroy the tail canary.
  printHeapIntegrity("before BLEDevice::init()");
  BLEDevice::init(serverName.c_str());
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

  // Create server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pServer->advertiseOnDisconnect(true);

  // Create service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create characteristics
  uint32_t insecure_properties = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE;
  uint32_t secure_properties = insecure_properties | BLECharacteristic::PROPERTY_READ_AUTHEN | BLECharacteristic::PROPERTY_WRITE_AUTHEN;

  uint32_t write_nr_properties = BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR;

  BLECharacteristic *pSecureChar = pService->createCharacteristic(SECURE_CHARACTERISTIC_UUID, secure_properties);
  BLECharacteristic *pInsecureChar = pService->createCharacteristic(INSECURE_CHARACTERISTIC_UUID, insecure_properties);
  BLECharacteristic *pWriteNrChar = pService->createCharacteristic(WRITE_NR_CHARACTERISTIC_UUID, write_nr_properties);

  // Set permissions for Bluedroid
  pSecureChar->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
  pInsecureChar->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  pWriteNrChar->setAccessPermissions(ESP_GATT_PERM_WRITE);

  // Set callbacks
  pSecureChar->setCallbacks(new MyCharacteristicCallbacks());
  pInsecureChar->setCallbacks(new MyCharacteristicCallbacks());
  pWriteNrChar->setCallbacks(new MyCharacteristicCallbacks());

  // Set initial values
  pSecureChar->setValue("Secure Hello World!");
  pInsecureChar->setValue("Insecure Hello World!");

  Serial.println("[SERVER] Characteristics configured");

  // Start service
  pService->start();

  // Start advertising. The scan response carries the name plus the window 1
  // malformed AD structures (see above); the primary advertisement is left to
  // the library so the service UUID still round-trips normally.
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);

  BLEAdvertisementData scanResponse;
  scanResponse.setName(serverName);
  scanResponse.addData(badUuid16List, sizeof(badUuid16List));
  scanResponse.addData(badUuid128, sizeof(badUuid128));
  scanResponse.addData(oversizeAd, sizeof(oversizeAd));
  pAdvertising->setScanResponseData(scanResponse);

  BLEDevice::startAdvertising();

  Serial.printf("[SERVER] Advertising started with name: %s\n", serverName.c_str());
  Serial.printf("[SERVER] Service UUID: %s\n", SERVICE_UUID);
}

// Drops every bond this device holds. Without this the next pairing attempt would
// re-encrypt the link with the stored LTK instead of running the pairing method
// the test wants to exercise.
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

// Switch from Numeric Comparison to Passkey Entry. DisplayOnly here against
// KeyboardOnly on the client selects Passkey Entry, which is the method used by the
// Server_secure_static_passkey example and the one reported broken in issue #12860.
static void startPasskeyPhase() {
  forgetBonds();
  BLESecurity::setPassKey(true, PASSKEY_PIN);
  BLESecurity::setCapability(ESP_IO_CAP_OUT);
  Serial.println("[SERVER] Passkey entry mode ready");
}

// Swap the scan response for the window 2 payload (32-bit guard + terminator).
void startAdvertisingPhase2() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->stop();

  BLEAdvertisementData scanResponse;
  scanResponse.setName(serverName);
  scanResponse.addData(badUuid32List, sizeof(badUuid32List));
  scanResponse.addData(terminatedAd, sizeof(terminatedAd));
  pAdvertising->setScanResponseData(scanResponse);

  BLEDevice::startAdvertising();
  // Configuring the payload is asynchronous on both stacks.
  delay(500);
  Serial.println("[SERVER] AD phase 2 advertising");
}

void loop() {
  if (writeNrBurstDone && !writeNrBurstReported) {
    writeNrBurstReported = true;
    bool ok = true;
    for (int i = 0; i < WRITE_NR_BURST_COUNT; i++) {
      if (writeNrReceivedLen[i] != 3) {
        ok = false;
        break;
      }
      for (int b = 0; b < 3; b++) {
        if (writeNrReceived[i][b] != WRITE_NR_EXPECTED[i][b]) {
          ok = false;
          break;
        }
      }
    }
    Serial.printf("[SERVER] Write-NR burst %s\n", ok ? "PASSED" : "FAILED");
    printHeapIntegrity("after Write-NR burst");
  }

  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "ADPHASE2") {
      startAdvertisingPhase2();
    } else if (command == "PSKPHASE") {
      startPasskeyPhase();
    }
  }

  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 3000) {
    lastStatus = millis();
    Serial.printf("[SERVER] Status: %s\n", deviceConnected ? "Connected" : "Waiting for connection");
  }
  delay(100);
}
