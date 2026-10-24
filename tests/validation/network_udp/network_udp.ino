/*
 * NetworkUDP IPv4/IPv6 loopback validation.
 * Verifies sends without an explicit local bind and preserves bound-client coverage.
 */

#include <Arduino.h>
#include <NetworkUdp.h>
#include <unity.h>
#include <esp_netif.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <unistd.h>

static NetworkUDP sender;
static int receiverFd = -1;

void setUp(void) {
  esp_err_t err = esp_netif_init();
  TEST_ASSERT_TRUE(err == ESP_OK || err == ESP_ERR_INVALID_STATE);
}

void tearDown(void) {
  sender.stop();
  if (receiverFd >= 0) {
    close(receiverFd);
    receiverFd = -1;
  }
}

static uint16_t socket_port(const sockaddr_storage &address) {
#if LWIP_IPV6
  if (address.ss_family == AF_INET6) {
    return ntohs(reinterpret_cast<const sockaddr_in6 *>(&address)->sin6_port);
  }
#endif
  return ntohs(reinterpret_cast<const sockaddr_in *>(&address)->sin_port);
}

static void check_udp_loopback(int family, bool bindSender) {
  receiverFd = socket(family, SOCK_DGRAM, 0);
  TEST_ASSERT_GREATER_OR_EQUAL(0, receiverFd);

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
#if LWIP_IPV6
  else {
    auto *addr = reinterpret_cast<sockaddr_in6 *>(&address);
    addr->sin6_family = AF_INET6;
    TEST_ASSERT_EQUAL(1, inet_pton(AF_INET6, "::1", &addr->sin6_addr));
    addressLength = sizeof(*addr);
    destination = IPAddress(IPv6, reinterpret_cast<const uint8_t *>(&addr->sin6_addr));
  }
#endif

  TEST_ASSERT_EQUAL(0, bind(receiverFd, reinterpret_cast<sockaddr *>(&address), addressLength));
  TEST_ASSERT_EQUAL(0, getsockname(receiverFd, reinterpret_cast<sockaddr *>(&address), &addressLength));
  uint16_t port = socket_port(address);
  TEST_ASSERT_NOT_EQUAL(0, port);

  timeval timeout = {3, 0};
  TEST_ASSERT_EQUAL(0, setsockopt(receiverFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)));

  if (bindSender) {
    TEST_ASSERT_EQUAL(1, sender.begin(destination, 0));
  }

  uint16_t firstSourcePort = 0;
  for (uint8_t i = 0; i < 2; i++) {
    const uint8_t payload[] = {'U', 'D', 'P', 0, i};
    TEST_ASSERT_EQUAL(1, sender.beginPacket(destination, port));
    TEST_ASSERT_EQUAL(sizeof(payload), sender.write(payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(1, sender.endPacket());

    uint8_t received[sizeof(payload) + 1] = {};
    sockaddr_storage source = {};
    socklen_t sourceLength = sizeof(source);
    int count = recvfrom(receiverFd, received, sizeof(received), 0, reinterpret_cast<sockaddr *>(&source), &sourceLength);
    TEST_ASSERT_EQUAL(sizeof(payload), count);
    TEST_ASSERT_EQUAL_MEMORY(payload, received, sizeof(payload));
    TEST_ASSERT_EQUAL(family, source.ss_family);

    uint16_t sourcePort = socket_port(source);
    TEST_ASSERT_NOT_EQUAL(0, sourcePort);
    if (i == 0) {
      firstSourcePort = sourcePort;
    } else {
      TEST_ASSERT_EQUAL(firstSourcePort, sourcePort);
    }
  }

  close(receiverFd);
  receiverFd = -1;
}

void test_udp_ipv4_without_local_bind(void) {
  check_udp_loopback(AF_INET, false);
}

void test_udp_ipv6_without_local_bind(void) {
#if LWIP_IPV6
  check_udp_loopback(AF_INET6, false);
#else
  TEST_IGNORE_MESSAGE("IPv6 is disabled");
#endif
}

void test_udp_ipv4_with_local_bind(void) {
  check_udp_loopback(AF_INET, true);
}

void test_udp_ipv6_with_local_bind(void) {
#if LWIP_IPV6
  check_udp_loopback(AF_INET6, true);
#else
  TEST_IGNORE_MESSAGE("IPv6 is disabled");
#endif
}

void test_udp_socket_family_after_stop(void) {
#if LWIP_IPV6
  check_udp_loopback(AF_INET, false);
  sender.stop();
  check_udp_loopback(AF_INET6, false);
  sender.stop();
  check_udp_loopback(AF_INET, false);
#else
  TEST_IGNORE_MESSAGE("IPv6 is disabled");
#endif
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();
  RUN_TEST(test_udp_ipv4_without_local_bind);
  RUN_TEST(test_udp_ipv6_without_local_bind);
  RUN_TEST(test_udp_ipv4_with_local_bind);
  RUN_TEST(test_udp_ipv6_with_local_bind);
  RUN_TEST(test_udp_socket_family_after_stop);
  UNITY_END();
}

void loop() {}
