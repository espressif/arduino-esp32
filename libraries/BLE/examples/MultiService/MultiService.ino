/*
 * BLE Multi-Service Example -- New API
 *
 * Creates a GATT server with two services, each containing multiple
 * characteristics, to demonstrate a realistic multi-service setup:
 *
 *   Environment Service
 *     - Temperature (read + notify)  -- simulated sensor data
 *     - Humidity    (read + notify)  -- simulated sensor data
 *
 *   Device Info Service
 *     - Firmware Version (read-only)
 *     - Device Name      (read + write) -- bidirectional communication
 *
 * Test with a BLE scanner app (e.g. nRF Connect):
 * 1. Scan for "MultiService"
 * 2. Connect and explore both services
 * 3. Enable notifications on Temperature and/or Humidity
 * 4. Write a new device name to the Device Name characteristic
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

// ---------------------------------------------------------------------------
// Custom UUIDs generated for this example
// (use https://www.uuidgenerator.net/ to create your own)
// ---------------------------------------------------------------------------

// Environment Service and its characteristics
static const char *ENV_SVC_UUID = "a1b2c3d4-0001-4e8f-a5b6-000000000001";
static const char *TEMP_CHR_UUID = "a1b2c3d4-0001-4e8f-a5b6-000000000002";
static const char *HUM_CHR_UUID = "a1b2c3d4-0001-4e8f-a5b6-000000000003";

// Device Info Service and its characteristics
static const char *DEV_SVC_UUID = "a1b2c3d4-0002-4e8f-a5b6-000000000001";
static const char *FW_CHR_UUID = "a1b2c3d4-0002-4e8f-a5b6-000000000002";
static const char *NAME_CHR_UUID = "a1b2c3d4-0002-4e8f-a5b6-000000000003";

static const char *FIRMWARE_VERSION = "1.0.0";

// Global characteristic handles needed in loop() for notifications
BLECharacteristic tempChr;
BLECharacteristic humChr;

// Track connection state for log gating
bool clientConnected = false;

// ---------------------------------------------------------------------------
// Connection callbacks (named free functions, as recommended by the API style)
// ---------------------------------------------------------------------------

void onClientConnect(BLEServer server, const BLEConnInfo &conn) {
  clientConnected = true;
  Serial.printf("Client connected: %s (MTU %d)\n", conn.getAddress().toString().c_str(), conn.getMTU());
}

void onClientDisconnect(BLEServer server, const BLEConnInfo &conn, uint8_t reason) {
  clientConnected = false;
  Serial.printf("Client disconnected (reason 0x%02X), restarting advertising...\n", reason);
  server.startAdvertising();
  Serial.println("Advertising restarted");
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BLE Multi-Service Example ===");

  // --- Initialize the BLE stack ---
  Serial.print("Initializing BLE... ");
  BTStatus initStatus = BLE.begin("MultiService");
  if (!initStatus) {
    Serial.printf("FAILED! (%s)\n", initStatus.toString());
    return;
  }
  Serial.println("OK");

  // --- Create the GATT server ---
  Serial.print("Creating server... ");
  BLEServer server = BLE.createServer();
  if (!server) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");

  server.onConnect(onClientConnect);
  server.onDisconnect(onClientDisconnect);

  // =====================================================================
  // Service 1 -- Environment
  // =====================================================================

  Serial.print("Creating Environment service... ");
  BLEService envSvc = server.createService(ENV_SVC_UUID);
  if (!envSvc) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");

  // Temperature: clients can read the current value or subscribe to notifications
  Serial.print("  Creating Temperature characteristic... ");
  tempChr = envSvc.createCharacteristic(TEMP_CHR_UUID, BLEProperty::Read | BLEProperty::Notify, BLEPermissions::OpenRead);
  if (!tempChr) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");
  tempChr.setValue("0.00");
  tempChr.setDescription("Temperature (C)");

  // Humidity: same properties as temperature
  Serial.print("  Creating Humidity characteristic... ");
  humChr = envSvc.createCharacteristic(HUM_CHR_UUID, BLEProperty::Read | BLEProperty::Notify, BLEPermissions::OpenRead);
  if (!humChr) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");
  humChr.setValue("0.00");
  humChr.setDescription("Humidity (%)");

  // =====================================================================
  // Service 2 -- Device Info
  // =====================================================================

  Serial.print("Creating Device Info service... ");
  BLEService devSvc = server.createService(DEV_SVC_UUID);
  if (!devSvc) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");

  // Firmware Version: read-only, never changes at runtime
  Serial.print("  Creating Firmware Version characteristic... ");
  BLECharacteristic fwChr = devSvc.createCharacteristic(FW_CHR_UUID, BLEProperty::Read, BLEPermissions::OpenRead);
  if (!fwChr) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");
  fwChr.setValue(FIRMWARE_VERSION);
  fwChr.setDescription("Firmware Version");

  // Device Name: read + write so clients can rename the device at runtime
  Serial.print("  Creating Device Name characteristic... ");
  BLECharacteristic nameChr = devSvc.createCharacteristic(NAME_CHR_UUID, BLEProperty::Read | BLEProperty::Write, BLEPermissions::OpenReadWrite);
  if (!nameChr) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");
  nameChr.setValue("MultiService");
  nameChr.setDescription("Device Name");

  // Write callback -- demonstrates bidirectional communication
  nameChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    String newName = c.getStringValue();
    Serial.printf("Device name changed by %s: \"%s\"\n", conn.getAddress().toString().c_str(), newName.c_str());
  });

  // --- Start the GATT server ---
  Serial.print("Starting server... ");
  BTStatus status = server.start();
  if (!status) {
    Serial.printf("FAILED! (%s)\n", status.toString());
    return;
  }
  Serial.println("OK");

  // --- Configure and start advertising ---
  Serial.print("Configuring advertising... ");
  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(ENV_SVC_UUID);
  adv.addServiceUUID(DEV_SVC_UUID);
  Serial.println("OK");

  Serial.print("Starting advertising... ");
  status = adv.start();
  if (!status) {
    Serial.printf("FAILED! (%s)\n", status.toString());
    return;
  }
  Serial.println("OK");

  Serial.println();
  Serial.println("Server is ready! Waiting for connections...");
  Serial.printf("Device name: %s\n", BLE.getDeviceName().c_str());
  Serial.println("Notifications will be sent every 2 seconds once subscribed.");
  Serial.println();
}

// ---------------------------------------------------------------------------
// loop() -- simulate sensor readings and notify subscribed clients
// ---------------------------------------------------------------------------

void loop() {
  // Simulate slowly-changing sensor data
  float temperature = 20.0f + 5.0f * sin(millis() / 10000.0f);
  float humidity = 50.0f + 15.0f * cos(millis() / 12000.0f);

  // Notify Temperature subscribers
  if (tempChr && tempChr.getSubscribedCount() > 0) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%.2f", temperature);
    tempChr.setValue(buf);
    tempChr.notify();
    Serial.printf("[%lus] Temperature: %s C  (subscribers: %d)\n", millis() / 1000, buf, tempChr.getSubscribedCount());
  }

  // Notify Humidity subscribers
  if (humChr && humChr.getSubscribedCount() > 0) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%.2f", humidity);
    humChr.setValue(buf);
    humChr.notify();
    Serial.printf("[%lus] Humidity: %s %%  (subscribers: %d)\n", millis() / 1000, buf, humChr.getSubscribedCount());
  }

  // Periodic hint when connected but no subscriptions active
  if (clientConnected && tempChr.getSubscribedCount() == 0 && humChr.getSubscribedCount() == 0) {
    static unsigned long lastHint = 0;
    if (millis() - lastHint > 10000) {
      Serial.println("Client connected but not subscribed to notifications yet.");
      lastHint = millis();
    }
  }

  delay(2000);
}
