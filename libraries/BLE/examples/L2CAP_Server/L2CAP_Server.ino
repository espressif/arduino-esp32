/*
 * BLE L2CAP CoC Server Example -- New API
 *
 * Demonstrates an L2CAP Connection-oriented Channel (CoC) server. L2CAP CoC is
 * a higher-throughput alternative to GATT for bulk data: once a client opens a
 * channel to this server's PSM, bytes flow directly over L2CAP, bypassing the
 * attribute protocol.
 *
 * A CoC channel rides on top of a normal ACL connection, so this sketch also
 * runs a minimal GATT server and advertises -- the client ACL-connects first,
 * then opens the L2CAP channel. This example simply echoes back whatever it
 * receives.
 *
 * Pair with the L2CAP_Client example on another ESP32.
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

// PSM and MTU must match the client. 0x0080 is in the dynamic PSM range.
static const uint16_t L2CAP_PSM = 0x0080;
static const uint16_t L2CAP_MTU = 256;

// A tiny GATT service so the client has something to advertise/connect to.
static const BLEUUID SVC_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");

BLEL2CAPServer l2capServer;

void onChannelAccept(const BLEL2CAPChannel &channel) {
  Serial.printf("L2CAP channel accepted (PSM 0x%04X, MTU %u)\n", channel.getPSM(), channel.getMTU());
}

void onChannelData(const BLEL2CAPChannel &channel, const uint8_t *data, size_t len) {
  Serial.printf("L2CAP received %u bytes: %.*s\n", (unsigned)len, (int)len, data);

  // Echo the payload back to the client (write() splits to the peer MTU).
  BLEL2CAPChannel reply = channel;
  BTStatus s = reply.write(data, len);
  if (!s) {
    Serial.printf("Echo write failed: %s\n", s.toString());
  }
}

void onClientDisconnect(BLEServer server, const BLEConnInfo &conn, uint8_t reason) {
  Serial.printf("ACL client disconnected (reason 0x%02X), restarting advertising...\n", reason);
  server.startAdvertising();
}

void haltWith(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BLE L2CAP CoC Server ===");

  BTStatus st = BLE.begin("L2CAP CoC Server");
  if (!st) {
    haltWith("BLE.begin", st);
  }

  // Minimal GATT server so the client can ACL-connect before opening the CoC.
  BLEServer server = BLE.createServer();
  if (!server) {
    haltWith("createServer", BTStatus::Fail);
  }
  server.onDisconnect(onClientDisconnect);
  BLEService svc = server.createService(SVC_UUID);
  if (!svc) {
    haltWith("createService", BTStatus::Fail);
  }
  st = server.start();
  if (!st) {
    haltWith("server.start", st);
  }

  // Open the L2CAP listener and wire up its callbacks before advertising.
  l2capServer = BLE.createL2CAPServer(L2CAP_PSM, L2CAP_MTU);
  if (!l2capServer) {
    haltWith("createL2CAPServer", BTStatus::Fail);
  }
  l2capServer.onAccept(onChannelAccept);
  l2capServer.onData(onChannelData);
  l2capServer.onDisconnect([](const BLEL2CAPChannel &channel) {
    Serial.printf("L2CAP channel closed (PSM 0x%04X)\n", channel.getPSM());
  });

  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(SVC_UUID);
  adv.setName("L2CAP CoC Server");
  st = adv.start();
  if (!st) {
    haltWith("advertising", st);
  }

  Serial.printf("Ready. Advertising; L2CAP listening on PSM 0x%04X (MTU %u).\n", L2CAP_PSM, L2CAP_MTU);
  Serial.println("Connect the L2CAP_Client example to echo data.");
}

void loop() {
  delay(1000);
}

#else  // !BLE_L2CAP_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("L2CAP_Server requires NimBLE with CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM > 0 in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_L2CAP_SUPPORTED */
