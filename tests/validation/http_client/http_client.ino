/*
 * HTTPClient stream upload validation.
 * Uses a fake transport to exercise short reads and write retries without a network.
 */

#include <Arduino.h>
#include <HTTPClient.h>
#include <unity.h>

class ShortReadStream : public Stream {
  const char *_data;
  size_t _length;
  size_t _position = 0;
  size_t _readLimit;

public:
  ShortReadStream(const char *data, size_t readLimit) : _data(data), _length(strlen(data)), _readLimit(readLimit) {}

  using Stream::readBytes;

  int available() override {
    return _length - _position;
  }

  int read() override {
    return available() ? (uint8_t)_data[_position++] : -1;
  }

  int peek() override {
    return available() ? (uint8_t)_data[_position] : -1;
  }

  size_t readBytes(uint8_t *buffer, size_t length) override {
    // Advertise the remaining data, but return only part of each requested read.
    if (length > _readLimit) {
      length = _readLimit;
    }
    if (length > _length - _position) {
      length = _length - _position;
    }
    memcpy(buffer, _data + _position, length);
    _position += length;
    return length;
  }

  size_t write(uint8_t) override {
    return 0;
  }
};

class UploadClient : public NetworkClient {
  static constexpr const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
  size_t _responsePosition = 0;
  size_t _writeLimit;

public:
  String requestHeaders;
  String requestBody;

  UploadClient(size_t writeLimit = 0) : _writeLimit(writeLimit) {}

  using NetworkClient::connect;
  using NetworkClient::write;

  int connect(const char *, uint16_t, int32_t) override {
    _connected = true;
    return 1;
  }

  uint8_t connected() override {
    return _connected;
  }

  void stop() override {
    _connected = false;
  }

  int available() override {
    return _connected && requestHeaders.length() ? strlen(response) - _responsePosition : 0;
  }

  int peek() override {
    return available() ? response[_responsePosition] : -1;
  }

  int read() override {
    return available() ? response[_responsePosition++] : -1;
  }

  int read(uint8_t *buffer, size_t size) override {
    size_t count = 0;
    while (count < size && available()) {
      buffer[count++] = read();
    }
    return count;
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    // HTTPClient sends the headers in one write; limit only payload writes.
    if (!requestHeaders.length()) {
      requestHeaders.concat((const char *)buffer, size);
      return size;
    }
    if (_writeLimit && size > _writeLimit) {
      size = _writeLimit;
    }
    requestBody.concat((const char *)buffer, size);
    return size;
  }
};

void setUp(void) {}
void tearDown(void) {}

static void check_stream_upload(size_t readLimit, size_t writeLimit, size_t uploadSize = 8) {
  const char *data = "abcdefghij";
  ShortReadStream stream(data, readLimit);
  UploadClient client(writeLimit);
  HTTPClient http;

  TEST_ASSERT_TRUE(http.begin(client, "http://upload.invalid/"));
  int code = http.sendRequest("POST", &stream, uploadSize);
  http.end();

  TEST_ASSERT_EQUAL(HTTP_CODE_OK, code);
  TEST_ASSERT_EQUAL(uploadSize, client.requestBody.length());
  TEST_ASSERT_EQUAL_MEMORY(data, client.requestBody.c_str(), uploadSize);
  TEST_ASSERT_EQUAL(strlen(data) - uploadSize, stream.available());
  String contentLength = String("Content-Length: ") + uploadSize + "\r\n";
  TEST_ASSERT_TRUE(client.requestHeaders.indexOf(contentLength) >= 0);
}

void test_http_stream_full_read(void) {
  check_stream_upload(8, 0);
}

void test_http_stream_short_read(void) {
  check_stream_upload(4, 0);
}

void test_http_stream_short_read_short_write(void) {
  check_stream_upload(4, 2);
}

void test_http_stream_full_read_short_write(void) {
  check_stream_upload(8, 4);
}

void test_http_stream_content_length_limit(void) {
  check_stream_upload(3, 0, 5);
}

void test_http_stream_failed_retry(void) {
  ShortReadStream stream("abcdefgh", 4);
  UploadClient client(1);
  HTTPClient http;

  TEST_ASSERT_TRUE(http.begin(client, "http://upload.invalid/"));
  int code = http.sendRequest("POST", &stream, 8);
  http.end();

  TEST_ASSERT_EQUAL(HTTPC_ERROR_SEND_PAYLOAD_FAILED, code);
  TEST_ASSERT_EQUAL_STRING("ab", client.requestBody.c_str());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();
  RUN_TEST(test_http_stream_full_read);
  RUN_TEST(test_http_stream_short_read);
  RUN_TEST(test_http_stream_short_read_short_write);
  RUN_TEST(test_http_stream_full_read_short_write);
  RUN_TEST(test_http_stream_content_length_limit);
  RUN_TEST(test_http_stream_failed_retry);
  UNITY_END();
}

void loop() {}
