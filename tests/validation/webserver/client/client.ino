/*
  client.ino - HTTP client device for multi-DUT WebServer validation test.

  Connects to the server's WiFi AP, then makes HTTP requests to test
  WebServer::send(code, content_type, Stream&) overload.

  Communicates with the Python test runner via serial protocol.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define SERVER_PORT  80
#define TEST_TIMEOUT 5000

// Test data (must match server)
static const char test_body[] = "Hello from Stream!";
static const uint8_t test_data[] = {0xDE, 0xAD, 0xBE, 0xEF};

String ssid = "";
String password = "";
String serverIP = "";

void readWiFiCredentials() {
  Serial.println("[CLIENT] Send SSID:");
  while (ssid.length() == 0) {
    if (Serial.available()) {
      ssid = Serial.readStringUntil('\n');
      ssid.trim();
    }
    delay(100);
  }

  Serial.println("[CLIENT] Send Password:");
  bool password_received = false;
  while (!password_received) {
    if (Serial.available()) {
      password = Serial.readStringUntil('\n');
      password.trim();
      password_received = true;
    }
    delay(100);
  }

  Serial.printf("[CLIENT] SSID: %s\n", ssid.c_str());
  Serial.printf("[CLIENT] Password: %s\n", password.c_str());
}

void readServerIP() {
  Serial.println("[CLIENT] Send server IP:");
  while (serverIP.length() == 0) {
    if (Serial.available()) {
      serverIP = Serial.readStringUntil('\n');
      serverIP.trim();
    }
    delay(100);
  }
  Serial.printf("[CLIENT] Server IP: %s\n", serverIP.c_str());
}

// Send an HTTP GET request and return the raw response
String http_get(const char *path) {
  WiFiClient client;
  if (!client.connect(serverIP.c_str(), SERVER_PORT)) {
    Serial.printf("[CLIENT] Failed to connect to %s:%d\n", serverIP.c_str(), SERVER_PORT);
    return "";
  }

  client.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, serverIP.c_str());

  unsigned long start = millis();
  while (client.connected() && !client.available()) {
    if (millis() - start > TEST_TIMEOUT) {
      client.stop();
      return "";
    }
    delay(1);
  }

  String response;
  unsigned long readStart = millis();
  while (client.connected() || client.available()) {
    if (client.available()) {
      response += (char)client.read();
      readStart = millis();
    } else if (millis() - readStart > TEST_TIMEOUT) {
      break;
    } else {
      delay(1);
    }
  }
  client.stop();
  return response;
}

// Extract body from HTTP response (after \r\n\r\n)
String get_body(const String &response) {
  int idx = response.indexOf("\r\n\r\n");
  if (idx < 0) {
    return "";
  }
  return response.substring(idx + 4);
}

// Extract HTTP status code from response
int get_status_code(const String &response) {
  int idx = response.indexOf(' ');
  if (idx < 0) {
    return -1;
  }
  return response.substring(idx + 1).toInt();
}

// Extract Content-Length header value
int get_content_length(const String &response) {
  int idx = response.indexOf("Content-Length: ");
  if (idx < 0) {
    return -1;
  }
  int end = response.indexOf("\r\n", idx);
  String val = response.substring(idx + 16, end);
  return val.toInt();
}

void runTests() {
  bool all_passed = true;

  // Test 1: Stream with text data
  {
    Serial.println("[CLIENT] Testing /stream_text");
    String response = http_get("/stream_text");
    int cl = get_content_length(response);
    String body = get_body(response);
    if (response.length() == 0) {
      Serial.println("[CLIENT] FAIL stream_text: no response");
      all_passed = false;
    } else if (cl != (int)strlen(test_body)) {
      Serial.printf("[CLIENT] FAIL stream_text: Content-Length=%d expected=%d\n", cl, (int)strlen(test_body));
      all_passed = false;
    } else if (!body.equals(test_body)) {
      Serial.println("[CLIENT] FAIL stream_text: body mismatch");
      all_passed = false;
    } else {
      Serial.println("[CLIENT] PASS stream_text");
    }
  }

  // Test 2: Stream with binary data
  {
    Serial.println("[CLIENT] Testing /stream_binary");
    String response = http_get("/stream_binary");
    int cl = get_content_length(response);
    String body = get_body(response);
    if (response.length() == 0) {
      Serial.println("[CLIENT] FAIL stream_binary: no response");
      all_passed = false;
    } else if (cl != 4) {
      Serial.printf("[CLIENT] FAIL stream_binary: Content-Length=%d expected=4\n", cl);
      all_passed = false;
    } else if (body.length() != 4 || (uint8_t)body[0] != 0xDE || (uint8_t)body[1] != 0xAD || (uint8_t)body[2] != 0xBE || (uint8_t)body[3] != 0xEF) {
      Serial.println("[CLIENT] FAIL stream_binary: body mismatch");
      all_passed = false;
    } else {
      Serial.println("[CLIENT] PASS stream_binary");
    }
  }

  // Test 3: Stream with explicit content length
  {
    Serial.println("[CLIENT] Testing /stream_explicit_len");
    String response = http_get("/stream_explicit_len");
    int cl = get_content_length(response);
    if (response.length() == 0) {
      Serial.println("[CLIENT] FAIL stream_explicit_len: no response");
      all_passed = false;
    } else if (cl != (int)strlen(test_body)) {
      Serial.printf("[CLIENT] FAIL stream_explicit_len: Content-Length=%d expected=%d\n", cl, (int)strlen(test_body));
      all_passed = false;
    } else {
      Serial.println("[CLIENT] PASS stream_explicit_len");
    }
  }

  // Test 4: Empty stream should return 204
  {
    Serial.println("[CLIENT] Testing /stream_empty");
    String response = http_get("/stream_empty");
    int status = get_status_code(response);
    String body = get_body(response);
    if (response.length() == 0) {
      Serial.println("[CLIENT] FAIL stream_empty: no response");
      all_passed = false;
    } else if (status != 204) {
      Serial.printf("[CLIENT] FAIL stream_empty: status=%d expected=204\n", status);
      all_passed = false;
    } else if (body.length() != 0) {
      Serial.println("[CLIENT] FAIL stream_empty: body should be empty");
      all_passed = false;
    } else {
      Serial.println("[CLIENT] PASS stream_empty");
    }
  }

  // Test 5: String send regression
  {
    Serial.println("[CLIENT] Testing /string");
    String response = http_get("/string");
    String body = get_body(response);
    if (response.length() == 0) {
      Serial.println("[CLIENT] FAIL string: no response");
      all_passed = false;
    } else if (!body.equals("OK")) {
      Serial.println("[CLIENT] FAIL string: body mismatch");
      all_passed = false;
    } else {
      Serial.println("[CLIENT] PASS string");
    }
  }

  if (all_passed) {
    Serial.println("[CLIENT] All tests passed");
  } else {
    Serial.println("[CLIENT] Some tests failed");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  Serial.println("[CLIENT] Device ready for WiFi credentials");
  readWiFiCredentials();
  readServerIP();

  // Connect to server's AP
  WiFi.STA.begin();
  WiFi.STA.connect(ssid, password);

  Serial.printf("[CLIENT] Connecting to SSID=%s\n", ssid.c_str());

  int retries = 50;
  while (WiFi.STA.status() != WL_CONNECTED && retries--) {
    delay(200);
  }

  if (WiFi.STA.status() != WL_CONNECTED) {
    Serial.println("[CLIENT] Failed to connect");
    return;
  }

  Serial.printf("[CLIENT] Connected IP=%s\n", WiFi.STA.localIP().toString().c_str());

  // Run all tests
  runTests();
}

void loop() {}
