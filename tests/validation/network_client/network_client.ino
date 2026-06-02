#include <Arduino.h>
#include <NetworkClient.h>
#include <unity.h>

#include <esp_netif.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <unistd.h>

static int create_loopback_client(int *peer_fd) {
  int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  TEST_ASSERT_GREATER_OR_EQUAL(0, listen_fd);

  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;

  TEST_ASSERT_EQUAL(0, bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)));
  TEST_ASSERT_EQUAL(0, listen(listen_fd, 1));

  socklen_t addr_len = sizeof(addr);
  TEST_ASSERT_EQUAL(0, getsockname(listen_fd, reinterpret_cast<sockaddr *>(&addr), &addr_len));

  int client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  TEST_ASSERT_GREATER_OR_EQUAL(0, client_fd);
  TEST_ASSERT_EQUAL(0, connect(client_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)));

  *peer_fd = accept(listen_fd, nullptr, nullptr);
  close(listen_fd);
  TEST_ASSERT_GREATER_OR_EQUAL(0, *peer_fd);

  return client_fd;
}

void test_network_client_available_counts_loopback_bytes(void) {
  esp_err_t err = esp_netif_init();
  TEST_ASSERT_TRUE(err == ESP_OK || err == ESP_ERR_INVALID_STATE);

  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  const char payload[] = "rx-count";
  TEST_ASSERT_EQUAL(sizeof(payload) - 1, send(peer_fd, payload, sizeof(payload) - 1, 0));

  uint32_t deadline = millis() + 3000;
  while (client.available() < static_cast<int>(sizeof(payload) - 1) && millis() < deadline) {
    delay(10);
  }

  TEST_ASSERT_GREATER_OR_EQUAL(sizeof(payload) - 1, client.available());

  char rx[sizeof(payload)] = {};
  TEST_ASSERT_EQUAL(sizeof(payload) - 1, client.readBytes(rx, sizeof(payload) - 1));
  TEST_ASSERT_EQUAL_STRING(payload, rx);
  TEST_ASSERT_EQUAL(0, client.available());

  client.stop();
  close(peer_fd);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  UNITY_BEGIN();
  RUN_TEST(test_network_client_available_counts_loopback_bytes);
  UNITY_END();
}

void loop() {}
