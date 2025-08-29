/*
  BLE Server Authorization Test

  This example demonstrates how to use authorization in both NimBLE and Bluedroid.
  The server creates a service with two characteristics:
  - One characteristic that requires read/write authorization
  - One characteristic that does not require authorization

  When authorization is required, the server will call the onAuthorizationRequest callback.
  This example shows a simple authorization logic that accepts requests from connection handle 0
  and rejects requests from all other connection handles.

  In a real application, you would implement more sophisticated authorization logic
  based on your security requirements.

  This example works with both NimBLE and Bluedroid implementations.

  Created by lucasssvaz.
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <string>

#define SERVICE_UUID                    "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define OPEN_CHARACTERISTIC_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define AUTHORIZED_CHARACTERISTIC_UUID  "ff1d2614-e2d6-4c87-9154-6625d39ca7f8"

class MySecurityCallbacks : public BLESecurityCallbacks {
public:
  bool onAuthorizationRequest(uint16_t connHandle, uint16_t attrHandle, bool isRead) override {
    Serial.printf("Authorization request: conn_handle=%d, attr_handle=%d, is_read=%s\n",
                  connHandle, attrHandle, isRead ? "true" : "false");

    // Simple authorization logic: allow only connection handle 0
    // In a real application, you would implement more sophisticated logic
    bool authorized = (connHandle == 0);

    Serial.printf("Authorization %s for connection handle %d\n",
                  authorized ? "granted" : "denied", connHandle);

    return authorized;
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    Serial.printf("Characteristic written: UUID=%s, value=%s\n",
                  pCharacteristic->getUUID().toString().c_str(), value.c_str());
  }

  void onRead(BLECharacteristic* pCharacteristic) override {
    Serial.printf("Characteristic read: UUID=%s\n",
                  pCharacteristic->getUUID().toString().c_str());
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE Authorization Test Server!");

  Serial.printf("Using BLE stack: %s\n", BLEDevice::getBLEStackString());

  BLEDevice::init("Authorization Test Server");

  // Set security callbacks
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());

  // Create the BLE Server
  BLEServer* pServer = BLEDevice::createServer();

  // Create the BLE Service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Create open characteristic (no authorization required)
  BLECharacteristic* pOpenCharacteristic = pService->createCharacteristic(
    OPEN_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  pOpenCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pOpenCharacteristic->setValue("Open characteristic - no authorization required");

  // Create authorized characteristic (authorization required for read/write)
  BLECharacteristic* pAuthorizedCharacteristic = pService->createCharacteristic(
    AUTHORIZED_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  pAuthorizedCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pAuthorizedCharacteristic->setValue("Authorized characteristic - authorization required");

  // Set authorization permissions for the authorized characteristic
#if defined(CONFIG_BLUEDROID_ENABLED)
  pAuthorizedCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_AUTHORIZATION | ESP_GATT_PERM_WRITE_AUTHORIZATION);
  Serial.println("Bluedroid: Set authorization permissions using setAccessPermissions()");
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  // In NimBLE, authorization is controlled by the characteristic properties
  // The PROPERTY_READ_AUTHOR and PROPERTY_WRITE_AUTHOR flags trigger authorization
  pAuthorizedCharacteristic = pService->createCharacteristic(
    AUTHORIZED_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_READ_AUTHOR | BLECharacteristic::PROPERTY_WRITE_AUTHOR
  );
  pAuthorizedCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pAuthorizedCharacteristic->setValue("Authorized characteristic - authorization required");
  Serial.println("NimBLE: Set authorization permissions using PROPERTY_READ_AUTHOR and PROPERTY_WRITE_AUTHOR");
#endif

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0); // Set value to 0x00 to not advertise this parameter

  BLEDevice::startAdvertising();
  Serial.println("Waiting for client connection to test authorization...");
  Serial.println();
  Serial.println("Connect with a BLE client and try to:");
  Serial.println("1. Read/write the open characteristic (should work without authorization)");
  Serial.println("2. Read/write the authorized characteristic (will trigger authorization callback)");
  Serial.println();
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(2000);
}
