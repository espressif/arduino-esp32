/*
 * Networking Validation Test
 *
 * Covers: NetworkClient TCP (connect, send/receive, timeout, large payload),
 *         NetworkUDP (unicast send/receive, multi-packet, remote info),
 *         NetworkServer (accept, echo),
 *         DNS resolution (hostByName),
 *         DNSServer (captive portal),
 *         ESPmDNS (hostname, service registration, query).
 *
 * WiFi credentials are received from the Python test driver via serial.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClient.h>
#include <NetworkUdp.h>
#include <NetworkServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <sys/time.h>
#include <unistd.h>
#include <unity.h>

#define WIFI_TIMEOUT_MS 15000

#if defined(CONFIG_LWIP_NETIF_LOOPBACK) && CONFIG_LWIP_NETIF_LOOPBACK
#define NETWORKING_HAS_LWIP_LOOPBACK 1
#else
#define NETWORKING_HAS_LWIP_LOOPBACK 0
#endif

#if defined(LWIP_IPV6) && LWIP_IPV6
#define NETWORKING_HAS_LWIP_IPV6 1
#else
#define NETWORKING_HAS_LWIP_IPV6 0
#endif

static String wifi_ssid;
static String wifi_pass;
static NetworkUDP udpLoopbackSender;
static int udpLoopbackReceiverFd = -1;

void setUp(void) {}
void tearDown(void) {
  udpLoopbackSender.stop();
  if (udpLoopbackReceiverFd >= 0) {
    close(udpLoopbackReceiverFd);
    udpLoopbackReceiverFd = -1;
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

// ==================== DNS Resolution ====================

void test_dns_resolve(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  IPAddress resolved;
  int ret = Network.hostByName("pool.ntp.org", resolved);
  TEST_ASSERT_EQUAL_MESSAGE(1, ret, "DNS resolution failed for pool.ntp.org");
  TEST_ASSERT_NOT_EQUAL(0, (uint32_t)resolved);
}

void test_dns_resolve_invalid(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  IPAddress resolved;
  int ret = Network.hostByName("this.domain.does.not.exist.invalid", resolved);
  TEST_ASSERT_LESS_OR_EQUAL(0, ret);
}

// ==================== TCP Client ====================

void test_tcp_connect(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  NetworkClient client;
  TEST_ASSERT_TRUE_MESSAGE(client.connect("example.com", 80), "TCP connect failed");
  TEST_ASSERT_TRUE(client.connected());
  client.stop();
}

void test_tcp_send_receive(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkServer server(8891);
  server.begin();

  NetworkClient client;
  TEST_ASSERT_TRUE_MESSAGE(client.connect(IPAddress(127, 0, 0, 1), 8891), "TCP connect failed");

  NetworkClient remote = server.accept();
  TEST_ASSERT_TRUE_MESSAGE((bool)remote, "Server accept failed");

  const char *msg = "SEND_RECEIVE_TEST";
  client.print(msg);
  delay(50);

  String received = remote.readString();
  TEST_ASSERT_TRUE(received.indexOf(msg) >= 0);

  const char *reply = "SEND_RECEIVE_REPLY";
  remote.print(reply);
  remote.stop();

  unsigned long start = millis();
  while (!client.available() && millis() - start < 5000) {
    delay(10);
  }
  TEST_ASSERT_TRUE(client.available());

  String response = client.readString();
  TEST_ASSERT_TRUE(response.indexOf(reply) >= 0);

  client.stop();
  server.end();
}

void test_tcp_timeout(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  NetworkClient client;
  client.setTimeout(2);
  bool connected = client.connect("192.0.2.1", 12345);
  TEST_ASSERT_FALSE(connected);
}

void test_tcp_large_payload(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  const size_t PAYLOAD_SIZE = 2048;
  NetworkServer server(8890);
  server.begin();

  NetworkClient client;
  TEST_ASSERT_TRUE_MESSAGE(client.connect(IPAddress(127, 0, 0, 1), 8890), "TCP connect failed");

  NetworkClient remote = server.accept();
  TEST_ASSERT_TRUE_MESSAGE((bool)remote, "Server accept failed");

  uint8_t buf[64];
  memset(buf, 0xAA, sizeof(buf));
  size_t sent = 0;
  while (sent < PAYLOAD_SIZE) {
    size_t chunk = min(sizeof(buf), PAYLOAD_SIZE - sent);
    remote.write(buf, chunk);
    sent += chunk;
  }
  remote.stop();

  unsigned long start = millis();
  size_t totalRead = 0;
  while (millis() - start < 10000) {
    while (client.available()) {
      client.read();
      totalRead++;
    }
    if (!client.connected() && !client.available()) {
      break;
    }
    delay(10);
  }
  TEST_ASSERT_GREATER_OR_EQUAL(PAYLOAD_SIZE, totalRead);

  client.stop();
  server.end();
}

// ==================== TCP Server ====================

void test_tcp_server_echo(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkServer server(8888);
  server.begin();

  NetworkClient local_client;
  TEST_ASSERT_TRUE(local_client.connect(IPAddress(127, 0, 0, 1), 8888));

  NetworkClient remote = server.accept();
  TEST_ASSERT_TRUE(remote);

  const char *msg = "PING_ECHO_TEST";
  local_client.print(msg);
  delay(100);

  String data = remote.readString();
  TEST_ASSERT_TRUE(data.indexOf(msg) >= 0);

  // Echo back
  remote.print(data);
  delay(100);
  String echo = local_client.readString();
  TEST_ASSERT_TRUE(echo.indexOf(msg) >= 0);

  remote.stop();
  local_client.stop();
  server.end();
}

// ==================== UDP ====================

void test_udp_send_receive(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkUDP udpServer;
  TEST_ASSERT_TRUE(udpServer.begin(9999));

  NetworkUDP udpClient;
  TEST_ASSERT_TRUE(udpClient.begin(0));

  udpClient.beginPacket(IPAddress(127, 0, 0, 1), 9999);
  udpClient.print("HELLO_UDP");
  udpClient.endPacket();

  unsigned long start = millis();
  int packetSize = 0;
  while (packetSize == 0 && millis() - start < 3000) {
    packetSize = udpServer.parsePacket();
    delay(10);
  }
  TEST_ASSERT_GREATER_THAN(0, packetSize);

  char buf[32] = {0};
  int len = udpServer.read(buf, sizeof(buf) - 1);
  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_EQUAL_STRING("HELLO_UDP", buf);

  udpServer.stop();
  udpClient.stop();
}

void test_udp_multi_packet(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkUDP receiver;
  TEST_ASSERT_TRUE(receiver.begin(9990));

  NetworkUDP sender;
  sender.begin(0);

  const int NUM_PACKETS = 5;
  for (int i = 0; i < NUM_PACKETS; i++) {
    sender.beginPacket(IPAddress(127, 0, 0, 1), 9990);
    sender.printf("PKT%d", i);
    sender.endPacket();
    delay(20);
  }

  int received = 0;
  unsigned long start = millis();
  while (received < NUM_PACKETS && millis() - start < 3000) {
    if (receiver.parsePacket() > 0) {
      char buf[16] = {0};
      receiver.read(buf, sizeof(buf) - 1);
      received++;
    }
    delay(10);
  }
  TEST_ASSERT_GREATER_OR_EQUAL(NUM_PACKETS - 1, received);

  receiver.stop();
  sender.stop();
}

void test_udp_remote_info(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  NetworkUDP receiver;
  TEST_ASSERT_TRUE(receiver.begin(9998));

  NetworkUDP sender;
  sender.begin(9997);
  sender.beginPacket(IPAddress(127, 0, 0, 1), 9998);
  sender.print("INFO");
  sender.endPacket();

  unsigned long start = millis();
  while (receiver.parsePacket() == 0 && millis() - start < 3000) {
    delay(10);
  }

  IPAddress remoteIP = receiver.remoteIP();
  TEST_ASSERT_EQUAL((uint32_t)IPAddress(127, 0, 0, 1), (uint32_t)remoteIP);
  TEST_ASSERT_EQUAL(9997, receiver.remotePort());

  receiver.stop();
  sender.stop();
}

// Verify lazy socket creation and socket-family changes after stop().
#if NETWORKING_HAS_LWIP_LOOPBACK
static uint16_t udp_loopback_socket_port(const sockaddr_storage &address) {
#if NETWORKING_HAS_LWIP_IPV6
  if (address.ss_family == AF_INET6) {
    return ntohs(reinterpret_cast<const sockaddr_in6 *>(&address)->sin6_port);
  }
#endif
  return ntohs(reinterpret_cast<const sockaddr_in *>(&address)->sin_port);
}

static void check_udp_loopback(int family, bool bindSender) {
  udpLoopbackReceiverFd = socket(family, SOCK_DGRAM, 0);
  TEST_ASSERT_GREATER_OR_EQUAL(0, udpLoopbackReceiverFd);

  sockaddr_storage address = {};
  socklen_t addressLength = sizeof(sockaddr_in);
  IPAddress destination;
  if (family == AF_INET) {
    auto *addr = reinterpret_cast<sockaddr_in *>(&address);
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addressLength = sizeof(*addr);
    destination = IPAddress(127, 0, 0, 1);
  }
#if NETWORKING_HAS_LWIP_IPV6
  else {
    auto *addr = reinterpret_cast<sockaddr_in6 *>(&address);
    addr->sin6_family = AF_INET6;
    TEST_ASSERT_EQUAL(1, inet_pton(AF_INET6, "::1", &addr->sin6_addr));
    addressLength = sizeof(*addr);
    destination = IPAddress(IPv6, reinterpret_cast<const uint8_t *>(&addr->sin6_addr));
  }
#endif

  TEST_ASSERT_EQUAL(0, bind(udpLoopbackReceiverFd, reinterpret_cast<sockaddr *>(&address), addressLength));
  TEST_ASSERT_EQUAL(0, getsockname(udpLoopbackReceiverFd, reinterpret_cast<sockaddr *>(&address), &addressLength));
  uint16_t port = udp_loopback_socket_port(address);
  TEST_ASSERT_NOT_EQUAL(0, port);

  timeval timeout = {3, 0};
  TEST_ASSERT_EQUAL(0, setsockopt(udpLoopbackReceiverFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)));

  if (bindSender) {
    TEST_ASSERT_EQUAL(1, udpLoopbackSender.begin(destination, 0));
  }

  uint16_t firstSourcePort = 0;
  for (uint8_t i = 0; i < 2; i++) {
    const uint8_t payload[] = {'U', 'D', 'P', 0, i};
    TEST_ASSERT_EQUAL(1, udpLoopbackSender.beginPacket(destination, port));
    TEST_ASSERT_EQUAL(sizeof(payload), udpLoopbackSender.write(payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(1, udpLoopbackSender.endPacket());

    uint8_t received[sizeof(payload) + 1] = {};
    sockaddr_storage source = {};
    socklen_t sourceLength = sizeof(source);
    int count = recvfrom(udpLoopbackReceiverFd, received, sizeof(received), 0, reinterpret_cast<sockaddr *>(&source), &sourceLength);
    TEST_ASSERT_EQUAL(sizeof(payload), count);
    TEST_ASSERT_EQUAL_MEMORY(payload, received, sizeof(payload));
    TEST_ASSERT_EQUAL(family, source.ss_family);

    uint16_t sourcePort = udp_loopback_socket_port(source);
    TEST_ASSERT_NOT_EQUAL(0, sourcePort);
    if (i == 0) {
      firstSourcePort = sourcePort;
    } else {
      TEST_ASSERT_EQUAL(firstSourcePort, sourcePort);
    }
  }

  close(udpLoopbackReceiverFd);
  udpLoopbackReceiverFd = -1;
}

void test_udp_ipv4_without_local_bind(void) {
  check_udp_loopback(AF_INET, false);
}

void test_udp_ipv6_without_local_bind(void) {
#if NETWORKING_HAS_LWIP_IPV6
  check_udp_loopback(AF_INET6, false);
#else
  TEST_IGNORE_MESSAGE("IPv6 is disabled");
#endif
}

void test_udp_ipv4_with_local_bind(void) {
  check_udp_loopback(AF_INET, true);
}

void test_udp_ipv6_with_local_bind(void) {
#if NETWORKING_HAS_LWIP_IPV6
  check_udp_loopback(AF_INET6, true);
#else
  TEST_IGNORE_MESSAGE("IPv6 is disabled");
#endif
}

void test_udp_socket_family_after_stop(void) {
#if NETWORKING_HAS_LWIP_IPV6
  check_udp_loopback(AF_INET, false);
  udpLoopbackSender.stop();
  check_udp_loopback(AF_INET6, false);
  udpLoopbackSender.stop();
  check_udp_loopback(AF_INET, false);
#else
  TEST_IGNORE_MESSAGE("IPv6 is disabled");
#endif
}
#else
void test_udp_loopback_requires_configuration(void) {
  TEST_IGNORE_MESSAGE("Loopback support is disabled");
}
#endif

// ==================== DNS Server ====================

void test_dns_server_captive(void) {
#if !SOC_WIFI_SUPPORTED
  TEST_IGNORE_MESSAGE("SoftAP requires native WiFi (not available on this SoC)");
#else
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");

  TEST_ASSERT_TRUE(WiFi.AP.create("ESP32_DNS_Test", "12345678"));
  delay(500);

  DNSServer dnsServer;
  IPAddress apIP = WiFi.AP.localIP();
  TEST_ASSERT_TRUE(dnsServer.start(53, "*", apIP));

  dnsServer.processNextRequest();

  dnsServer.stop();
  WiFi.AP.end();
  delay(100);
#endif
}

// ==================== mDNS ====================

void test_mdns_begin(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  TEST_ASSERT_TRUE(MDNS.begin("esp32-nettest"));
  MDNS.end();
}

void test_mdns_service(void) {
  TEST_ASSERT_TRUE_MESSAGE(connectWiFi(), "WiFi connect failed");
  TEST_ASSERT_TRUE(MDNS.begin("esp32-svc"));
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("myservice", "udp", 1234);
  MDNS.end();
}

// ==================== Setup ====================

static void readCredentials() {
  Serial.println("NET_READY");

  while (Serial.available()) {
    Serial.read();
  }

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

  UNITY_BEGIN();

  RUN_TEST(test_dns_resolve);
  RUN_TEST(test_dns_resolve_invalid);
  RUN_TEST(test_tcp_connect);
  RUN_TEST(test_tcp_send_receive);
  RUN_TEST(test_tcp_timeout);
  RUN_TEST(test_tcp_large_payload);
  RUN_TEST(test_tcp_server_echo);
  RUN_TEST(test_udp_send_receive);
  RUN_TEST(test_udp_multi_packet);
  RUN_TEST(test_udp_remote_info);
#if NETWORKING_HAS_LWIP_LOOPBACK
  RUN_TEST(test_udp_ipv4_without_local_bind);
  RUN_TEST(test_udp_ipv6_without_local_bind);
  RUN_TEST(test_udp_ipv4_with_local_bind);
  RUN_TEST(test_udp_ipv6_with_local_bind);
  RUN_TEST(test_udp_socket_family_after_stop);
#else
  RUN_TEST(test_udp_loopback_requires_configuration);
#endif
  RUN_TEST(test_dns_server_captive);
  RUN_TEST(test_mdns_begin);
  RUN_TEST(test_mdns_service);

  UNITY_END();
}

void loop() {}
