/**
 * A BLE client example that connects to multiple BLE servers simultaneously.
 *
 * This example demonstrates how to:
 * - Scan for multiple BLE servers
 * - Connect to multiple servers at the same time
 * - Interact with characteristics on different servers
 * - Handle disconnections and reconnections
 *
 * The example looks for servers advertising the service UUID: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
 * and connects to up to MAX_SERVERS servers.
 *
 * Created by lucasssvaz
 * Based on the original Client example by Neil Kolban and chegewara
 */

#include "BLEDevice.h"

// The remote service we wish to connect to.
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
// The characteristic of the remote service we are interested in.
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

// Maximum number of servers to connect to
#define MAX_SERVERS 3

// Structure to hold information about each connected server
struct ServerConnection {
  BLEClient *pClient;
  BLEAdvertisedDevice *pDevice;
  BLERemoteCharacteristic *pRemoteCharacteristic;
  bool connected;
  bool doConnect;
  String name;
};

// Array to manage multiple server connections
ServerConnection servers[MAX_SERVERS];
int connectedServers = 0;
static bool doScan = true;

// Callback function to handle notifications from any server
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  // Find which server this notification came from
  for (int i = 0; i < MAX_SERVERS; i++) {
    if (servers[i].connected && servers[i].pRemoteCharacteristic == pBLERemoteCharacteristic) {
      Serial.print("Notify from server ");
      Serial.print(servers[i].name);
      Serial.print(" - Characteristic: ");
      Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
      Serial.print(" | Length: ");
      Serial.print(length);
      Serial.print(" | Data: ");
      Serial.write(pData, length);
      Serial.println();
      break;
    }
  }
}

// Client callback class to handle connect/disconnect events
class MyClientCallback : public BLEClientCallbacks {
  int serverIndex;

public:
  MyClientCallback(int index) : serverIndex(index) {}

  void onConnect(BLEClient *pclient) {
    Serial.print("Connected to server ");
    Serial.println(servers[serverIndex].name);
  }

  void onDisconnect(BLEClient *pclient) {
    servers[serverIndex].connected = false;
    connectedServers--;
    Serial.print("Disconnected from server ");
    Serial.print(servers[serverIndex].name);
    Serial.print(" | Total connected: ");
    Serial.println(connectedServers);
    doScan = true;  // Resume scanning to find replacement servers
  }
};

// Function to connect to a specific server
bool connectToServer(int serverIndex) {
  Serial.print("Connecting to server ");
  Serial.print(serverIndex);
  Serial.print(" at address: ");
  Serial.println(servers[serverIndex].pDevice->getAddress().toString().c_str());

  servers[serverIndex].pClient = BLEDevice::createClient();
  Serial.println(" - Created client");

  // Set the callback for this specific server connection
  servers[serverIndex].pClient->setClientCallbacks(new MyClientCallback(serverIndex));

  // Connect to the remote BLE Server
  servers[serverIndex].pClient->connect(servers[serverIndex].pDevice);
  Serial.println(" - Connected to server");
  servers[serverIndex].pClient->setMTU(517);  // Request maximum MTU from server

  // Obtain a reference to the service we are after in the remote BLE server
  BLERemoteService *pRemoteService = servers[serverIndex].pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    servers[serverIndex].pClient->disconnect();
    return false;
  }
  Serial.println(" - Found service");

  // Obtain a reference to the characteristic in the service
  servers[serverIndex].pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (servers[serverIndex].pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    servers[serverIndex].pClient->disconnect();
    return false;
  }
  Serial.println(" - Found characteristic");

  // Read the value of the characteristic
  if (servers[serverIndex].pRemoteCharacteristic->canRead()) {
    String value = servers[serverIndex].pRemoteCharacteristic->readValue();
    Serial.print("Initial characteristic value: ");
    Serial.println(value.c_str());
  }

  // Register for notifications if available
  if (servers[serverIndex].pRemoteCharacteristic->canNotify()) {
    servers[serverIndex].pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println(" - Registered for notifications");
  }

  servers[serverIndex].connected = true;
  connectedServers++;
  Serial.print("Successfully connected! Total servers connected: ");
  Serial.println(connectedServers);
  return true;
}

// Scan callback class to find BLE servers
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // Check if this device has the service we're looking for
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      Serial.println(" -> This device has our service!");

      // Check if we already know about this device
      String deviceAddress = advertisedDevice.getAddress().toString().c_str();
      bool alreadyKnown = false;

      for (int i = 0; i < MAX_SERVERS; i++) {
        if (servers[i].pDevice != nullptr) {
          if (servers[i].pDevice->getAddress().toString() == deviceAddress) {
            alreadyKnown = true;
            break;
          }
        }
      }

      if (alreadyKnown) {
        Serial.println(" -> Already connected or connecting to this device");
        return;
      }

      // Find an empty slot for this server
      for (int i = 0; i < MAX_SERVERS; i++) {
        if (servers[i].pDevice == nullptr || (!servers[i].connected && !servers[i].doConnect)) {
          servers[i].pDevice = new BLEAdvertisedDevice(advertisedDevice);
          servers[i].doConnect = true;
          servers[i].name = "Server_" + String(i);
          Serial.print(" -> Assigned to slot ");
          Serial.println(i);

          // If we've found enough servers, stop scanning
          int pendingConnections = 0;
          for (int j = 0; j < MAX_SERVERS; j++) {
            if (servers[j].connected || servers[j].doConnect) {
              pendingConnections++;
            }
          }
          if (pendingConnections >= MAX_SERVERS) {
            Serial.println("Found enough servers, stopping scan");
            BLEDevice::getScan()->stop();
            doScan = false;
          }
          break;
        }
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("=================================");
  Serial.println("BLE Multi-Client Example");
  Serial.println("=================================");
  Serial.print("Max servers to connect: ");
  Serial.println(MAX_SERVERS);
  Serial.println();

  // Initialize all server connections
  for (int i = 0; i < MAX_SERVERS; i++) {
    servers[i].pClient = nullptr;
    servers[i].pDevice = nullptr;
    servers[i].pRemoteCharacteristic = nullptr;
    servers[i].connected = false;
    servers[i].doConnect = false;
    servers[i].name = "";
  }

  // Initialize BLE
  BLEDevice::init("ESP32_MultiClient");

  // Set up BLE scanner
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  Serial.println("Scanning for BLE servers...");
}

void loop() {
  // Process any pending connections
  for (int i = 0; i < MAX_SERVERS; i++) {
    if (servers[i].doConnect) {
      if (connectToServer(i)) {
        Serial.println("Connection successful");
      } else {
        Serial.println("Connection failed");
        // Clear this slot so we can try another server
        delete servers[i].pDevice;
        servers[i].pDevice = nullptr;
      }
      servers[i].doConnect = false;
    }
  }

  // If we're connected to servers, send data to each one
  if (connectedServers > 0) {
    for (int i = 0; i < MAX_SERVERS; i++) {
      if (servers[i].connected && servers[i].pRemoteCharacteristic != nullptr) {
        // Create a unique message for each server
        String newValue = servers[i].name + " | Time: " + String(millis() / 1000);

        Serial.print("Sending to ");
        Serial.print(servers[i].name);
        Serial.print(": ");
        Serial.println(newValue);

        // Write the value to the characteristic
        servers[i].pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
      }
    }
  } else {
    Serial.println("No servers connected");
  }

  // Resume scanning if we have room for more connections
  if (doScan && connectedServers < MAX_SERVERS) {
    Serial.println("Resuming scan for more servers...");
    BLEDevice::getScan()->start(5, false);
    doScan = false;
    delay(5000);  // Wait for scan to complete
  }

  delay(2000);  // Delay between loop iterations
}
