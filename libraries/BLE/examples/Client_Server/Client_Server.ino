/*
 * BLE Client and Server Coexistence Example
 *
 * This example demonstrates how to run both BLE client and server
 * functionality on the same ESP32 device simultaneously.
 *
 * The device will:
 * - Act as a BLE server, advertising a service with a characteristic
 * - Act as a BLE client, scanning for other BLE servers
 * - Connect to found servers and interact with their services
 * - Handle both incoming and outgoing connections
 *
 * You can test this example by uploading it to two ESP32 boards.
 *
 * Author: lucasssvaz
 * Based on Arduino BLE examples
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEScan.h>

// Server-side definitions
#define SERVER_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SERVER_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Client-side definitions (looking for the same service)
static BLEUUID clientServiceUUID(SERVER_SERVICE_UUID);
static BLEUUID clientCharUUID(SERVER_CHARACTERISTIC_UUID);

// Server objects
BLEServer *pServer = nullptr;
BLECharacteristic *pServerCharacteristic = nullptr;

// Client objects
static boolean doConnect = false;
static boolean clientConnected = false;
static BLERemoteCharacteristic *pRemoteCharacteristic;
static BLEAdvertisedDevice *targetDevice;
static BLEClient *pClient = nullptr;
BLEScan *pBLEScan = nullptr;

// Server callbacks
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    Serial.println("Server: Client connected");
  }

  void onDisconnect(BLEServer *pServer) {
    Serial.println("Server: Client disconnected");
    // Restart advertising
    BLEDevice::startAdvertising();
  }
};

// Characteristic callbacks for server
class CharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();
    Serial.print("Server: Characteristic written, value: ");
    Serial.println(value.c_str());
  }

  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("Server: Characteristic read");
  }
};

// Client callbacks
class ClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient *pClient) {
    Serial.println("Client: Connected to server");
    clientConnected = true;
  }

  void onDisconnect(BLEClient *pClient) {
    Serial.println("Client: Disconnected from server");
    clientConnected = false;
  }
};

// Client notification callback
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  Serial.print("Client: Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
  Serial.print("Client: Data: ");
  Serial.write(pData, length);
  Serial.println();
}

// Scan callbacks
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("Client: Found device: ");
    Serial.println(advertisedDevice.toString().c_str());

    // Check if this device has our target service
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(clientServiceUUID) && !clientConnected) {

      Serial.println("Client: Found target service, attempting connection...");
      BLEDevice::getScan()->stop();
      targetDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
    }
  }
};

bool connectToServer() {
  Serial.print("Client: Forming connection to ");
  Serial.println(targetDevice->getAddress().toString().c_str());

  // Create client if it doesn't exist, otherwise reuse existing one
  if (pClient == nullptr) {
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks());
    Serial.println("Client: Created new client");
  } else {
    Serial.println("Client: Reusing existing client");
  }

  if (!pClient->connect(targetDevice)) {
    Serial.println("Client: Failed to connect");
    return false;
  }

  Serial.println("Client: Connected to server");
  pClient->setMTU(517);  // Request maximum MTU

  // Get the service
  BLERemoteService *pRemoteService = pClient->getService(clientServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Client: Failed to find service UUID: ");
    Serial.println(clientServiceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println("Client: Found service");

  // Get the characteristic
  pRemoteCharacteristic = pRemoteService->getCharacteristic(clientCharUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Client: Failed to find characteristic UUID: ");
    Serial.println(clientCharUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println("Client: Found characteristic");

  // Read the initial value
  if (pRemoteCharacteristic->canRead()) {
    String value = pRemoteCharacteristic->readValue();
    Serial.print("Client: Initial characteristic value: ");
    Serial.println(value.c_str());
  }

  // Register for notifications if available
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Client: Registered for notifications");
  }

  return true;
}

void setupServer() {
  Serial.println("Setting up BLE Server...");

  // Create server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Create service
  BLEService *pService = pServer->createService(SERVER_SERVICE_UUID);

  // Create characteristic
  pServerCharacteristic = pService->createCharacteristic(
    SERVER_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );

  pServerCharacteristic->setCallbacks(new CharacteristicCallbacks());
  pServerCharacteristic->setValue("Hello from Coexistence Server");

  // Start service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVER_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("Server: Advertising started");
}

void setupClient() {
  Serial.println("Setting up BLE Client...");

  // Create scanner
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  Serial.println("Client: Scanner configured");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE Client-Server Coexistence Example...");

  // Initialize BLE device with a name
  BLEDevice::init("ESP32-Coexistence");

  // Setup both server and client
  setupServer();
  setupClient();

  // Start initial scan
  pBLEScan->start(10, false);  // Scan for 10 seconds, don't repeat

  Serial.println("Setup complete. Device is advertising as server and scanning as client.");
}

void loop() {
  static unsigned long lastServerUpdate = 0;
  static unsigned long lastClientWrite = 0;
  static unsigned long lastScanStart = 0;
  unsigned long currentTime = millis();

  // Handle client connection attempts
  if (doConnect && !clientConnected) {
    if (connectToServer()) {
      Serial.println("Client: Successfully connected to remote server");
    } else {
      Serial.println("Client: Failed to connect, will retry scanning");
      // Restart scanning after failed connection
      pBLEScan->start(10, false);
    }
    doConnect = false;
  }

  // Update server characteristic periodically
  if (currentTime - lastServerUpdate > 5000) {  // Every 5 seconds
    String value = "Server time: " + String(millis() / 1000);
    pServerCharacteristic->setValue(value.c_str());
    pServerCharacteristic->notify();  // Notify connected clients
    Serial.print("Server: Updated characteristic to: ");
    Serial.println(value);

    lastServerUpdate = currentTime;
  }

  // Write to remote characteristic if connected as client
  if (clientConnected && pRemoteCharacteristic && currentTime - lastClientWrite > 3000) {
    if (pRemoteCharacteristic->canWrite()) {
      String clientValue = "Client msg: " + String(millis() / 1000);
      pRemoteCharacteristic->writeValue(clientValue.c_str(), clientValue.length());
      Serial.print("Client: Wrote to remote characteristic: ");
      Serial.println(clientValue);
      lastClientWrite = currentTime;
    }
  }

  // Restart scanning periodically if not connected
  if (!clientConnected && currentTime - lastScanStart > 15000) {  // Every 15 seconds
    Serial.println("Client: Restarting scan...");
    pBLEScan->start(10, false);
    lastScanStart = currentTime;
  }

  delay(100);
}
