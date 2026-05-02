#include <Arduino.h>
#include <BLE.h>

static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

BTAddress targetAddr;
bool found = false;

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Secure Client Example");

  BLE.begin("ESP32-SecureClient");

  BLESecurity sec = BLE.getSecurity();
  sec.setIOCapability(BLESecurity::DisplayYesNo);
  sec.setAuthenticationMode(true, true, true);
  sec.onConfirmPassKey([](const BLEConnInfo &conn, uint32_t passkey) -> bool {
    Serial.printf("Confirm passkey: %06lu (y/n)? Auto-accepting.\n", (unsigned long)passkey);
    return true;
  });
  sec.onAuthenticationComplete([](const BLEConnInfo &conn, bool success) {
    Serial.printf("Authentication %s\n", success ? "complete" : "failed");
  });

  BLEScan scan = BLE.getScan();
  scan.onResult([](const BLEAdvertisedDevice &dev) {
    Serial.printf("Found: %s (%s)\n", dev.getName().c_str(), dev.getAddress().toString().c_str());
    if (dev.isAdvertisingService(serviceUUID)) {
      Serial.println("Found target server!");
      targetAddr = dev.getAddress();
      found = true;
      BLE.getScan().stop();
    }
  });

  Serial.println("Scanning...");
  scan.startBlocking(15000);

  if (!found) {
    Serial.println("Secure server not found. Run the ServerSecure example on another device.");
    return;
  }

  BLEClient client = BLE.createClient();
  BTStatus status = client.connect(targetAddr);
  if (!status) {
    Serial.printf("Connect failed: %s\n", status.toString());
    return;
  }
  Serial.println("Connected!");

  BLERemoteService svc = client.getService(serviceUUID);
  if (!svc) {
    Serial.println("Service not found");
    return;
  }

  BLERemoteCharacteristic chr = svc.getCharacteristic(charUUID);
  if (!chr) {
    Serial.println("Characteristic not found");
    return;
  }

  String val = chr.readValue();
  Serial.printf("Read secure value: %s\n", val.c_str());

  delay(5000);
  client.disconnect();
  Serial.println("Disconnected");
}

void loop() {
  delay(1000);
}
