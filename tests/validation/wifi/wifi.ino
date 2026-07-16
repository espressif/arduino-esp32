/*
 * WiFi Validation Test
 *
 * Covers: STA connect with assertions, SSID/IP/MAC/RSSI/channel/BSSID
 * verification, gateway/DNS, scan networks, disconnect/reconnect,
 * auto-reconnect, hostname, static IP configuration, AP mode (softAP),
 * WiFi event verification, and mode switching.
 *
 * WiFi credentials are received from the Python test driver via serial.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <unity.h>

#define WIFI_TIMEOUT_MS 15000

static String wifi_ssid;
static String wifi_pass;

void setUp(void) {}
void tearDown(void) {}

static bool waitForConnection(unsigned long timeout_ms = WIFI_TIMEOUT_MS) {
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout_ms) {
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
}

static bool waitForDisconnection(unsigned long timeout_ms = 5000) {
  unsigned long start = millis();
  while (WiFi.status() == WL_CONNECTED && millis() - start < timeout_ms) {
    delay(100);
  }
  return WiFi.status() != WL_CONNECTED;
}

// ==================== STA Basic Tests ====================

void test_sta_connect(void) {
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "WiFi STA connect timed out");
  TEST_ASSERT_EQUAL(WL_CONNECTED, WiFi.status());
  WiFi.disconnect(true);
  waitForDisconnection();
}

void test_sta_ip_valid(void) {
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "WiFi STA connect timed out");
  IPAddress ip = WiFi.localIP();
  TEST_ASSERT_NOT_EQUAL(0, (uint32_t)ip);
  WiFi.disconnect(true);
  waitForDisconnection();
}

void test_sta_ssid_matches(void) {
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "WiFi STA connect timed out");
  TEST_ASSERT_EQUAL_STRING(wifi_ssid.c_str(), WiFi.SSID().c_str());
  WiFi.disconnect(true);
  waitForDisconnection();
}

void test_sta_mac_address(void) {
  String mac = WiFi.macAddress();
  TEST_ASSERT_EQUAL(17, mac.length());
  int colons = 0;
  for (unsigned int i = 0; i < mac.length(); i++) {
    if (mac[i] == ':') {
      colons++;
    }
  }
  TEST_ASSERT_EQUAL(5, colons);
}

// ==================== STA Network Info ====================

void test_sta_rssi(void) {
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "WiFi connect failed");

  int32_t rssi = WiFi.RSSI();
  // Real hardware returns negative dBm; Wokwi may return 0 or arbitrary positive values
  TEST_ASSERT_GREATER_THAN(-100, rssi);
  TEST_ASSERT_LESS_THAN(100, rssi);

  WiFi.disconnect(true);
  waitForDisconnection();
}

void test_sta_channel(void) {
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "WiFi connect failed");

  int32_t channel = WiFi.channel();
  TEST_ASSERT_GREATER_OR_EQUAL(1, channel);
  TEST_ASSERT_LESS_OR_EQUAL(14, channel);

  WiFi.disconnect(true);
  waitForDisconnection();
}

void test_sta_bssid(void) {
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "WiFi connect failed");

  String bssid = WiFi.BSSIDstr();
  TEST_ASSERT_EQUAL(17, bssid.length());

  WiFi.disconnect(true);
  waitForDisconnection();
}

void test_sta_gateway(void) {
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "WiFi connect failed");

  IPAddress gw = WiFi.gatewayIP();
  TEST_ASSERT_NOT_EQUAL_MESSAGE(0, (uint32_t)gw, "Gateway is 0.0.0.0");

  WiFi.disconnect(true);
  waitForDisconnection();
}

void test_sta_dns(void) {
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "WiFi connect failed");

  IPAddress dns = WiFi.dnsIP();
  TEST_ASSERT_NOT_EQUAL_MESSAGE(0, (uint32_t)dns, "DNS server is 0.0.0.0");

  WiFi.disconnect(true);
  waitForDisconnection();
}

// ==================== Hostname ====================

void test_sta_hostname(void) {
  WiFi.setHostname("esp32-wifi-test");
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "WiFi connect failed");

  const char *hostname = WiFi.getHostname();
  TEST_ASSERT_EQUAL_STRING("esp32-wifi-test", hostname);

  WiFi.disconnect(true);
  waitForDisconnection();
}

// ==================== Disconnect / Reconnect ====================

void test_sta_disconnect_reconnect(void) {
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "First connect timed out");

  WiFi.disconnect(true);
  TEST_ASSERT_TRUE_MESSAGE(waitForDisconnection(), "Disconnect timed out");

  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "Reconnect timed out");
  TEST_ASSERT_EQUAL(WL_CONNECTED, WiFi.status());

  WiFi.disconnect(true);
  waitForDisconnection();
}

void test_sta_auto_reconnect(void) {
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "Initial connect failed");

  bool autoReconnect = WiFi.getAutoReconnect();
  TEST_ASSERT_TRUE(autoReconnect);

  WiFi.setAutoReconnect(false);
  TEST_ASSERT_FALSE(WiFi.getAutoReconnect());

  WiFi.disconnect(true);
  waitForDisconnection();
}

// ==================== Scan ====================

void test_scan_networks(void) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  TEST_ASSERT_GREATER_OR_EQUAL(1, n);

  bool found = false;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == wifi_ssid) {
      found = true;
      break;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(found, "Target SSID not found in scan results");
  WiFi.scanDelete();
}

// ==================== Static IP ====================

void test_static_ip(void) {
  IPAddress ip(192, 168, 4, 100);
  IPAddress gw(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(8, 8, 8, 8);

  WiFi.config(ip, gw, subnet, dns);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "Static IP connect timed out");

  IPAddress assigned = WiFi.localIP();
  TEST_ASSERT_EQUAL(ip[0], assigned[0]);
  TEST_ASSERT_EQUAL(ip[1], assigned[1]);
  TEST_ASSERT_EQUAL(ip[2], assigned[2]);
  TEST_ASSERT_EQUAL(ip[3], assigned[3]);

  WiFi.disconnect(true);
  waitForDisconnection();
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
}

// ==================== AP Mode ====================

void test_soft_ap(void) {
  WiFi.disconnect(true);
  delay(100);
  TEST_ASSERT_TRUE(WiFi.softAP("ESP32_Test_AP", "testpass123"));
  delay(500);

  IPAddress apIP = WiFi.softAPIP();
  TEST_ASSERT_NOT_EQUAL(0, (uint32_t)apIP);

  String apSSID = WiFi.softAPSSID();
  TEST_ASSERT_EQUAL_STRING("ESP32_Test_AP", apSSID.c_str());

  TEST_ASSERT_EQUAL(0, WiFi.softAPgetStationNum());

  WiFi.softAPdisconnect(true);
  delay(100);
}

// ==================== WiFi Events ====================

void test_event_fires_on_connect(void) {
  volatile bool got_ip_event = false;

  WiFiEventId_t id = WiFi.onEvent([&got_ip_event](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      got_ip_event = true;
    }
  });
  TEST_ASSERT_NOT_EQUAL(0, id);

  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  TEST_ASSERT_TRUE_MESSAGE(waitForConnection(), "WiFi connect failed");

  delay(500);
  TEST_ASSERT_TRUE_MESSAGE(got_ip_event, "GOT_IP event never fired");

  WiFi.removeEvent(id);
  WiFi.disconnect(true);
  waitForDisconnection();
}

// ==================== Mode Switching ====================

void test_mode_switching(void) {
  TEST_ASSERT_TRUE(WiFi.mode(WIFI_STA));
  TEST_ASSERT_EQUAL(WIFI_STA, WiFi.getMode());

  TEST_ASSERT_TRUE(WiFi.mode(WIFI_AP));
  TEST_ASSERT_EQUAL(WIFI_AP, WiFi.getMode());

  TEST_ASSERT_TRUE(WiFi.mode(WIFI_AP_STA));
  TEST_ASSERT_EQUAL(WIFI_AP_STA, WiFi.getMode());

  TEST_ASSERT_TRUE(WiFi.mode(WIFI_OFF));
  delay(100);
}

// ==================== Setup ====================

static void readCredentials() {
  Serial.println("WIFI_READY");

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
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  readCredentials();

  WiFi.mode(WIFI_STA);

  UNITY_BEGIN();

  RUN_TEST(test_sta_connect);
  RUN_TEST(test_sta_ip_valid);
  RUN_TEST(test_sta_ssid_matches);
  RUN_TEST(test_sta_mac_address);
  RUN_TEST(test_sta_rssi);
  RUN_TEST(test_sta_channel);
  RUN_TEST(test_sta_bssid);
  RUN_TEST(test_sta_gateway);
  RUN_TEST(test_sta_dns);
  RUN_TEST(test_sta_hostname);
  RUN_TEST(test_sta_disconnect_reconnect);
  RUN_TEST(test_sta_auto_reconnect);
  RUN_TEST(test_scan_networks);
  RUN_TEST(test_static_ip);
  RUN_TEST(test_soft_ap);
  RUN_TEST(test_event_fires_on_connect);
  RUN_TEST(test_mode_switching);

  UNITY_END();
}

void loop() {}
