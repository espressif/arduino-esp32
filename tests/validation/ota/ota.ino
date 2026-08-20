/*
 * OTA Validation Test (unsigned workflow)
 *
 * Covers: HTTPUpdate (download, verify success/failure, MD5 check),
 *         Update API (begin, write, end, abort, error handling),
 *         ArduinoOTA (begin/end, hostname, espota upload IPv4/IPv6, with/without auth).
 *
 * WiFi credentials and HTTP server URL are received via serial from pytest.
 * The pytest harness serves firmware binaries over HTTP and drives espota.py
 * when the DUT prints ARDUINO_OTA_BEGIN.
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
#include <ArduinoOTA.h>
#include <unity.h>

#define WIFI_TIMEOUT_MS        15000
#define IPV6_WAIT_MS           10000
#define ARDUINO_OTA_PORT       3232
#define ARDUINO_OTA_TIMEOUT_MS 120000

static String wifi_ssid;
static String wifi_pass;
static String server_url;
// Host IPv6 capability from pytest: NONE | MAPPED | FULL
static String host_ipv6_mode;
static String server_host_v6;  // bare IPv6 host when mode is FULL
static uint16_t server_port_v6 = 0;

static bool hostSupportsMappedIPv6() {
  return host_ipv6_mode == "MAPPED" || host_ipv6_mode == "FULL";
}

static bool hostSupportsGlobalIPv6() {
  return host_ipv6_mode == "FULL" && server_host_v6.length() > 0 && server_port_v6 != 0;
}

static volatile bool s_ota_done = false;
static volatile bool s_ota_ok = false;
static volatile int s_ota_error = -1;

void setUp(void) {}
void tearDown(void) {
  httpUpdate.setMD5sum("");
  httpUpdate.setSHA256sum("");
  if (Update.isRunning()) {
    Update.abort();
  }
}

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

#if CONFIG_LWIP_IPV6
static bool ensureIPv6Enabled() {
  if (!WiFi.enableIPv6()) {
    return false;
  }
  // Link-local is created immediately; global needs RA — wait briefly.
  unsigned long start = millis();
  while (!WiFi.STA.hasGlobalIPv6() && (millis() - start) < IPV6_WAIT_MS) {
    delay(100);
  }
  return true;
}

static bool hasGlobalIPv6() {
  return WiFi.STA.hasGlobalIPv6();
}
#endif

static void resetArduinoOtaFlags() {
  s_ota_done = false;
  s_ota_ok = false;
  s_ota_error = -1;
}

static void configureArduinoOtaCallbacks() {
  ArduinoOTA.onStart([]() {
    Serial.println("ArduinoOTA starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("ArduinoOTA finished");
    s_ota_ok = true;
    s_ota_done = true;
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("ArduinoOTA error %u\n", (unsigned)error);
    s_ota_error = (int)error;
    s_ota_ok = false;
    s_ota_done = true;
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total == 0) {
      return;
    }
    static unsigned last_pct = 0xFFFF;
    unsigned pct = progress * 100 / total;
    if (pct != last_pct && (pct % 25 == 0 || pct == 100)) {
      Serial.printf("ArduinoOTA progress %u%%\n", pct);
      last_pct = pct;
    }
  });
}

static bool runArduinoOtaUpload(const char *auth, const IPAddress &listen_ip) {
  resetArduinoOtaFlags();

  ArduinoOTA.setPort(ARDUINO_OTA_PORT);
  ArduinoOTA.setHostname("ota-validation");
  ArduinoOTA.setMdnsEnabled(false);
  ArduinoOTA.setRebootOnSuccess(false);
  if (auth && auth[0] != '\0') {
    ArduinoOTA.setPassword(auth);
  }
  configureArduinoOtaCallbacks();
  ArduinoOTA.begin();

  // Pytest watches for this line and runs tools/espota.py.
  // IPv6 addresses may contain ':' — fields are space-separated.
  Serial.printf("ARDUINO_OTA_BEGIN %s %u %s\n", listen_ip.toString().c_str(), ARDUINO_OTA_PORT, (auth && auth[0]) ? auth : "NONE");

  unsigned long start = millis();
  while (!s_ota_done && (millis() - start) < ARDUINO_OTA_TIMEOUT_MS) {
    ArduinoOTA.handle();
    delay(1);
  }

  ArduinoOTA.end();

  if (!s_ota_done) {
    Serial.println("ArduinoOTA timed out waiting for upload");
    return false;
  }
  if (!s_ota_ok) {
    Serial.printf("ArduinoOTA failed with error %d\n", s_ota_error);
    return false;
  }
  return true;
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

void test_update_sha256_format(void) {
  TEST_ASSERT_TRUE(Update.begin(1024));
  TEST_ASSERT_FALSE(Update.setSHA256("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b85z"));
  TEST_ASSERT_TRUE(Update.setSHA256("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  Update.abort();
}

// ==================== ArduinoOTA Tests ====================

void test_arduino_ota_begin_end(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  ArduinoOTA.setPort(ARDUINO_OTA_PORT);
  ArduinoOTA.setHostname("ota-validation");
  ArduinoOTA.setMdnsEnabled(false);
  ArduinoOTA.setRebootOnSuccess(false);
  ArduinoOTA.begin();

  TEST_ASSERT_EQUAL_STRING("ota-validation", ArduinoOTA.getHostname().c_str());

  ArduinoOTA.end();
}

void test_arduino_ota_upload_no_auth(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  TEST_ASSERT_TRUE_MESSAGE(runArduinoOtaUpload(nullptr, WiFi.localIP()), "ArduinoOTA upload without auth failed");
}

void test_arduino_ota_upload_with_auth(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  TEST_ASSERT_TRUE_MESSAGE(runArduinoOtaUpload("test-ota", WiFi.localIP()), "ArduinoOTA upload with auth failed");
}

void test_arduino_ota_upload_ipv6(void) {
#if !CONFIG_LWIP_IPV6
  TEST_IGNORE_MESSAGE("IPv6 not enabled in this build");
#else
  if (!hostSupportsGlobalIPv6()) {
    TEST_IGNORE_MESSAGE("Host has no global IPv6");
  }
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  if (!ensureIPv6Enabled()) {
    TEST_IGNORE_MESSAGE("Failed to enable IPv6 on DUT");
  }
  if (!hasGlobalIPv6()) {
    TEST_IGNORE_MESSAGE("No global IPv6 address (router RA unavailable)");
  }

  IPAddress v6 = WiFi.globalIPv6();
  Serial.printf("Using global IPv6: %s\n", v6.toString().c_str());
  TEST_ASSERT_TRUE_MESSAGE(runArduinoOtaUpload(nullptr, v6), "ArduinoOTA IPv6 upload failed");
#endif
}

// Dual-stack bind (::) must still accept IPv4 invites after IPv6 is enabled on STA.
void test_arduino_ota_ipv4_with_ipv6_enabled(void) {
#if !CONFIG_LWIP_IPV6
  TEST_IGNORE_MESSAGE("IPv6 not enabled in this build");
#else
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  if (!ensureIPv6Enabled()) {
    TEST_IGNORE_MESSAGE("Failed to enable IPv6 on DUT");
  }
  TEST_ASSERT_TRUE_MESSAGE(runArduinoOtaUpload(nullptr, WiFi.localIP()), "IPv4 ArduinoOTA failed while IPv6 was enabled");
#endif
}

// After a successful IPv6 session, a fresh begin() must still accept IPv4 invites.
void test_arduino_ota_ipv4_after_ipv6(void) {
#if !CONFIG_LWIP_IPV6
  TEST_IGNORE_MESSAGE("IPv6 not enabled in this build");
#else
  if (!hostSupportsGlobalIPv6()) {
    TEST_IGNORE_MESSAGE("Host has no global IPv6");
  }
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  if (!ensureIPv6Enabled()) {
    TEST_IGNORE_MESSAGE("Failed to enable IPv6 on DUT");
  }
  if (!hasGlobalIPv6()) {
    TEST_IGNORE_MESSAGE("No global IPv6 address (router RA unavailable)");
  }

  IPAddress v6 = WiFi.globalIPv6();
  TEST_ASSERT_TRUE_MESSAGE(runArduinoOtaUpload(nullptr, v6), "IPv6 upload (setup for v4-after-v6) failed");
  TEST_ASSERT_TRUE_MESSAGE(runArduinoOtaUpload(nullptr, WiFi.localIP()), "IPv4 ArduinoOTA failed after a prior IPv6 upload");
#endif
}

void test_arduino_ota_ipv6_with_auth(void) {
#if !CONFIG_LWIP_IPV6
  TEST_IGNORE_MESSAGE("IPv6 not enabled in this build");
#else
  if (!hostSupportsGlobalIPv6()) {
    TEST_IGNORE_MESSAGE("Host has no global IPv6");
  }
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  if (!ensureIPv6Enabled()) {
    TEST_IGNORE_MESSAGE("Failed to enable IPv6 on DUT");
  }
  if (!hasGlobalIPv6()) {
    TEST_IGNORE_MESSAGE("No global IPv6 address (router RA unavailable)");
  }

  IPAddress v6 = WiFi.globalIPv6();
  TEST_ASSERT_TRUE_MESSAGE(runArduinoOtaUpload("test-ota-v6", v6), "ArduinoOTA IPv6 upload with auth failed");
#endif
}

// Host uses IPv4-mapped IPv6 literals (::ffff:a.b.c.d) while the DUT listens dual-stack.
void test_arduino_ota_ipv4_mapped(void) {
#if !CONFIG_LWIP_IPV6
  TEST_IGNORE_MESSAGE("IPv6 not enabled in this build");
#else
  if (!hostSupportsMappedIPv6()) {
    TEST_IGNORE_MESSAGE("Host has no IPv6 socket support for mapped addresses");
  }
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  if (!ensureIPv6Enabled()) {
    TEST_IGNORE_MESSAGE("Failed to enable IPv6 on DUT");
  }

  resetArduinoOtaFlags();
  ArduinoOTA.setPort(ARDUINO_OTA_PORT);
  ArduinoOTA.setHostname("ota-validation");
  ArduinoOTA.setMdnsEnabled(false);
  ArduinoOTA.setRebootOnSuccess(false);
  configureArduinoOtaCallbacks();
  ArduinoOTA.begin();

  // Pytest maps both DUT and host IPv4 into ::ffff: form for espota.
  Serial.printf("ARDUINO_OTA_BEGIN_MAPPED %s %u NONE\n", WiFi.localIP().toString().c_str(), ARDUINO_OTA_PORT);

  unsigned long start = millis();
  while (!s_ota_done && (millis() - start) < ARDUINO_OTA_TIMEOUT_MS) {
    ArduinoOTA.handle();
    delay(1);
  }
  ArduinoOTA.end();

  TEST_ASSERT_TRUE_MESSAGE(s_ota_done, "ArduinoOTA IPv4-mapped upload timed out");
  TEST_ASSERT_TRUE_MESSAGE(s_ota_ok, "ArduinoOTA IPv4-mapped upload failed");
#endif
}

// ==================== HTTPUpdate Tests ====================

void test_httpupdate_invalid_url(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkClient client;
  httpUpdate.rebootOnUpdate(false);
  HTTPUpdateResult ret = httpUpdate.update(client, "http://192.0.2.1:9999/nonexistent.bin");
  TEST_ASSERT_NOT_EQUAL(HTTP_UPDATE_OK, ret);
}

void test_httpupdate_invalid_url_ipv6(void) {
#if !CONFIG_LWIP_IPV6
  TEST_IGNORE_MESSAGE("IPv6 not enabled in this build");
#else
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  if (!ensureIPv6Enabled()) {
    TEST_IGNORE_MESSAGE("Failed to enable IPv6 on DUT");
  }

  // RFC 3986 bracketed IPv6 URL (documentation address — must not succeed).
  NetworkClient client;
  httpUpdate.rebootOnUpdate(false);
  HTTPUpdateResult ret = httpUpdate.update(client, "http://[2001:db8::1]:9999/nonexistent.bin");
  TEST_ASSERT_NOT_EQUAL(HTTP_UPDATE_OK, ret);
#endif
}

void test_httpupdate_invalid_checksums_abort(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  TEST_ASSERT_TRUE_MESSAGE(server_url.length() > 0, "No server URL provided");

  NetworkClient client;
  httpUpdate.rebootOnUpdate(false);
  String url = server_url + "/ota.ino.bin";

  httpUpdate.setMD5sum("invalid");
  TEST_ASSERT_EQUAL(HTTP_UPDATE_FAILED, httpUpdate.update(client, url));
  TEST_ASSERT_FALSE(Update.isRunning());
  httpUpdate.setMD5sum("");

  httpUpdate.setSHA256sum("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b85z");
  TEST_ASSERT_EQUAL(HTTP_UPDATE_FAILED, httpUpdate.update(client, url));
  TEST_ASSERT_FALSE(Update.isRunning());
  httpUpdate.setSHA256sum("");
}

void test_httpupdate_wrong_sha256_has_no_digest(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  TEST_ASSERT_TRUE_MESSAGE(server_url.length() > 0, "No server URL provided");

  NetworkClient client;
  httpUpdate.rebootOnUpdate(false);
  httpUpdate.setSHA256sum("0000000000000000000000000000000000000000000000000000000000000000");
  String url = server_url + "/ota.ino.bin";
  TEST_ASSERT_EQUAL(HTTP_UPDATE_FAILED, httpUpdate.update(client, url));
  TEST_ASSERT_FALSE(Update.isRunning());
  TEST_ASSERT_TRUE(Update.sha256String().isEmpty());
  httpUpdate.setSHA256sum("");
}

void test_httpupdate_download(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  TEST_ASSERT_TRUE_MESSAGE(server_url.length() > 0, "No server URL provided");

  NetworkClient client;
  httpUpdate.rebootOnUpdate(false);
  String url = server_url + "/ota.ino.bin";
  HTTPUpdateResult ret = httpUpdate.update(client, url);

  TEST_ASSERT_EQUAL_MESSAGE(HTTP_UPDATE_OK, ret, "HTTPUpdate could not connect to server or download failed");
  TEST_ASSERT_EQUAL(64, Update.sha256String().length());
}

void test_httpupdate_download_ipv6(void) {
#if !CONFIG_LWIP_IPV6
  TEST_IGNORE_MESSAGE("IPv6 not enabled in this build");
#else
  if (!hostSupportsGlobalIPv6()) {
    TEST_IGNORE_MESSAGE("Host has no global IPv6 HTTP server");
  }
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  if (!ensureIPv6Enabled()) {
    TEST_IGNORE_MESSAGE("Failed to enable IPv6 on DUT");
  }
  if (!hasGlobalIPv6()) {
    TEST_IGNORE_MESSAGE("No global IPv6 address (router RA unavailable)");
  }

  NetworkClient client;
  httpUpdate.rebootOnUpdate(false);
  String url = "http://[" + server_host_v6 + "]:" + String(server_port_v6) + "/ota.ino.bin";
  HTTPUpdateResult ret = httpUpdate.update(client, url);
  TEST_ASSERT_TRUE_MESSAGE(ret == HTTP_UPDATE_OK || ret == HTTP_UPDATE_NO_UPDATES, "HTTPUpdate IPv6 download failed");
#endif
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

  while (Serial.available()) {
    Serial.read();
  }

  Serial.println("Send IPv6 Mode (NONE|MAPPED|FULL):");
  host_ipv6_mode = waitForLine();
  host_ipv6_mode.toUpperCase();
  if (host_ipv6_mode != "MAPPED" && host_ipv6_mode != "FULL") {
    host_ipv6_mode = "NONE";
  }

  while (Serial.available()) {
    Serial.read();
  }

  Serial.println("Send IPv6 Server Host (or NONE):");
  server_host_v6 = waitForLine();
  if (server_host_v6 == "NONE") {
    server_host_v6 = "";
  }

  while (Serial.available()) {
    Serial.read();
  }

  Serial.println("Send IPv6 Server Port (or 0):");
  String port_line = waitForLine();
  server_port_v6 = (uint16_t)port_line.toInt();
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
  RUN_TEST(test_update_sha256_format);
  RUN_TEST(test_arduino_ota_begin_end);
  RUN_TEST(test_httpupdate_invalid_url);
  RUN_TEST(test_httpupdate_invalid_url_ipv6);
  RUN_TEST(test_httpupdate_invalid_checksums_abort);
  RUN_TEST(test_httpupdate_wrong_sha256_has_no_digest);
  // ArduinoOTA uploads before HTTPUpdate download so partition state stays predictable.
  // Keep all no-auth cases before any setPassword() so leftover hashes cannot force AUTH.
  RUN_TEST(test_arduino_ota_upload_no_auth);
  RUN_TEST(test_arduino_ota_ipv4_with_ipv6_enabled);
  RUN_TEST(test_arduino_ota_upload_ipv6);
  RUN_TEST(test_arduino_ota_ipv4_after_ipv6);
  RUN_TEST(test_arduino_ota_ipv4_mapped);
  RUN_TEST(test_arduino_ota_ipv6_with_auth);
  RUN_TEST(test_arduino_ota_upload_with_auth);
  RUN_TEST(test_httpupdate_download);
  RUN_TEST(test_httpupdate_download_ipv6);

  UNITY_END();
}

void loop() {}
