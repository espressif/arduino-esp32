/*
 * NetworkClient validation test
 *
 * Exercises the full NetworkClient API using localhost TCP (no WiFi).
 */

#include <Arduino.h>
#include <Network.h>
#include <NetworkClient.h>
#include <unity.h>

#include <esp_netif.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <unistd.h>

#include <cstring>

static constexpr uint32_t kWaitMs = 3000;

class TestStream : public Stream {
  const uint8_t *_data;
  size_t _len;
  size_t _pos;

public:
  TestStream(const uint8_t *data, size_t len) : _data(data), _len(len), _pos(0) {}

  int available() override {
    return static_cast<int>(_len - _pos);
  }

  int read() override {
    if (_pos >= _len) {
      return -1;
    }
    return _data[_pos++];
  }

  int peek() override {
    if (_pos >= _len) {
      return -1;
    }
    return _data[_pos];
  }

  size_t write(uint8_t) override {
    return 0;
  }
};

static int create_loopback_listener(uint16_t *bound_port) {
  int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, listen_fd, "socket() failed");

  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;

  TEST_ASSERT_EQUAL_MESSAGE(0, bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), "bind() failed");
  TEST_ASSERT_EQUAL_MESSAGE(0, listen(listen_fd, 1), "listen() failed");

  socklen_t addr_len = sizeof(addr);
  TEST_ASSERT_EQUAL_MESSAGE(0, getsockname(listen_fd, reinterpret_cast<sockaddr *>(&addr), &addr_len), "getsockname() failed");
  *bound_port = ntohs(addr.sin_port);
  return listen_fd;
}

static int accept_loopback_peer(int listen_fd) {
  int peer_fd = accept(listen_fd, nullptr, nullptr);
  TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, peer_fd, "accept() failed");
  return peer_fd;
}

static int create_loopback_client(int *peer_fd) {
  uint16_t port = 0;
  int listen_fd = create_loopback_listener(&port);

  int client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  TEST_ASSERT_GREATER_OR_EQUAL(0, client_fd);

  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);

  TEST_ASSERT_EQUAL(0, connect(client_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)));

  *peer_fd = accept_loopback_peer(listen_fd);
  close(listen_fd);
  TEST_ASSERT_GREATER_OR_EQUAL(0, *peer_fd);

  return client_fd;
}

static bool wait_available(NetworkClient &client, int min_bytes, uint32_t timeout_ms = kWaitMs) {
  uint32_t deadline = millis() + timeout_ms;
  while (client.available() < min_bytes && millis() < deadline) {
    delay(10);
  }
  return client.available() >= min_bytes;
}

static void peer_send(int peer_fd, const void *data, size_t len) {
  TEST_ASSERT_EQUAL(len, send(peer_fd, data, len, 0));
}

static void peer_recv_all(int peer_fd, void *data, size_t len) {
  size_t total = 0;
  auto *buf = static_cast<uint8_t *>(data);
  while (total < len) {
    int received = recv(peer_fd, buf + total, len - total, 0);
    TEST_ASSERT_GREATER_THAN(0, received);
    total += static_cast<size_t>(received);
  }
}

void setUp(void) {
  esp_err_t err = esp_netif_init();
  TEST_ASSERT_TRUE(err == ESP_OK || err == ESP_ERR_INVALID_STATE);
}

void tearDown(void) {}

void test_network_client_default_disconnected(void) {
  NetworkClient client;

  TEST_ASSERT_EQUAL(0, client.connected());
  TEST_ASSERT_FALSE(static_cast<bool>(client));
  TEST_ASSERT_EQUAL(-1, client.fd());
  TEST_ASSERT_EQUAL(0, client.available());
  TEST_ASSERT_EQUAL(-1, client.read());
  TEST_ASSERT_EQUAL(0, client.write(static_cast<uint8_t>('x')));
  TEST_ASSERT_FALSE(client.isSSE());

  client.setSSE(true);
  TEST_ASSERT_TRUE(client.isSSE());
  client.flush();
}

void test_network_client_fd_constructor_and_stop(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  TEST_ASSERT_TRUE(client.connected());
  TEST_ASSERT_TRUE(static_cast<bool>(client));
  TEST_ASSERT_EQUAL(client_fd, client.fd());

  client.stop();
  TEST_ASSERT_EQUAL(-1, client.fd());
  TEST_ASSERT_EQUAL(0, client.connected());
  TEST_ASSERT_EQUAL(0, client.write(static_cast<uint8_t>('x')));

  close(peer_fd);
}

void test_network_client_available_read_peek_clear(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  const char payload[] = "rx-count";
  peer_send(peer_fd, payload, sizeof(payload) - 1);

  TEST_ASSERT_TRUE(wait_available(client, sizeof(payload) - 1));
  TEST_ASSERT_GREATER_OR_EQUAL(sizeof(payload) - 1, client.available());

  TEST_ASSERT_EQUAL(payload[0], client.peek());
  TEST_ASSERT_GREATER_OR_EQUAL(sizeof(payload) - 1, client.available());

  TEST_ASSERT_EQUAL(payload[0], client.read());
  TEST_ASSERT_GREATER_OR_EQUAL(sizeof(payload) - 2, client.available());

  uint8_t tail[sizeof(payload) - 2] = {};
  TEST_ASSERT_EQUAL(sizeof(payload) - 2, client.read(tail, sizeof(tail)));
  TEST_ASSERT_EQUAL_MEMORY(payload + 1, tail, sizeof(tail));
  TEST_ASSERT_EQUAL(0, client.available());

  peer_send(peer_fd, "more", 4);
  TEST_ASSERT_TRUE(wait_available(client, 4));
  client.clear();
  TEST_ASSERT_EQUAL(0, client.available());

  client.stop();
  close(peer_fd);
}

void test_network_client_read_bytes_large_payload(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  static constexpr size_t kLen = 2500;
  uint8_t tx[kLen];
  for (size_t i = 0; i < kLen; i++) {
    tx[i] = static_cast<uint8_t>(i & 0xFF);
  }
  peer_send(peer_fd, tx, kLen);

  TEST_ASSERT_TRUE(wait_available(client, static_cast<int>(kLen), kWaitMs + 2000));

  uint8_t rx[kLen] = {};
  TEST_ASSERT_EQUAL(kLen, client.readBytes(rx, kLen));
  TEST_ASSERT_EQUAL_MEMORY(tx, rx, kLen);
  TEST_ASSERT_EQUAL(0, client.available());

  client.stop();
  close(peer_fd);
}

void test_network_client_write_variants(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  const char chunk[] = "writ";
  TEST_ASSERT_EQUAL(1, client.write(static_cast<uint8_t>(chunk[0])));
  TEST_ASSERT_EQUAL(sizeof(chunk) - 2, client.write(reinterpret_cast<const uint8_t *>(chunk + 1), sizeof(chunk) - 2));

  const char prog[] PROGMEM = "!";
  TEST_ASSERT_EQUAL(1, client.write_P(prog, 1));

  char rx[16] = {};
  peer_recv_all(peer_fd, rx, 5);
  TEST_ASSERT_EQUAL_STRING("writ!", rx);

  client.stop();
  close(peer_fd);
}

void test_network_client_write_stream(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  const uint8_t stream_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  TestStream full(stream_data, sizeof(stream_data));
  TestStream partial(stream_data, sizeof(stream_data));

  TEST_ASSERT_EQUAL(sizeof(stream_data), client.write(full));
  TEST_ASSERT_EQUAL(3, client.write(partial, 3));

  uint8_t rx[8] = {};
  peer_recv_all(peer_fd, rx, sizeof(rx));
  TEST_ASSERT_EQUAL_MEMORY(stream_data, rx, sizeof(stream_data));
  TEST_ASSERT_EQUAL_MEMORY(stream_data, rx + sizeof(stream_data), 3);

  client.stop();
  close(peer_fd);
}

void test_network_client_connect_loopback_ip(void) {
  uint16_t port = 0;
  int listen_fd = create_loopback_listener(&port);

  NetworkClient client;
  client.setConnectionTimeout(3000);
  TEST_ASSERT_TRUE(client.connect(IPAddress(127, 0, 0, 1), port));
  TEST_ASSERT_TRUE(client.connected());

  int peer_fd = accept_loopback_peer(listen_fd);
  close(listen_fd);

  TEST_ASSERT_EQUAL(3, client.write(reinterpret_cast<const uint8_t *>("ok\n"), 3));
  char rx[4] = {};
  peer_send(peer_fd, "ack", 3);
  TEST_ASSERT_TRUE(wait_available(client, 3));
  TEST_ASSERT_EQUAL(3, client.readBytes(rx, 3));
  TEST_ASSERT_EQUAL_STRING("ack", rx);

  client.stop();
  close(peer_fd);
}

void test_network_client_connect_loopback_host(void) {
  uint16_t port = 0;
  int listen_fd = create_loopback_listener(&port);

  NetworkClient client;
  TEST_ASSERT_TRUE(client.connect("127.0.0.1", port, 3000));
  TEST_ASSERT_TRUE(client.connected());

  int peer_fd = accept_loopback_peer(listen_fd);
  close(listen_fd);

  client.stop();
  close(peer_fd);
}

void test_network_client_connect_timeout(void) {
  NetworkClient client;
  TEST_ASSERT_FALSE(client.connect(IPAddress(127, 0, 0, 1), 59999, 200));
  TEST_ASSERT_FALSE(client.connected());
  TEST_ASSERT_FALSE(client.connect("127.0.0.1", 59998, 200));
}

void test_network_client_socket_options(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  client.setConnectionTimeout(5000);

  TEST_ASSERT_GREATER_OR_EQUAL(0, client.setNoDelay(true));
  TEST_ASSERT_TRUE(client.getNoDelay());
  TEST_ASSERT_GREATER_OR_EQUAL(0, client.setNoDelay(false));
  TEST_ASSERT_FALSE(client.getNoDelay());

  int flag = 1;
  TEST_ASSERT_GREATER_OR_EQUAL(0, client.setOption(TCP_NODELAY, &flag));
  int read_flag = 0;
  TEST_ASSERT_GREATER_OR_EQUAL(0, client.getOption(TCP_NODELAY, &read_flag));
  TEST_ASSERT_EQUAL(1, read_flag);

  int keepalive = 1;
  TEST_ASSERT_GREATER_OR_EQUAL(0, client.setSocketOption(SO_KEEPALIVE, reinterpret_cast<char *>(&keepalive), sizeof(keepalive)));

  client.stop();
  close(peer_fd);
}

void test_network_client_endpoints(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  IPAddress loopback(127, 0, 0, 1);
  TEST_ASSERT_TRUE(client.remoteIP() == loopback);
  TEST_ASSERT_TRUE(client.remoteIP(client.fd()) == loopback);
  TEST_ASSERT_TRUE(client.localIP() == loopback);
  TEST_ASSERT_TRUE(client.localIP(client.fd()) == loopback);
  TEST_ASSERT_NOT_EQUAL(0, client.remotePort());
  TEST_ASSERT_NOT_EQUAL(0, client.remotePort(client.fd()));
  TEST_ASSERT_NOT_EQUAL(0, client.localPort());
  TEST_ASSERT_NOT_EQUAL(0, client.localPort(client.fd()));

  client.stop();
  close(peer_fd);
}

void test_network_client_operator_compare(void) {
  int peer_a = -1;
  int peer_b = -1;
  int fd_a = create_loopback_client(&peer_a);
  int fd_b = create_loopback_client(&peer_b);

  NetworkClient client_a(fd_a);
  NetworkClient client_b(fd_b);

  TEST_ASSERT_TRUE(client_a != client_b);

  client_a.stop();
  client_b.stop();
  close(peer_a);
  close(peer_b);
}

void test_network_client_peer_shutdown(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  TEST_ASSERT_TRUE(client.connected());
  shutdown(peer_fd, SHUT_RDWR);
  close(peer_fd);
  peer_fd = -1;

  uint32_t deadline = millis() + kWaitMs;
  while (client.connected() && millis() < deadline) {
    delay(10);
  }
  TEST_ASSERT_FALSE(client.connected());

  client.stop();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  UNITY_BEGIN();
  RUN_TEST(test_network_client_default_disconnected);
  RUN_TEST(test_network_client_fd_constructor_and_stop);
  RUN_TEST(test_network_client_available_read_peek_clear);
  RUN_TEST(test_network_client_read_bytes_large_payload);
  RUN_TEST(test_network_client_write_variants);
  RUN_TEST(test_network_client_write_stream);
  RUN_TEST(test_network_client_connect_loopback_ip);
  RUN_TEST(test_network_client_connect_loopback_host);
  RUN_TEST(test_network_client_connect_timeout);
  RUN_TEST(test_network_client_socket_options);
  RUN_TEST(test_network_client_endpoints);
  RUN_TEST(test_network_client_operator_compare);
  RUN_TEST(test_network_client_peer_shutdown);
  UNITY_END();
}

void loop() {}
