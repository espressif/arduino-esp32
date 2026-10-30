/*
 * BluetoothSerial Multi-Client Example
 *
 * Accepts multiple simultaneous SPP connections (up to the controller's
 * BR/EDR ACL limit) and demonstrates per-peer callbacks, peer enumeration,
 * targeted writes, and broadcast writes.
 *
 * Pair and connect from two phones or PCs to see per-peer echo behavior.
 */

#include <Arduino.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);

  SerialBT.onConnect([](const BTAddress &addr) {
    Serial.printf("Client connected: %s (peers=%u)\n", addr.toString().c_str(), SerialBT.peerCount());
  });

  SerialBT.onDisconnect([](const BTAddress &addr) {
    Serial.printf("Client disconnected: %s (peers=%u)\n", addr.toString().c_str(), SerialBT.peerCount());
  });

  SerialBT.onPeerData([](const BTAddress &addr, const uint8_t *data, size_t len) {
    Serial.printf("RX from %s (%u bytes): ", addr.toString().c_str(), (unsigned)len);
    Serial.write(data, len);
    Serial.println();

    // Echo back only to the sender.
    SerialBT.writeTo(addr, data, len);
  });

  BTStatus status = SerialBT.begin("ESP32-MultiSPP");
  if (!status) {
    Serial.printf("Bluetooth init failed! (%s)\n", status.toString());
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Multi-client SPP server ready.");
  Serial.printf("Local address: %s\n", SerialBT.getAddress().toString().c_str());
  Serial.println("Type in Serial Monitor to broadcast to all connected peers.");
}

void loop() {
  static uint32_t lastHeartbeat = 0;

  if (millis() - lastHeartbeat >= 10000) {
    lastHeartbeat = millis();
    if (SerialBT.peerCount() > 0) {
      String msg = "heartbeat peers=" + String(SerialBT.peerCount()) + "\n";
      SerialBT.write(reinterpret_cast<const uint8_t *>(msg.c_str()), msg.length());
      Serial.println("Broadcast heartbeat to all peers.");
    }
  }

  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line += '\n';
    SerialBT.write(reinterpret_cast<const uint8_t *>(line.c_str()), line.length());
    Serial.printf("Broadcast to %u peer(s): %s", (unsigned)SerialBT.peerCount(), line.c_str());
  }

  delay(10);
}
