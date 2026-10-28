#include <Arduino.h>
#include <DNSServer.h>
#include <esp_netif.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <sys/time.h>
#include <unistd.h>
#include <unity.h>

#include <cstring>

static constexpr uint16_t kDnsPort = 15353;

void setUp() {
  esp_err_t err = esp_netif_init();
  TEST_ASSERT_TRUE(err == ESP_OK || err == ESP_ERR_INVALID_STATE);
}

void tearDown() {}

#if defined(CONFIG_LWIP_NETIF_LOOPBACK) && CONFIG_LWIP_NETIF_LOOPBACK
static bool sendQuestion(int fd, const sockaddr_in &serverAddress, uint16_t id, const uint8_t *question, size_t questionLength) {
  uint8_t packet[DNS_HEADER_SIZE + 32] = {};
  if (questionLength > sizeof(packet) - DNS_HEADER_SIZE) {
    return false;
  }

  packet[0] = id >> 8;
  packet[1] = id & 0xff;
  packet[2] = 0x01;  // recursion desired
  packet[5] = 0x01;  // one question
  memcpy(packet + DNS_HEADER_SIZE, question, questionLength);

  size_t length = DNS_HEADER_SIZE + questionLength;
  ssize_t sent = sendto(fd, packet, length, 0, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress));
  return sent >= 0 && static_cast<size_t>(sent) == length;
}

static void expectNoReply(int fd) {
  uint8_t reply[64];
  TEST_ASSERT_EQUAL(-1, recvfrom(fd, reply, sizeof(reply), 0, nullptr, nullptr));
}

void test_dns_question_stays_within_packet() {
  DNSServer server;
  TEST_ASSERT_TRUE(server.start(kDnsPort, "*", IPAddress(127, 0, 0, 1)));

  int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

  sockaddr_in localAddress = {};
  localAddress.sin_family = AF_INET;
  localAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  TEST_ASSERT_EQUAL(0, bind(fd, reinterpret_cast<const sockaddr *>(&localAddress), sizeof(localAddress)));

  timeval timeout = {0, 300000};
  TEST_ASSERT_EQUAL(0, setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)));

  sockaddr_in serverAddress = {};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  serverAddress.sin_port = htons(kDnsPort);

  const uint8_t noTerminator[] = {3, 'a', 'b', 'c', 1, 1, 1, 1};
  TEST_ASSERT_TRUE(sendQuestion(fd, serverAddress, 0x1001, noTerminator, sizeof(noTerminator)));
  expectNoReply(fd);

  const uint8_t terminatorInQType[] = {3, 'a', 'b', 'c', 0, 1, 0, 1};
  TEST_ASSERT_TRUE(sendQuestion(fd, serverAddress, 0x1002, terminatorInQType, sizeof(terminatorInQType)));
  expectNoReply(fd);

  const uint8_t truncatedQuestion[] = {0, 0, 1, 0};
  TEST_ASSERT_TRUE(sendQuestion(fd, serverAddress, 0x1003, truncatedQuestion, sizeof(truncatedQuestion)));
  expectNoReply(fd);

  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  TEST_ASSERT_EQUAL(0, setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)));

  const uint8_t rootQuestion[] = {0, 0, 1, 0, 1};
  TEST_ASSERT_TRUE(sendQuestion(fd, serverAddress, 0x1004, rootQuestion, sizeof(rootQuestion)));

  uint8_t reply[64];
  int received = recvfrom(fd, reply, sizeof(reply), 0, nullptr, nullptr);
  TEST_ASSERT_GREATER_THAN(DNS_HEADER_SIZE, received);
  TEST_ASSERT_EQUAL_HEX8(0x10, reply[0]);
  TEST_ASSERT_EQUAL_HEX8(0x04, reply[1]);
  DNSHeader response;
  memcpy(&response, reply, sizeof(response));
  TEST_ASSERT_EQUAL(DNS_QR_RESPONSE, response.QR);
  TEST_ASSERT_EQUAL_UINT16(1, ntohs(response.ANCount));

  close(fd);
  server.stop();
}
#else
void test_dns_question_stays_within_packet() {
  TEST_IGNORE_MESSAGE("LwIP loopback support is disabled");
}
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  UNITY_BEGIN();
  RUN_TEST(test_dns_question_stays_within_packet);
  UNITY_END();
}

void loop() {}
