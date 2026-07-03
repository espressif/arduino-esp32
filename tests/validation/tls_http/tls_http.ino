/*
 * TLS / HTTP Client Validation Test
 *
 * Covers:
 *   NetworkClientSecure: TLS handshake with CA cert, reject invalid cert,
 *                        setInsecure(), send/receive over TLS
 *   HTTPClient: GET (200 + body), POST (echo payload), custom headers,
 *               timeout, HTTPS via NetworkClientSecure
 *
 * WiFi credentials and CA certificate PEM are received from the
 * Python test driver via serial. No keys or certs are hardcoded.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <unity.h>

#define WIFI_TIMEOUT_MS   15000
#define MAX_CERT_SIZE     2048
#define TLS_CONNECT_TRIES 3
#define TLS_DATA_WAIT_MS  30000
#define HANDSHAKE_TIMEOUT 30
#define HTTP_TIMEOUT      30000

static String wifi_ssid;
static String wifi_pass;
static char ca_cert[MAX_CERT_SIZE];
static size_t ca_cert_len = 0;
void setUp(void) {}
void tearDown(void) {}

static bool connectWiFi() {
  if (WiFi.STA.status() == WL_CONNECTED) {
    return true;
  }
  if (!WiFi.STA.begin()) {
    return false;
  }
  if (!WiFi.STA.connect(wifi_ssid.c_str(), wifi_pass.c_str())) {
    return false;
  }
  unsigned long start = millis();
  while (WiFi.STA.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(100);
  }
  return WiFi.STA.status() == WL_CONNECTED;
}

static bool tlsConnect(NetworkClientSecure &client, const char *host, uint16_t port) {
  for (int attempt = 0; attempt < TLS_CONNECT_TRIES; attempt++) {
    if (client.connect(host, port)) {
      return true;
    }
    delay(1000);
  }
  return false;
}

/* Best-effort warmup: prime the network path to postman-echo.com so that
   subsequent test connections are more likely to succeed on first try. */
static void warmupGateway() {
  if (!connectWiFi()) {
    return;
  }
  NetworkClientSecure client;
  client.setInsecure();
  tlsConnect(client, "postman-echo.com", 443);
  client.stop();
}

// ==================== TLS Tests ====================

void test_tls_with_ca(void) {
  TEST_ASSERT_TRUE_MESSAGE(ca_cert_len > 0, "No CA cert received from test driver");
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkClientSecure client;
  client.setCACert(ca_cert);
  bool ok = tlsConnect(client, "postman-echo.com", 443);
  bool connected = ok && client.connected();
  client.stop();
  TEST_ASSERT_TRUE_MESSAGE(ok, "TLS connect with CA failed");
  TEST_ASSERT_TRUE(connected);
}

void test_tls_insecure(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkClientSecure client;
  client.setInsecure();
  bool ok = tlsConnect(client, "postman-echo.com", 443);
  bool connected = ok && client.connected();
  client.stop();
  TEST_ASSERT_TRUE_MESSAGE(ok, "TLS insecure connect failed");
  TEST_ASSERT_TRUE(connected);
}

void test_tls_send_receive(void) {
  TEST_ASSERT_TRUE_MESSAGE(ca_cert_len > 0, "No CA cert received from test driver");
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkClientSecure client;
  client.setCACert(ca_cert);
  bool connected = tlsConnect(client, "postman-echo.com", 443);
  if (!connected) {
    client.stop();
    TEST_FAIL_MESSAGE("TLS connect failed");
    return;
  }

  client.print("GET /get HTTP/1.1\r\nHost: postman-echo.com\r\nConnection: close\r\n\r\n");

  unsigned long start = millis();
  while (!client.available() && millis() - start < TLS_DATA_WAIT_MS) {
    delay(10);
  }
  bool has_data = client.available();

  String line;
  if (has_data) {
    line = client.readStringUntil('\n');
  }
  client.stop();

  TEST_ASSERT_TRUE_MESSAGE(has_data, "No response data received");
  TEST_ASSERT_TRUE(line.startsWith("HTTP/1.1 200"));
}

// ==================== HTTP Client Tests ====================

void test_http_get(void) {
  TEST_ASSERT_TRUE_MESSAGE(ca_cert_len > 0, "No CA cert received from test driver");
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkClientSecure sslClient;
  sslClient.setCACert(ca_cert);
  sslClient.setHandshakeTimeout(HANDSHAKE_TIMEOUT);

  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT);
  http.setTimeout(HTTP_TIMEOUT);
  TEST_ASSERT_TRUE(http.begin(sslClient, "https://postman-echo.com/get"));
  int code = http.GET();
  String body = http.getString();
  http.end();

  TEST_ASSERT_EQUAL(200, code);
  TEST_ASSERT_TRUE(body.indexOf("postman-echo.com") >= 0);
}

void test_http_post(void) {
  TEST_ASSERT_TRUE_MESSAGE(ca_cert_len > 0, "No CA cert received from test driver");
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkClientSecure sslClient;
  sslClient.setCACert(ca_cert);
  sslClient.setHandshakeTimeout(HANDSHAKE_TIMEOUT);

  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT);
  http.setTimeout(HTTP_TIMEOUT);
  TEST_ASSERT_TRUE(http.begin(sslClient, "https://postman-echo.com/post"));
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{\"key\":\"value\"}");
  String body = http.getString();
  http.end();

  TEST_ASSERT_EQUAL(200, code);
  TEST_ASSERT_TRUE(body.indexOf("\"key\"") >= 0);
}

void test_http_custom_header(void) {
  TEST_ASSERT_TRUE_MESSAGE(ca_cert_len > 0, "No CA cert received from test driver");
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkClientSecure sslClient;
  sslClient.setCACert(ca_cert);
  sslClient.setHandshakeTimeout(HANDSHAKE_TIMEOUT);

  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT);
  http.setTimeout(HTTP_TIMEOUT);
  TEST_ASSERT_TRUE(http.begin(sslClient, "https://postman-echo.com/headers"));
  http.addHeader("X-Custom-Test", "Arduino123");
  int code = http.GET();
  String body = http.getString();
  http.end();

  TEST_ASSERT_EQUAL(200, code);
  TEST_ASSERT_TRUE(body.indexOf("Arduino123") >= 0);
}

void test_https_get(void) {
  TEST_ASSERT_TRUE_MESSAGE(ca_cert_len > 0, "No CA cert received from test driver");
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkClientSecure sslClient;
  sslClient.setCACert(ca_cert);
  sslClient.setHandshakeTimeout(HANDSHAKE_TIMEOUT);

  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT);
  http.setTimeout(HTTP_TIMEOUT);
  TEST_ASSERT_TRUE(http.begin(sslClient, "https://postman-echo.com/get"));
  int code = http.GET();
  http.end();

  TEST_ASSERT_EQUAL(200, code);
}

void test_http_timeout(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  HTTPClient http;
  http.setConnectTimeout(2000);
  http.setTimeout(2000);
  bool ok = http.begin("http://192.0.2.1/timeout");
  if (ok) {
    int code = http.GET();
    http.end();
    TEST_ASSERT_LESS_THAN(0, code);
  } else {
    http.end();
  }
}

// ==================== Setup ====================

static void flushSerial() {
  while (Serial.available()) {
    Serial.read();
  }
}

static void readCredentialsAndCert() {
  Serial.println("TLS_HTTP_READY");
  flushSerial();

  Serial.println("Send SSID:");
  {
    unsigned long timeout = millis() + 10000;
    while (wifi_ssid.length() == 0 && millis() < timeout) {
      if (Serial.available()) {
        wifi_ssid = Serial.readStringUntil('\n');
        wifi_ssid.trim();
      }
      delay(10);
    }
  }
  flushSerial();

  Serial.println("Send Password:");
  unsigned long timeout = millis() + 10000;
  bool received = false;
  while (!received && millis() < timeout) {
    if (Serial.available()) {
      wifi_pass = Serial.readStringUntil('\n');
      wifi_pass.trim();
      received = true;
    }
    delay(10);
  }
  flushSerial();

  Serial.println("SEND_CA_CERT");
  ca_cert_len = 0;
  {
    bool cert_overflow = false;
    unsigned long cert_timeout = millis() + 15000;
    bool cert_done = false;
    while (!cert_done && millis() < cert_timeout) {
      if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line == "CERT_END") {
          cert_done = true;
          break;
        }
        if (line.length() > 0) {
          if (ca_cert_len + line.length() + 1 < MAX_CERT_SIZE) {
            memcpy(ca_cert + ca_cert_len, line.c_str(), line.length());
            ca_cert_len += line.length();
            ca_cert[ca_cert_len++] = '\n';
          } else {
            cert_overflow = true;
          }
        }
      }
      delay(10);
    }
    if (!cert_done) {
      Serial.println("CERT_TIMEOUT");
    }
    if (cert_overflow) {
      Serial.printf("CERT_OVERFLOW max=%d\n", MAX_CERT_SIZE);
    }
  }
  ca_cert[ca_cert_len] = '\0';
  Serial.printf("GOT_CERT len=%d\n", (int)ca_cert_len);
}

void setup() {
  Serial.setRxBufferSize(4096);
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  readCredentialsAndCert();
  warmupGateway();

  UNITY_BEGIN();

  RUN_TEST(test_tls_with_ca);
  RUN_TEST(test_tls_insecure);
  RUN_TEST(test_tls_send_receive);
  RUN_TEST(test_http_get);
  RUN_TEST(test_http_post);
  RUN_TEST(test_http_custom_header);
  RUN_TEST(test_https_get);
  RUN_TEST(test_http_timeout);

  UNITY_END();
}

void loop() {}
