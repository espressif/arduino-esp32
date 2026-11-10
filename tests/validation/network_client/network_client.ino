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

class ShortWriteClient : public NetworkClient {
  size_t _maxAccepted;
  size_t _writeCalls = 0;

public:
  explicit ShortWriteClient(size_t maxAccepted) : _maxAccepted(maxAccepted) {}

  using NetworkClient::write;

  size_t write(const uint8_t *, size_t size) override {
    ++_writeCalls;
    return size < _maxAccepted ? size : _maxAccepted;
  }

  size_t writeCalls() const {
    return _writeCalls;
  }
};

class NetworkServerTest : public NetworkServer {
public:
  NetworkServerTest(const IPAddress &addr, uint16_t port) : NetworkServer(addr, port) {}

  void injectAcceptedSocket(int fd) {
    _accepted_sockfd = fd;
  }
};

class StalledStream : public Stream {
public:
  int available() override {
    return 1;
  }

  int read() override {
    return -1;
  }

  int peek() override {
    return -1;
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

void test_ip_address_valid_ipv4_string(void) {
  IPAddress address("192.0.2.1");

  TEST_ASSERT_EQUAL(IPv4, address.type());
  TEST_ASSERT_EQUAL_UINT8(192, address[0]);
  TEST_ASSERT_EQUAL_UINT8(0, address[1]);
  TEST_ASSERT_EQUAL_UINT8(2, address[2]);
  TEST_ASSERT_EQUAL_UINT8(1, address[3]);
}

void test_ip_address_unscoped_ipv6_string(void) {
  IPAddress address("2001:db8::1");
  const IPAddress expected(0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);

  TEST_ASSERT_EQUAL(IPv6, address.type());
  TEST_ASSERT_TRUE(address == expected);
  TEST_ASSERT_EQUAL_UINT8(0, address.zone());
}

void test_ip_address_scoped_ipv6_string(void) {
  IPAddress address("fe80::1%2");
  const IPAddress expected(0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);

  TEST_ASSERT_EQUAL(IPv6, address.type());
  TEST_ASSERT_TRUE(address == expected);
  TEST_ASSERT_EQUAL_UINT8(3, address.zone());
}

void test_ip_address_reparse_clears_ipv6_zone(void) {
  IPAddress address("fe80::1%2");

  TEST_ASSERT_TRUE(address.fromString("fe80::2"));
  TEST_ASSERT_EQUAL(IPv6, address.type());
  TEST_ASSERT_EQUAL_UINT8(0, address.zone());
  TEST_ASSERT_EQUAL_UINT8(2, address[15]);
}

void test_ip_address_invalid_string_constructor_is_initialized(void) {
  IPAddress invalid_ipv4("192.0.2.999");
  IPAddress invalid_ipv6("2001:db8::zzzz");

  TEST_ASSERT_EQUAL(IPv4, invalid_ipv4.type());
  TEST_ASSERT_EQUAL_UINT8(0, invalid_ipv4.zone());
  TEST_ASSERT_EQUAL(IPv4, invalid_ipv6.type());
  TEST_ASSERT_EQUAL_UINT8(0, invalid_ipv6.zone());
}

void test_ip_address_null_string_constructor_is_initialized(void) {
  IPAddress address(static_cast<const char *>(nullptr));

  TEST_ASSERT_EQUAL(IPv4, address.type());
  TEST_ASSERT_TRUE(address == IPAddress(0, 0, 0, 0));
}

void test_ip_address_rejects_invalid_input(void) {
  IPAddress address("fe80::1%2");

  TEST_ASSERT_FALSE(address.fromString("192.0.2.999"));
  TEST_ASSERT_FALSE(address.fromString("2001:db8::zzzz"));
  TEST_ASSERT_FALSE(address.fromString(static_cast<const char *>(nullptr)));

  TEST_ASSERT_TRUE(address.fromString("fe80::2"));
  const IPAddress expected(0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2);
  TEST_ASSERT_EQUAL(IPv6, address.type());
  TEST_ASSERT_TRUE(address == expected);
  TEST_ASSERT_EQUAL_UINT8(0, address.zone());
}

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

void test_network_client_write_stream_stops_on_short_write(void) {
  uint8_t data[2721] = {};
  TestStream unbounded(data, sizeof(data));
  ShortWriteClient unboundedClient(5);

  TEST_ASSERT_EQUAL(5, unboundedClient.write(unbounded));
  TEST_ASSERT_EQUAL(1, unboundedClient.writeCalls());
  TEST_ASSERT_EQUAL(sizeof(data) - 1360, unbounded.available());

  TestStream bounded(data, sizeof(data));
  ShortWriteClient boundedClient(5);

  TEST_ASSERT_EQUAL(5, boundedClient.write(bounded, sizeof(data)));
  TEST_ASSERT_EQUAL(1, boundedClient.writeCalls());
  TEST_ASSERT_EQUAL(sizeof(data) - 1360, bounded.available());
}

void test_network_client_write_stream_stops_on_zero_write(void) {
  uint8_t data[2721] = {};
  TestStream unbounded(data, sizeof(data));
  ShortWriteClient unboundedClient(0);

  TEST_ASSERT_EQUAL(0, unboundedClient.write(unbounded));
  TEST_ASSERT_EQUAL(1, unboundedClient.writeCalls());
  TEST_ASSERT_EQUAL(sizeof(data) - 1360, unbounded.available());

  TestStream bounded(data, sizeof(data));
  ShortWriteClient boundedClient(0);

  TEST_ASSERT_EQUAL(0, boundedClient.write(bounded, sizeof(data)));
  TEST_ASSERT_EQUAL(1, boundedClient.writeCalls());
  TEST_ASSERT_EQUAL(sizeof(data) - 1360, bounded.available());
}

void test_network_client_write_stream_stops_when_source_stalls(void) {
  StalledStream source;
  source.setTimeout(10);
  ShortWriteClient client(1);

  TEST_ASSERT_EQUAL(0, client.write(source));
  TEST_ASSERT_EQUAL(0, client.writeCalls());
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

static bool wait_connected(NetworkClient &client, bool connected, uint32_t timeout_ms = kWaitMs) {
  uint32_t deadline = millis() + timeout_ms;
  while (client.connected() != connected && millis() < deadline) {
    delay(10);
  }
  return client.connected() == connected;
}

// An orderly close (FIN) must eventually make connected() report false once the
// buffered payload has been drained. This is the normal case for a peer that
// calls close() or shutdown(SHUT_WR), not an abortive reset.
void test_network_client_peer_fin_reports_disconnected(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  TEST_ASSERT_TRUE(client.connected());

  peer_send(peer_fd, "bye", 3);
  TEST_ASSERT_EQUAL(0, shutdown(peer_fd, SHUT_WR));
  TEST_ASSERT_TRUE(wait_available(client, 3));
  delay(20);  // let lwIP process the FIN segment that follows the payload

  // A FIN does not discard unread data: connected() stays true while the
  // payload is still available.
  TEST_ASSERT_TRUE(client.connected());

  char rx[4] = {};
  TEST_ASSERT_EQUAL(3, client.readBytes(rx, 3));
  TEST_ASSERT_EQUAL_STRING("bye", rx);
  TEST_ASSERT_EQUAL(0, client.available());

  // The very first probe after the buffered payload is drained must already
  // report the FIN as disconnected. lwIP returns ERR_CLSD (recv() == 0) for
  // that first probe, and a later probe happens to return ENOTCONN, which
  // would hide the bug if the test only waited for a state change.
  TEST_ASSERT_FALSE_MESSAGE(client.connected(), "connected() reported true on the first probe after an orderly FIN");
  TEST_ASSERT_TRUE_MESSAGE(wait_connected(client, false), "connected() stayed true after an orderly FIN");

  client.stop();
  close(peer_fd);
}

void test_network_client_peer_close_reports_disconnected(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  peer_send(peer_fd, "bye", 3);
  close(peer_fd);
  peer_fd = -1;

  char rx[4] = {};
  TEST_ASSERT_TRUE(wait_available(client, 3));
  delay(20);  // let lwIP process the FIN segment that follows the payload
  TEST_ASSERT_EQUAL(3, client.readBytes(rx, 3));
  TEST_ASSERT_EQUAL_STRING("bye", rx);

  TEST_ASSERT_FALSE_MESSAGE(client.connected(), "connected() reported true on the first probe after the peer closed");
  TEST_ASSERT_TRUE_MESSAGE(wait_connected(client, false), "connected() stayed true after the peer closed");

  client.stop();
}

// Reading a half-closed socket must report EOF, not an error: the peer may only
// have shut down its write side and still be waiting for a reply.
void test_network_client_peer_half_close_allows_reply(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  peer_send(peer_fd, "req", 3);
  TEST_ASSERT_EQUAL(0, shutdown(peer_fd, SHUT_WR));
  TEST_ASSERT_TRUE(wait_available(client, 3));

  char req[4] = {};
  TEST_ASSERT_EQUAL(3, client.readBytes(req, 3));
  TEST_ASSERT_EQUAL_STRING("req", req);

  uint8_t byte = 0;
  TEST_ASSERT_EQUAL(0, client.read(&byte, 1));
  TEST_ASSERT_EQUAL(-1, client.read());
  TEST_ASSERT_GREATER_OR_EQUAL(0, client.fd());

  TEST_ASSERT_EQUAL(2, client.write(reinterpret_cast<const uint8_t *>("ok"), 2));
  char reply[3] = {};
  peer_recv_all(peer_fd, reply, 2);
  TEST_ASSERT_EQUAL_STRING("ok", reply);

  client.stop();
  close(peer_fd);
}

// connected() reports that no more inbound data can arrive, but the socket is
// still writable after a half-close. Keep that reply path working even when
// the sketch probed connected() before writing.
void test_network_client_peer_half_close_allows_reply_after_probe(void) {
  int peer_fd = -1;
  int client_fd = create_loopback_client(&peer_fd);
  NetworkClient client(client_fd);

  peer_send(peer_fd, "req", 3);
  TEST_ASSERT_EQUAL(0, shutdown(peer_fd, SHUT_WR));
  TEST_ASSERT_TRUE(wait_available(client, 3));
  delay(20);  // let lwIP process the FIN segment that follows the payload

  char req[4] = {};
  TEST_ASSERT_EQUAL(3, client.readBytes(req, 3));
  TEST_ASSERT_EQUAL_STRING("req", req);

  TEST_ASSERT_FALSE(client.connected());
  TEST_ASSERT_FALSE(client.connected());
  TEST_ASSERT_GREATER_OR_EQUAL(0, client.fd());

  TEST_ASSERT_EQUAL(2, client.write(reinterpret_cast<const uint8_t *>("ok"), 2));
  char reply[3] = {};
  peer_recv_all(peer_fd, reply, 2);
  TEST_ASSERT_EQUAL_STRING("ok", reply);

  client.stop();
  close(peer_fd);
}

void test_network_server_accept_roundtrip(void) {
  static constexpr uint16_t kPort = 39876;

  NetworkServer server(IPAddress(127, 0, 0, 1), kPort);
  server.begin();
  TEST_ASSERT_TRUE(server);

  NetworkClient client;
  TEST_ASSERT_TRUE(client.connect(IPAddress(127, 0, 0, 1), kPort));

  uint32_t deadline = millis() + kWaitMs;
  while (!server.hasClient() && millis() < deadline) {
    delay(10);
  }
  TEST_ASSERT_TRUE(server.hasClient());

  NetworkClient accepted = server.accept();
  TEST_ASSERT_TRUE(accepted.connected());
  TEST_ASSERT_GREATER_OR_EQUAL(0, accepted.fd());

  TEST_ASSERT_EQUAL(4, client.write(reinterpret_cast<const uint8_t *>("ping"), 4));
  char request[5] = {};
  TEST_ASSERT_TRUE(wait_available(accepted, 4));
  TEST_ASSERT_EQUAL(4, accepted.readBytes(request, 4));
  TEST_ASSERT_EQUAL_STRING("ping", request);

  TEST_ASSERT_EQUAL(4, accepted.write(reinterpret_cast<const uint8_t *>("pong"), 4));
  char response[5] = {};
  TEST_ASSERT_TRUE(wait_available(client, 4));
  TEST_ASSERT_EQUAL(4, client.readBytes(response, 4));
  TEST_ASSERT_EQUAL_STRING("pong", response);

  accepted.stop();
  client.stop();
  server.end();
}

void test_network_server_accept_closes_socket_on_option_failure(void) {
  NetworkServerTest server(IPAddress(127, 0, 0, 1), 0);
  server.begin();
  TEST_ASSERT_TRUE(server);

  // A UDP socket is a valid descriptor but TCP_NODELAY is TCP-only, so
  // accept() takes the same setsockopt() failure path as a real client whose
  // socket options could not be configured.
  int non_tcp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  TEST_ASSERT_GREATER_OR_EQUAL(0, non_tcp_fd);

  server.injectAcceptedSocket(non_tcp_fd);

  NetworkClient accepted = server.accept();
  TEST_ASSERT_FALSE(accepted);
  TEST_ASSERT_EQUAL(-1, accepted.fd());

  // The descriptor must be closed, not leaked.
  int type = 0;
  socklen_t type_len = sizeof(type);
  TEST_ASSERT_EQUAL(-1, getsockopt(non_tcp_fd, SOL_SOCKET, SO_TYPE, &type, &type_len));
  TEST_ASSERT_EQUAL(EBADF, errno);

  server.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  UNITY_BEGIN();
  RUN_TEST(test_ip_address_valid_ipv4_string);
  RUN_TEST(test_ip_address_unscoped_ipv6_string);
  RUN_TEST(test_ip_address_scoped_ipv6_string);
  RUN_TEST(test_ip_address_reparse_clears_ipv6_zone);
  RUN_TEST(test_ip_address_invalid_string_constructor_is_initialized);
  RUN_TEST(test_ip_address_null_string_constructor_is_initialized);
  RUN_TEST(test_ip_address_rejects_invalid_input);
  RUN_TEST(test_network_client_default_disconnected);
  RUN_TEST(test_network_client_fd_constructor_and_stop);
  RUN_TEST(test_network_client_available_read_peek_clear);
  RUN_TEST(test_network_client_read_bytes_large_payload);
  RUN_TEST(test_network_client_write_variants);
  RUN_TEST(test_network_client_write_stream);
  RUN_TEST(test_network_client_write_stream_stops_on_short_write);
  RUN_TEST(test_network_client_write_stream_stops_on_zero_write);
  RUN_TEST(test_network_client_write_stream_stops_when_source_stalls);
  RUN_TEST(test_network_client_connect_loopback_ip);
  RUN_TEST(test_network_client_connect_loopback_host);
  RUN_TEST(test_network_client_connect_timeout);
  RUN_TEST(test_network_client_socket_options);
  RUN_TEST(test_network_client_endpoints);
  RUN_TEST(test_network_client_operator_compare);
  RUN_TEST(test_network_client_peer_shutdown);
  RUN_TEST(test_network_client_peer_fin_reports_disconnected);
  RUN_TEST(test_network_client_peer_close_reports_disconnected);
  RUN_TEST(test_network_client_peer_half_close_allows_reply);
  RUN_TEST(test_network_client_peer_half_close_allows_reply_after_probe);
  RUN_TEST(test_network_server_accept_roundtrip);
  RUN_TEST(test_network_server_accept_closes_socket_on_option_failure);
  UNITY_END();
}

void loop() {}
