/*
  server.ino - WebServer device for multi-DUT validation test.

  Runs a WiFi softAP and WebServer with test endpoints for
  WebServer::send(code, content_type, Stream&) overload.

  Communicates with the Python test runner via serial protocol.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#define SERVER_PORT 80

static WebServer server(SERVER_PORT);

// In-memory stream for testing (simulates a File)
class TestStream : public Stream {
  const uint8_t *_buf;
  size_t _size;
  size_t _pos;

public:
  TestStream(const uint8_t *buf, size_t size) : _buf(buf), _size(size), _pos(0) {}

  int available() override {
    return (int)(_size - _pos);
  }

  int read() override {
    if (_pos >= _size) {
      return -1;
    }
    return _buf[_pos++];
  }

  int peek() override {
    if (_pos >= _size) {
      return -1;
    }
    return _buf[_pos];
  }

  size_t write(uint8_t) override {
    return 0;
  }
};

// Test data
static const char test_body[] = "Hello from Stream!";
static const uint8_t test_data[] = {0xDE, 0xAD, 0xBE, 0xEF};

String ssid = "";
String password = "";

void readWiFiCredentials() {
  Serial.println("[SERVER] Send SSID:");
  while (ssid.length() == 0) {
    if (Serial.available()) {
      ssid = Serial.readStringUntil('\n');
      ssid.trim();
    }
    delay(100);
  }

  Serial.println("[SERVER] Send Password:");
  bool password_received = false;
  while (!password_received) {
    if (Serial.available()) {
      password = Serial.readStringUntil('\n');
      password.trim();
      password_received = true;
    }
    delay(100);
  }

  Serial.printf("[SERVER] SSID: %s\n", ssid.c_str());
  Serial.printf("[SERVER] Password: %s\n", password.c_str());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  Serial.println("[SERVER] Device ready for WiFi credentials");
  readWiFiCredentials();

  // Start WiFi softAP
  WiFi.AP.begin();
  bool ok = WiFi.AP.create(ssid, password);
  if (!ok) {
    Serial.println("[SERVER] Failed to start AP");
    return;
  }

  IPAddress ip = WiFi.AP.localIP();
  Serial.printf("[SERVER] AP started IP=%s\n", ip.toString().c_str());

  // Register test endpoints
  server.on("/stream_text", HTTP_GET, []() {
    TestStream stream((const uint8_t *)test_body, strlen(test_body));
    server.send(200, "text/plain", stream);
    Serial.println("[SERVER] Served /stream_text");
  });

  server.on("/stream_binary", HTTP_GET, []() {
    TestStream stream(test_data, sizeof(test_data));
    server.send(200, "application/octet-stream", stream);
    Serial.println("[SERVER] Served /stream_binary");
  });

  server.on("/stream_explicit_len", HTTP_GET, []() {
    TestStream stream((const uint8_t *)test_body, strlen(test_body));
    server.send(200, "text/plain", stream, strlen(test_body));
    Serial.println("[SERVER] Served /stream_explicit_len");
  });

  server.on("/stream_empty", HTTP_GET, []() {
    uint8_t empty = 0;
    TestStream stream(&empty, 0);
    server.send(200, "text/plain", stream);
    Serial.println("[SERVER] Served /stream_empty");
  });

  server.on("/string", HTTP_GET, []() {
    server.send(200, "text/plain", "OK");
    Serial.println("[SERVER] Served /string");
  });

  server.begin();
  Serial.println("[SERVER] Server started");
}

void loop() {
  server.handleClient();
}
