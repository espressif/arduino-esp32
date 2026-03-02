/*
  webserver.ino - Validation tests for WebServer send(Stream&) overload.

  Tests the ability to send Stream objects (e.g., File) directly via
  WebServer::send(), matching the ESP8266 WebServer API. Uses WiFi
  softAP for self-contained testing without an external router.

  Verifies:
    - send(code, content_type, stream) delivers correct body
    - Content-Length header matches stream size
    - Explicit content_length parameter is honored
    - Empty stream sends headers only
    - Existing send(code, content_type, string) still works
*/

#include <Arduino.h>
#include <unity.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>

#define AP_SSID      "webserver_test"
#define SERVER_PORT  8080
#define TEST_TIMEOUT 5000

static WebServer server(SERVER_PORT);
static IPAddress serverIP(192, 168, 4, 1);

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

/* These functions are intended to be called before and after each test. */
void setUp(void) {}

void tearDown(void) {}

// Send an HTTP GET request and return the raw response
static String http_get(const char *path) {
  WiFiClient client;
  if (!client.connect(serverIP, SERVER_PORT)) {
    return "";
  }

  client.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, serverIP.toString().c_str());

  unsigned long start = millis();
  while (client.connected() && !client.available()) {
    if (millis() - start > TEST_TIMEOUT) {
      client.stop();
      return "";
    }
    server.handleClient();
    delay(1);
  }

  String response;
  while (client.available()) {
    response += (char)client.read();
  }
  client.stop();
  return response;
}

// Extract body from HTTP response (after \r\n\r\n)
static String get_body(const String &response) {
  int idx = response.indexOf("\r\n\r\n");
  if (idx < 0) {
    return "";
  }
  return response.substring(idx + 4);
}

// Extract Content-Length header value
static int get_content_length(const String &response) {
  int idx = response.indexOf("Content-Length: ");
  if (idx < 0) {
    return -1;
  }
  int end = response.indexOf("\r\n", idx);
  String val = response.substring(idx + 16, end);
  return val.toInt();
}

// Test: send(code, content_type, Stream&) with text data
void test_send_stream_text(void) {
  String response = http_get("/stream_text");
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, response.length(), "No response received");

  int cl = get_content_length(response);
  TEST_ASSERT_EQUAL_MESSAGE((int)strlen(test_body), cl, "Content-Length mismatch");

  String body = get_body(response);
  TEST_ASSERT_EQUAL_STRING_MESSAGE(test_body, body.c_str(), "Body mismatch");
}

// Test: send(code, content_type, Stream&) with binary data
void test_send_stream_binary(void) {
  String response = http_get("/stream_binary");
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, response.length(), "No response received");

  int cl = get_content_length(response);
  TEST_ASSERT_EQUAL_MESSAGE(4, cl, "Content-Length should be 4");

  String body = get_body(response);
  TEST_ASSERT_EQUAL_MESSAGE(4, body.length(), "Body should be 4 bytes");
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xDE, (uint8_t)body[0], "Byte 0 mismatch");
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xAD, (uint8_t)body[1], "Byte 1 mismatch");
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xBE, (uint8_t)body[2], "Byte 2 mismatch");
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xEF, (uint8_t)body[3], "Byte 3 mismatch");
}

// Test: send(code, content_type, Stream&, explicit_length)
void test_send_stream_explicit_length(void) {
  String response = http_get("/stream_explicit_len");
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, response.length(), "No response received");

  int cl = get_content_length(response);
  TEST_ASSERT_EQUAL_MESSAGE((int)strlen(test_body), cl, "Content-Length should match explicit value");
}

// Test: send(code, content_type, Stream&) with empty stream
void test_send_stream_empty(void) {
  String response = http_get("/stream_empty");
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, response.length(), "No response received");

  int cl = get_content_length(response);
  TEST_ASSERT_EQUAL_MESSAGE(0, cl, "Content-Length should be 0");

  String body = get_body(response);
  TEST_ASSERT_EQUAL_MESSAGE(0, body.length(), "Body should be empty");
}

// Test: existing send(code, content_type, String) still works (regression)
void test_send_string_regression(void) {
  String response = http_get("/string");
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, response.length(), "No response received");

  String body = get_body(response);
  TEST_ASSERT_EQUAL_STRING_MESSAGE("OK", body.c_str(), "String send regression failed");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Start WiFi softAP for self-contained testing
  WiFi.softAP(AP_SSID);
  delay(500);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Register test handlers
  server.on("/stream_text", HTTP_GET, []() {
    TestStream stream((const uint8_t *)test_body, strlen(test_body));
    server.send(200, "text/plain", stream);
  });

  server.on("/stream_binary", HTTP_GET, []() {
    TestStream stream(test_data, sizeof(test_data));
    server.send(200, "application/octet-stream", stream);
  });

  server.on("/stream_explicit_len", HTTP_GET, []() {
    TestStream stream((const uint8_t *)test_body, strlen(test_body));
    server.send(200, "text/plain", stream, strlen(test_body));
  });

  server.on("/stream_empty", HTTP_GET, []() {
    uint8_t empty = 0;
    TestStream stream(&empty, 0);
    server.send(200, "text/plain", stream);
  });

  server.on("/string", HTTP_GET, []() {
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("Server started");
  delay(500);

  UNITY_BEGIN();
  RUN_TEST(test_send_stream_text);
  RUN_TEST(test_send_stream_binary);
  RUN_TEST(test_send_stream_explicit_length);
  RUN_TEST(test_send_stream_empty);
  RUN_TEST(test_send_string_regression);
  UNITY_END();
}

void loop() {}
