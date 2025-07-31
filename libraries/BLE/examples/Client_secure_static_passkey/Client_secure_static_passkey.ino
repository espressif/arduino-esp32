/*
  Secure client with static passkey

  This example demonstrates how to create a secure BLE client that connects to
  a secure BLE server using a static passkey without prompting the user.
  The client will automatically use the same passkey (123456) as the server.

  This client is designed to work with the Server_secure_static_passkey example.

  Note that ESP32 uses Bluedroid by default and the other SoCs use NimBLE.
  Bluedroid initiates security on-connect, while NimBLE initiates security on-demand.
  This means that in NimBLE you can read the unsecure characteristic without entering
  the passkey. This is not possible in Bluedroid.

  Also, the SoC stores the authentication info in the NVS memory. After a successful
  connection it is possible that a passkey change will be ineffective.
  To avoid this, clear the memory of the SoC's between security tests.

  Based on examples from Neil Kolban and h2zero.
  Created by lucasssvaz.
*/

#include "BLEDevice.h"
#include "BLESecurity.h"

// The remote service we wish to connect to.
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
// The characteristics of the remote service we are interested in.
static BLEUUID unsecureCharUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID secureCharUUID("ff1d2614-e2d6-4c87-9154-6625d39ca7f8");

// This must match the server's passkey
#define CLIENT_PIN 123456

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic *pRemoteUnsecureCharacteristic;
static BLERemoteCharacteristic *pRemoteSecureCharacteristic;
static BLEAdvertisedDevice *myDevice;

// Callback function to handle notifications
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
  Serial.print("data: ");
  Serial.write(pData, length);
  Serial.println();
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient *pclient) {
    Serial.println("Connected to secure server");
  }

  void onDisconnect(BLEClient *pclient) {
    connected = false;
    Serial.println("Disconnected from server");
  }
};

bool connectToServer() {
  Serial.print("Forming a secure connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient *pClient = BLEDevice::createClient();
  Serial.println(" - Created client");

  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remote BLE Server.
  pClient->connect(myDevice);
  Serial.println(" - Connected to server");

  // Set MTU to maximum for better performance
  pClient->setMTU(517);

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");

  // Obtain a reference to the unsecure characteristic
  pRemoteUnsecureCharacteristic = pRemoteService->getCharacteristic(unsecureCharUUID);
  if (pRemoteUnsecureCharacteristic == nullptr) {
    Serial.print("Failed to find unsecure characteristic UUID: ");
    Serial.println(unsecureCharUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found unsecure characteristic");

  // Obtain a reference to the secure characteristic
  pRemoteSecureCharacteristic = pRemoteService->getCharacteristic(secureCharUUID);
  if (pRemoteSecureCharacteristic == nullptr) {
    Serial.print("Failed to find secure characteristic UUID: ");
    Serial.println(secureCharUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found secure characteristic");

  // Read the value of the unsecure characteristic (should work without authentication)
  if (pRemoteUnsecureCharacteristic->canRead()) {
    String value = pRemoteUnsecureCharacteristic->readValue();
    Serial.print("Unsecure characteristic value: ");
    Serial.println(value.c_str());
  }

  // For Bluedroid, we need to set the authentication request type for the secure characteristic
  // This is not needed for NimBLE and will be ignored.
  pRemoteSecureCharacteristic->setAuth(ESP_GATT_AUTH_REQ_NO_MITM);

  // Try to read the secure characteristic (this will trigger security negotiation in NimBLE)
  if (pRemoteSecureCharacteristic->canRead()) {
    Serial.println("Attempting to read secure characteristic...");
    String value = pRemoteSecureCharacteristic->readValue();
    Serial.print("Secure characteristic value: ");
    Serial.println(value.c_str());
  }

  // Register for notifications on both characteristics if they support it
  if (pRemoteUnsecureCharacteristic->canNotify()) {
    pRemoteUnsecureCharacteristic->registerForNotify(notifyCallback);
    Serial.println(" - Registered for unsecure characteristic notifications");
  }

  if (pRemoteSecureCharacteristic->canNotify()) {
    pRemoteSecureCharacteristic->registerForNotify(notifyCallback);
    Serial.println(" - Registered for secure characteristic notifications");
  }

  connected = true;
  return true;
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      Serial.println("Found our secure server!");
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Secure BLE Client application...");

  BLEDevice::init("Secure BLE Client");

  // Set up security with the same passkey as the server
  BLESecurity *pSecurity = new BLESecurity();

  // Set security parameters
  // Deafult parameters:
  // - IO capability is set to NONE
  // - Initiator and responder key distribution flags are set to both encryption and identity keys.
  // - Passkey is set to BLE_SM_DEFAULT_PASSKEY (123456). It will warn if you don't change it.
  // - Max key size is set to 16 bytes

  // Set the same static passkey as the server
  // The first argument defines if the passkey is static or random.
  // The second argument is the passkey (ignored when using a random passkey).
  pSecurity->setPassKey(true, CLIENT_PIN);

  // Set authentication mode to match server requirements
  // Enable secure connection only for this example
  pSecurity->setAuthenticationMode(false, false, true);

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device. Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

void loop() {
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect. Now we connect to it.
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the secure BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothing more we will do.");
    }
    doConnect = false;
  }

  // If we are connected to a peer BLE Server, demonstrate secure communication
  if (connected) {
    // Write to the unsecure characteristic
    String unsecureValue = "Client time: " + String(millis() / 1000);
    if (pRemoteUnsecureCharacteristic->canWrite()) {
      pRemoteUnsecureCharacteristic->writeValue(unsecureValue.c_str(), unsecureValue.length());
      Serial.println("Wrote to unsecure characteristic: " + unsecureValue);
    }

    // Write to the secure characteristic
    String secureValue = "Secure client time: " + String(millis() / 1000);
    if (pRemoteSecureCharacteristic->canWrite()) {
      pRemoteSecureCharacteristic->writeValue(secureValue.c_str(), secureValue.length());
      Serial.println("Wrote to secure characteristic: " + secureValue);
    }
  } else if (doScan) {
    // Restart scanning if we're disconnected
    BLEDevice::getScan()->start(0);
  }

  delay(2000);  // Delay 2 seconds between loops
}
