/*
 * BLE L2CAP CoC Client Example -- New API
 *
 * Demonstrates an L2CAP Connection-oriented Channel (CoC) client. It scans for
 * the L2CAP_Server example, opens a normal ACL connection, then opens an L2CAP
 * CoC channel to the server's PSM and exchanges data over it -- a
 * higher-throughput path than GATT for bulk transfers.
 *
 * It sends an incrementing message every couple of seconds and prints the
 * server's echo.
 *
 * Pair with the L2CAP_Server example on another ESP32.
 *
 * NOTE: L2CAP CoC is NimBLE-only and requires CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM
 * to be non-zero in the build.
 *
 * Callback style: named functions (+ one lambda).
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_L2CAP_SUPPORTED

// PSM and MTU must match the server.
static const uint16_t L2CAP_PSM = 0x0080;
static const uint16_t L2CAP_MTU = 256;
static const char *TARGET_NAME = "L2CAP CoC Server";

BLEClient client;
BLEL2CAPChannel channel;
BTAddress serverAddress;
volatile bool doConnect = false;
bool channelReady = false;

void onChannelData(const BLEL2CAPChannel &ch, const uint8_t *data, size_t len) {
  (void)ch;
  Serial.printf("Server echoed %u bytes: %.*s\n", (unsigned)len, (int)len, data);
}

// Keep the scan callback lightweight: record the address and defer the
// (blocking) connect to loop().
void onDeviceFound(BLEAdvertisedDevice device) {
  if (device.getName() != TARGET_NAME) {
    return;
  }
  Serial.printf("Found \"%s\" at %s\n", TARGET_NAME, device.getAddress().toString().c_str());
  serverAddress = device.getAddress();
  doConnect = true;
  BLE.getScan().stop();
}

bool openChannel() {
  Serial.print("ACL connecting... ");
  client = BLE.createClient();
  BTStatus st = client.connect(serverAddress);
  if (!st) {
    Serial.printf("FAILED! (%s)\n", st.toString());
    return false;
  }
  Serial.printf("OK (handle %u)\n", client.getHandle());

  Serial.printf("Opening L2CAP channel to PSM 0x%04X... ", L2CAP_PSM);
  channel = BLE.connectL2CAP(client.getHandle(), L2CAP_PSM, L2CAP_MTU);
  if (!channel) {
    Serial.println("FAILED to start!");
    client.disconnect();
    return false;
  }
  channel.onData(onChannelData);
  channel.onDisconnect([](const BLEL2CAPChannel &ch) {
    (void)ch;
    Serial.println("L2CAP channel closed");
    channelReady = false;
  });

  // The CoC connection completes asynchronously; wait for it to come up.
  unsigned long deadline = millis() + 10000;
  while (!channel.isConnected() && millis() < deadline) {
    delay(20);
  }
  if (!channel.isConnected()) {
    Serial.println("L2CAP channel did not connect in time");
    client.disconnect();
    return false;
  }
  Serial.printf("L2CAP channel connected (MTU %u)\n", channel.getMTU());
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BLE L2CAP CoC Client ===");

  BTStatus st = BLE.begin("L2CAP CoC Client");
  if (!st) {
    Serial.printf("BLE.begin failed: %s\n", st.toString());
    while (true) {
      delay(1000);
    }
  }

  BLEScan scan = BLE.getScan();
  scan.setActiveScan(true);
  scan.onResult(onDeviceFound);
  scan.start(0);
  Serial.printf("Scanning for \"%s\"...\n", TARGET_NAME);
}

void loop() {
  if (doConnect) {
    doConnect = false;
    if (openChannel()) {
      channelReady = true;
    } else {
      Serial.println("Retrying scan...");
      BLE.getScan().start(0);
    }
  }

  if (channelReady && channel.isConnected()) {
    static uint32_t counter = 0;
    char msg[32];
    int n = snprintf(msg, sizeof(msg), "PING #%lu", (unsigned long)counter++);
    BTStatus s = channel.write((const uint8_t *)msg, (size_t)n);
    if (s) {
      Serial.printf("Sent: %s\n", msg);
    } else {
      Serial.printf("Send failed: %s\n", s.toString());
    }
  }

  if (channelReady && client && !client.isConnected()) {
    Serial.println("ACL lost, rescanning...");
    channelReady = false;
    BLE.getScan().start(0);
  }

  delay(2000);
}

#else  // !BLE_L2CAP_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("L2CAP_Client requires NimBLE with CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM > 0 in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_L2CAP_SUPPORTED */
