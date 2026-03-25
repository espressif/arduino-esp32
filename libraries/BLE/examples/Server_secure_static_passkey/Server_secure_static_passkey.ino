/*
  Secure server with static passkey

  This example demonstrates how to create a secure BLE server with no
  IO capability using a static passkey.
  The server will accept connections from devices that have the same passkey set.
  The example passkey is set to 123456.
  The server will create a service and a secure and an insecure characteristic
  to be used as example.

  This server is designed to be used with the Client_secure_static_passkey example.

  After successful bonding, the example demonstrates how to retrieve the
  peer's and local Identity Resolving Key (IRK) in multiple formats:
  - Comma-separated hex format: 0x1A,0x1B,0x1C,...
  - Base64 encoded (for Home Assistant Private BLE Device service)
  - Reverse hex order (for Home Assistant ESPresense)

  WARNING: THE IRK IS A LONG-TERM IDENTIFIER OF THE DEVICE. ANYONE WITH THE IRK CAN
  USE IT TO TRACK OR IMPERSONATE THE DEVICE IN BLE PRESENCE SYSTEMS. USE WITH CAUTION.

  Note that ESP32 uses Bluedroid by default and the other SoCs use NimBLE.
  Bluedroid initiates security on-connect, while NimBLE initiates security on-demand.
  This means that in NimBLE you can read the insecure characteristic without entering
  the passkey. This is not possible in Bluedroid.

  IMPORTANT: MITM (Man-In-The-Middle protection) must be enabled for password prompts
  to work. Without MITM, the BLE stack assumes no user interaction is needed and will use
  "Just Works" pairing method (with encryption if secure connection is enabled).

  Based on examples from Neil Kolban and h2zero.
  Created by lucasssvaz.
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <nvs_flash.h>
#include <string>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID                 "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define INSECURE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SECURE_CHARACTERISTIC_UUID   "ff1d2614-e2d6-4c87-9154-6625d39ca7f8"

// This is an example passkey. You should use a different or random passkey.
#define SERVER_PIN 123456

// Print an IRK buffer as hex with leading zeros and ':' separator
static void printIrkBinary(uint8_t *irk) {
  for (int i = 0; i < 16; i++) {
    if (irk[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(irk[i], HEX);
    if (i < 15) {
      Serial.print(":");
    }
  }
}

static void get_local_irk() {
  Serial.println("\n=== Retrieving local IRK (this device) ===\n");

  String irkString = BLEDevice::getLocalIRKString();
  String irkBase64 = BLEDevice::getLocalIRKBase64();
  String irkReverse = BLEDevice::getLocalIRKReverse();

  if (irkString.length() > 0) {
    Serial.println("Successfully retrieved local IRK in multiple formats:\n");
    Serial.print("IRK (comma-separated hex): ");
    Serial.println(irkString);
    Serial.print("IRK (Base64 for Home Assistant Private BLE Device): ");
    Serial.println(irkBase64);
    Serial.print("IRK (reverse hex for Home Assistant ESPresense): ");
    Serial.println(irkReverse);
    Serial.println();
  } else {
    Serial.println("!!! Failed to retrieve local IRK !!!");
  }

  Serial.println("==========================================\n");
}

static void get_peer_irk(BLEAddress peerAddr) {
  Serial.println("\n=== Retrieving peer IRK (Client) ===\n");

  uint8_t irk[16];

  // Get IRK in binary format
  if (BLEDevice::getPeerIRK(peerAddr, irk)) {
    Serial.println("Successfully retrieved peer IRK in binary format:");
    printIrkBinary(irk);
    Serial.println("\n");
  }

  // Get IRK in different string formats
  String irkString = BLEDevice::getPeerIRKString(peerAddr);
  String irkBase64 = BLEDevice::getPeerIRKBase64(peerAddr);
  String irkReverse = BLEDevice::getPeerIRKReverse(peerAddr);

  if (irkString.length() > 0) {
    Serial.println("Successfully retrieved peer IRK in multiple formats:\n");
    Serial.print("IRK (comma-separated hex): ");
    Serial.println(irkString);
    Serial.print("IRK (Base64 for Home Assistant Private BLE Device): ");
    Serial.println(irkBase64);
    Serial.print("IRK (reverse hex for Home Assistant ESPresense): ");
    Serial.println(irkReverse);
    Serial.println();
  } else {
    Serial.println("!!! Failed to retrieve peer IRK !!!");
    Serial.println("This is expected if bonding is disabled or the peer doesn't distribute its Identity Key.");
    Serial.println("To enable bonding, change setAuthenticationMode to: pSecurity->setAuthenticationMode(true, true, true);\n");
  }

  Serial.println("=======================================\n");
}

// Security callbacks to print IRKs once authentication completes
class MySecurityCallbacks : public BLESecurityCallbacks {
#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t desc) override {
    // desc.bd_addr is the peer's connection address (may be a Resolvable Private Address).
    // getPeerIRK() will also search by the stored identity address as a fallback.
    BLEAddress peerAddr(desc.bd_addr);
    get_peer_irk(peerAddr);
  }
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
    // peer_id_addr is always the resolved identity address in NimBLE
    BLEAddress peerAddr(desc->peer_id_addr.val, desc->peer_id_addr.type);
    get_peer_irk(peerAddr);
  }
#endif
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  // WARNING: nvs_flash_erase() wipes the entire NVS, including the Bluetooth identity keys (IR/IRK).
  // This causes the BLE stack to generate a new IRK on every boot, so the IRK will change each time.
  // This is intentional here to force fresh authentication for testing purposes.
  //
  // For production use where a stable IRK is needed (e.g. Home Assistant presence detection),
  // remove these lines. The server's IRK will remain the same across reboots as long as NVS is intact.
  // To clear only bond data without affecting identity keys, use esp_ble_remove_bond_device() (Bluedroid)
  // or ble_store_util_delete_all() (NimBLE) instead.
  Serial.println("Clearing NVS pairing data...");
  nvs_flash_erase();
  nvs_flash_init();

  Serial.print("Using BLE stack: ");
  Serial.println(BLEDevice::getBLEStackString());

  BLEDevice::init("Secure BLE Server");

  // Display this device's own local IRK
  get_local_irk();

  BLESecurity *pSecurity = new BLESecurity();

  // Set security parameters
  // Default parameters:
  // - IO capability is set to NONE
  // - Initiator and responder key distribution flags are set to both encryption and identity keys.
  // - Passkey is set to BLE_SM_DEFAULT_PASSKEY (123456). It will warn if you don't change it.
  // - Key size is set to 16 bytes

  // Set static passkey
  // The first argument defines if the passkey is static or random.
  // The second argument is the passkey (ignored when using a random passkey).
  pSecurity->setPassKey(true, SERVER_PIN);

  // Set IO capability to DisplayOnly
  // We need the proper IO capability for MITM authentication even
  // if the passkey is static and won't be shown to the user
  // See https://www.bluetooth.com/blog/bluetooth-pairing-part-2-key-generation-methods/
  pSecurity->setCapability(ESP_IO_CAP_OUT);

  // Set authentication mode
  // Enable bonding, MITM (for password prompts), and secure connection for this example
  pSecurity->setAuthenticationMode(true, true, true);

  // Set callbacks to handle authentication completion and print IRKs
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());

  BLEServer *pServer = BLEDevice::createServer();
  pServer->advertiseOnDisconnect(true);

  BLEService *pService = pServer->createService(SERVICE_UUID);

  uint32_t insecure_properties = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE;
  uint32_t secure_properties = insecure_properties;

  // NimBLE uses properties to secure characteristics.
  // These special permission properties are not supported by Bluedroid and will be ignored.
  // This can be removed if only using Bluedroid (ESP32).
  // Check the BLECharacteristic.h file for more information.
  secure_properties |= BLECharacteristic::PROPERTY_READ_AUTHEN | BLECharacteristic::PROPERTY_WRITE_AUTHEN;

  BLECharacteristic *pSecureCharacteristic = pService->createCharacteristic(SECURE_CHARACTERISTIC_UUID, secure_properties);
  BLECharacteristic *pInsecureCharacteristic = pService->createCharacteristic(INSECURE_CHARACTERISTIC_UUID, insecure_properties);

  // Bluedroid uses permissions to secure characteristics.
  // This is the same as using the properties above.
  // NimBLE does not use permissions and will ignore these calls.
  // This can be removed if only using NimBLE (any SoC except ESP32).
  pSecureCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
  pInsecureCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);

  // Set value for secure characteristic
  pSecureCharacteristic->setValue("Secure Hello World!");

  // Set value for insecure characteristic
  // When using NimBLE you will be able to read this characteristic without entering the passkey.
  pInsecureCharacteristic->setValue("Insecure Hello World!");

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop() {
  delay(2000);
}
