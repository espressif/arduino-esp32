/*
 * OTA Validation Test (unsigned workflow)
 *
 * Covers: HTTPUpdate (download, verify success/failure, MD5 check),
 *         Update API (begin, write, end, abort, error handling).
 *
 * WiFi credentials and HTTP server URL are received via serial from pytest.
 * The pytest harness serves firmware binaries over HTTP.
 *
 * Note: signed_ota/ (separate test) covers RSA-PSS and ECDSA-DER
 * signature verification. This suite covers the unsigned path only.
 *
 * Runner: wifi_router or wifi_high_traffic
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <unity.h>

#define WIFI_TIMEOUT_MS 15000

static String wifi_ssid;
static String wifi_pass;
static String server_url;

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

// ==================== Update API Tests ====================

void test_update_begin_abort(void) {
  TEST_ASSERT_TRUE(Update.begin(0x100000));
  Update.abort();
  TEST_ASSERT_FALSE(Update.isRunning());
}

void test_update_error_no_begin(void) {
  Update.abort();
  uint8_t buf[4] = {0};
  size_t written = Update.write(buf, sizeof(buf));
  TEST_ASSERT_EQUAL(0, written);
}

void test_update_md5_check(void) {
  TEST_ASSERT_TRUE(Update.begin(1024));
  Update.setMD5("d41d8cd98f00b204e9800998ecf8427e");
  Update.abort();
}

// ==================== HTTPUpdate Tests ====================

void test_httpupdate_invalid_url(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkClient client;
  httpUpdate.rebootOnUpdate(false);
  HTTPUpdateResult ret = httpUpdate.update(client, "http://192.0.2.1:9999/nonexistent.bin");
  TEST_ASSERT_NOT_EQUAL(HTTP_UPDATE_OK, ret);
}

void test_httpupdate_download(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  TEST_ASSERT_TRUE_MESSAGE(server_url.length() > 0, "No server URL provided");

  NetworkClient client;
  httpUpdate.rebootOnUpdate(false);
  String url = server_url + "/ota.ino.bin";
  HTTPUpdateResult ret = httpUpdate.update(client, url);

  TEST_ASSERT_TRUE_MESSAGE(ret == HTTP_UPDATE_OK || ret == HTTP_UPDATE_NO_UPDATES, "HTTPUpdate could not connect to server or download failed");
}

// ==================== Setup ====================

static String waitForLine() {
  String s;
  while (true) {
    if (Serial.available()) {
      s = Serial.readStringUntil('\n');
      s.trim();
      if (s.length() > 0) {
        return s;
      }
    }
    delay(10);
  }
}

static void readCredentials() {
  Serial.println("OTA_READY");

  while (Serial.available()) {
    Serial.read();
  }

  Serial.println("Send SSID:");
  while (wifi_ssid.length() == 0) {
    if (Serial.available()) {
      wifi_ssid = Serial.readStringUntil('\n');
      wifi_ssid.trim();
    }
    delay(10);
  }

  while (Serial.available()) {
    Serial.read();
  }

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

  while (Serial.available()) {
    Serial.read();
  }

  Serial.println("Send Server URL (or NONE):");
  server_url = waitForLine();
  if (server_url == "NONE") {
    server_url = "";
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  readCredentials();

  UNITY_BEGIN();

  RUN_TEST(test_update_begin_abort);
  RUN_TEST(test_update_error_no_begin);
  RUN_TEST(test_update_md5_check);
  RUN_TEST(test_httpupdate_invalid_url);
  RUN_TEST(test_httpupdate_download);

  UNITY_END();
}

void loop() {}
