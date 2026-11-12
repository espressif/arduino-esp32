/*
 * BLE Multi-Client UART Example -- BLEStream API
 *
 * Accepts multiple NUS centrals simultaneously. Demonstrates peerCount(),
 * peers(), attributed onPeerData(), per-peer writeTo() echo, and broadcast write().
 *
 * Connect two phones with nRF Connect or a BLE serial terminal app.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

BLEStream bleSerial;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BLE Multi-Client UART (BLEStream) ===");

  bleSerial.onConnect([](const BLEConnInfo &connInfo) {
    Serial.printf("Peer connected: %s (peers=%u)\n", connInfo.getAddress().toString().c_str(), bleSerial.peerCount());
  });

  bleSerial.onDisconnect([](const BLEConnInfo &connInfo, uint8_t reason) {
    Serial.printf("Peer disconnected: %s reason=0x%02X (peers=%u)\n", connInfo.getAddress().toString().c_str(), reason, bleSerial.peerCount());
  });

  bleSerial.onPeerData([](const BLEConnInfo &peer, const uint8_t *data, size_t len) {
    Serial.printf("RX from %s (%u bytes): ", peer.getAddress().toString().c_str(), (unsigned)len);
    Serial.write(data, len);
    Serial.println();

    // Echo back only to the sender.
    bleSerial.writeTo(peer, data, len);
  });

  Serial.print("Starting BLEStream... ");
  BTStatus status = bleSerial.begin("Multi UART");
  if (!status) {
    Serial.printf("FAILED! (%s)\n", status.toString());
    while (true) {
      delay(1000);
    }
  }
  Serial.println("OK");

  Serial.println();
  Serial.println("Multi-client NUS server ready.");
  Serial.printf("Device: %s\n", BLE.getDeviceName().c_str());
  Serial.println("Connect multiple centrals; each line is echoed to its sender only.");
  Serial.println("A heartbeat is broadcast to all peers every 10 seconds.");
  Serial.println();
}

void loop() {
  static uint32_t lastHeartbeat = 0;

  if (millis() - lastHeartbeat >= 10000) {
    lastHeartbeat = millis();
    if (bleSerial.peerCount() > 0) {
      String msg = "heartbeat peers=" + String(bleSerial.peerCount()) + "\n";
      bleSerial.write(reinterpret_cast<const uint8_t *>(msg.c_str()), msg.length());
      Serial.printf("Broadcast heartbeat (%u peer(s))\n", (unsigned)bleSerial.peerCount());
    }
  }

  // USB Serial -> broadcast to all connected centrals.
  if (Serial.available()) {
    uint8_t buffer[128];
    int bytesToRead = Serial.available();
    if (bytesToRead > static_cast<int>(sizeof(buffer))) {
      bytesToRead = static_cast<int>(sizeof(buffer));
    }

    size_t bytesRead = Serial.readBytes(reinterpret_cast<char *>(buffer), bytesToRead);
    if (bytesRead > 0) {
      bleSerial.write(buffer, bytesRead);
      Serial.printf("Broadcast %u byte(s) to %u peer(s)\n", (unsigned)bytesRead, (unsigned)bleSerial.peerCount());
    }
  }

  delay(5);
}
