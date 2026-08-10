/*
 * Ethernet Validation Test
 *
 * Covers: ETH begin, link up event, DHCP IP acquisition,
 *         MAC address validation, link speed, gateway/subnet/DNS
 *         validation, DNS resolution, TCP data transfer, static IP.
 *
 * Requires hardware with ethernet PHY (RMII or SPI).
 * Runner: ethernet
 */

#include <Arduino.h>
#include <ETH.h>
#include <NetworkClient.h>
#include <unity.h>

#define ETH_TIMEOUT_MS 30000

static volatile bool eth_connected = false;
static volatile bool eth_got_ip = false;

void setUp(void) {}
void tearDown(void) {}

static void onEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_CONNECTED: eth_connected = true; break;
    case ARDUINO_EVENT_ETH_GOT_IP:    eth_got_ip = true; break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      eth_connected = false;
      eth_got_ip = false;
      break;
    default: break;
  }
}

static bool waitForIP(unsigned long timeout_ms = ETH_TIMEOUT_MS) {
  unsigned long start = millis();
  while (!eth_got_ip && millis() - start < timeout_ms) {
    delay(100);
  }
  return eth_got_ip;
}

// ==================== Link Up ====================

void test_eth_link_up(void) {
  TEST_ASSERT_TRUE_MESSAGE(waitForIP(), "Ethernet did not get IP within timeout");
  TEST_ASSERT_TRUE(eth_connected);
}

// ==================== IP Address ====================

void test_eth_ip_valid(void) {
  TEST_ASSERT_TRUE_MESSAGE(eth_got_ip, "No IP assigned");
  IPAddress ip = ETH.localIP();
  TEST_ASSERT_NOT_EQUAL(0, (uint32_t)ip);
}

// ==================== MAC Address ====================

void test_eth_mac_address(void) {
  String mac = ETH.macAddress();
  TEST_ASSERT_EQUAL(17, mac.length());
  int colons = 0;
  for (unsigned int i = 0; i < mac.length(); i++) {
    if (mac[i] == ':') {
      colons++;
    }
  }
  TEST_ASSERT_EQUAL(5, colons);
}

// ==================== Link Speed ====================

void test_eth_link_speed(void) {
  TEST_ASSERT_TRUE(eth_connected);
  uint8_t speed = ETH.linkSpeed();
  TEST_ASSERT_TRUE(speed == 10 || speed == 100);
  ETH.fullDuplex();  // smoke-check: call succeeds without crashing
}

// ==================== Gateway ====================

void test_eth_gateway(void) {
  IPAddress gw = ETH.gatewayIP();
  TEST_ASSERT_NOT_EQUAL_MESSAGE(0, (uint32_t)gw, "Gateway IP is 0.0.0.0");
}

// ==================== Subnet Mask ====================

void test_eth_subnet(void) {
  IPAddress sn = ETH.subnetMask();
  uint32_t mask = (uint32_t)sn;
  TEST_ASSERT_NOT_EQUAL_MESSAGE(0, mask, "Subnet mask is 0.0.0.0");
  // Common subnet masks: 255.255.255.0, 255.255.0.0, etc.
  TEST_ASSERT_TRUE_MESSAGE(sn[0] == 255, "First octet of subnet should be 255");
}

// ==================== DNS Server ====================

void test_eth_dns_server(void) {
  IPAddress dns = ETH.dnsIP();
  TEST_ASSERT_NOT_EQUAL_MESSAGE(0, (uint32_t)dns, "DNS server IP is 0.0.0.0");
}

// ==================== DNS Resolution ====================

void test_eth_dns_resolution(void) {
  IPAddress resolved;
  int ret = Network.hostByName("pool.ntp.org", resolved);
  TEST_ASSERT_EQUAL_MESSAGE(1, ret, "DNS resolution failed for pool.ntp.org");
  TEST_ASSERT_NOT_EQUAL(0, (uint32_t)resolved);
}

// ==================== TCP Connect to Gateway ====================

void test_eth_tcp_gateway(void) {
  TEST_ASSERT_TRUE_MESSAGE(eth_got_ip, "No IP assigned");

  // Connect to the gateway on port 80 -- most CI routers have a web interface
  // If port 80 is not open, try a raw TCP connect which still verifies the data path
  NetworkClient client;
  IPAddress gw = ETH.gatewayIP();

  bool connected = client.connect(gw, 80, 5000);
  if (!connected) {
    // Fallback: try port 53 (DNS) on the DNS server
    connected = client.connect(ETH.dnsIP(), 53, 5000);
  }
  TEST_ASSERT_TRUE_MESSAGE(connected, "TCP connect failed to both gateway:80 and DNS:53");
  client.stop();
}

// ==================== Static IP ====================

void test_eth_static_ip(void) {
  IPAddress static_ip(192, 168, 200, 250);
  IPAddress gw = ETH.gatewayIP();
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns = ETH.dnsIP();

  ETH.config(static_ip, gw, subnet, dns);
  delay(1000);

  IPAddress current = ETH.localIP();
  TEST_ASSERT_TRUE_MESSAGE(current == static_ip, "Static IP was not applied");

  // Restore DHCP
  ETH.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  delay(3000);

  IPAddress restored = ETH.localIP();
  TEST_ASSERT_NOT_EQUAL_MESSAGE(0, (uint32_t)restored, "Failed to restore DHCP");
}

// ==================== Setup ====================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Network.onEvent(onEvent);
  ETH.begin();

  UNITY_BEGIN();

  RUN_TEST(test_eth_link_up);
  RUN_TEST(test_eth_ip_valid);
  RUN_TEST(test_eth_mac_address);
  RUN_TEST(test_eth_link_speed);
  RUN_TEST(test_eth_gateway);
  RUN_TEST(test_eth_subnet);
  RUN_TEST(test_eth_dns_server);
  RUN_TEST(test_eth_dns_resolution);
  RUN_TEST(test_eth_tcp_gateway);
  RUN_TEST(test_eth_static_ip);

  UNITY_END();
}

void loop() {}
