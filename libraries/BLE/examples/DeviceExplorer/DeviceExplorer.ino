/*
 * BLE Device Explorer Example -- New API
 *
 * Scans for nearby BLE devices, connects to the first one found,
 * and enumerates the entire GATT hierarchy: all services, their
 * characteristics, and each characteristic's properties.
 *
 * GATT Hierarchy:
 *   Server
 *     └── Service (identified by UUID)
 *           └── Characteristic (identified by UUID, has properties)
 *                 └── Descriptor (optional metadata)
 *
 * This is useful for exploring unknown BLE peripherals and
 * understanding what services and data they expose.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

static const uint32_t SCAN_DURATION_MS = 5000;

void printCharacteristicProperties(BLERemoteCharacteristic &chr) {
  Serial.print("      Properties: ");
  bool first = true;

  if (chr.canRead()) {
    Serial.print("READ");
    first = false;
  }
  if (chr.canWrite()) {
    if (!first) {
      Serial.print(" | ");
    }
    Serial.print("WRITE");
    first = false;
  }
  if (chr.canNotify()) {
    if (!first) {
      Serial.print(" | ");
    }
    Serial.print("NOTIFY");
    first = false;
  }
  if (chr.canIndicate()) {
    if (!first) {
      Serial.print(" | ");
    }
    Serial.print("INDICATE");
    first = false;
  }

  if (first) {
    Serial.print("(none)");
  }
  Serial.println();
}

void exploreDevice(BTAddress address) {
  Serial.printf("Creating client...\n");
  BLEClient client = BLE.createClient();
  if (!client) {
    Serial.println("Failed to create client!");
    return;
  }

  // Connect to the remote device
  Serial.printf("Connecting to %s... ", address.toString().c_str());
  BTStatus status = client.connect(address);
  if (!status) {
    Serial.printf("FAILED! (%s)\n", status.toString());
    return;
  }
  Serial.println("OK");

  // Discover all services on the remote device
  Serial.print("Discovering services... ");
  if (!client.discoverServices()) {
    Serial.println("FAILED!");
    client.disconnect();
    return;
  }
  Serial.println("OK");

  // Retrieve the full list of services
  std::vector<BLERemoteService> services = client.getServices();
  Serial.printf("\nFound %d service(s):\n", (int)services.size());
  Serial.println("=================================================");

  for (auto &svc : services) {
    Serial.printf("\n  Service: %s (handle 0x%04X)\n", svc.getUUID().toString().c_str(), svc.getHandle());
    Serial.println("  -------------------------------------------------");

    // Enumerate all characteristics within this service
    std::vector<BLERemoteCharacteristic> chars = svc.getCharacteristics();
    if (chars.empty()) {
      Serial.println("    (no characteristics)");
      continue;
    }

    for (auto &chr : chars) {
      Serial.printf("    Characteristic: %s (handle 0x%04X)\n", chr.getUUID().toString().c_str(), chr.getHandle());
      printCharacteristicProperties(chr);

      // If readable, attempt to read and print the current value
      if (chr.canRead()) {
        String value = chr.readValue();
        if (value.length() > 0) {
          Serial.printf("      Value: \"%s\"\n", value.c_str());
        } else {
          Serial.println("      Value: (empty)");
        }
      }

      Serial.println();
    }
  }

  Serial.println("=================================================");
  Serial.println("Exploration complete!");

  // Disconnect from the remote device
  Serial.print("Disconnecting... ");
  client.disconnect();
  Serial.println("OK");
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BLE Device Explorer ===");

  Serial.print("Initializing BLE... ");
  BTStatus status = BLE.begin("DeviceExplorer");
  if (!status) {
    Serial.printf("FAILED! (%s)\n", status.toString());
    return;
  }
  Serial.println("OK");

  // Configure and start a blocking scan
  BLEScan scan = BLE.getScan();
  if (!scan) {
    Serial.println("Failed to get scan object!");
    return;
  }

  scan.setActiveScan(true);
  scan.setInterval(100);
  scan.setWindow(99);
  scan.setFilterDuplicates(true);

  Serial.printf("Scanning for %d seconds...\n", (int)(SCAN_DURATION_MS / 1000));
  Serial.println();
  BLEScanResults results = scan.startBlocking(SCAN_DURATION_MS);

  int count = results.getCount();
  Serial.printf("Scan complete! Found %d device(s):\n", count);
  Serial.println("---------------------------------------------");

  for (const BLEAdvertisedDevice &dev : results) {
    Serial.printf("  [%d] %s", dev.getRSSI(), dev.getAddress().toString().c_str());
    if (dev.haveName()) {
      Serial.printf("  \"%s\"", dev.getName().c_str());
    }
    Serial.println();
  }
  Serial.println("---------------------------------------------");

  if (count == 0) {
    Serial.println("No devices found. Reset to try again.");
    return;
  }

  // Auto-connect to the first discovered device
  const BLEAdvertisedDevice &target = *results.begin();
  Serial.printf("\nExploring first device: %s\n\n", target.getAddress().toString().c_str());
  exploreDevice(target.getAddress());
}

void loop() {
  delay(10000);
}
