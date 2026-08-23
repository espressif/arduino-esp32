/**
 *
 * @file HTTPUpdateChecksum.cpp
 * @brief Optional MD5/SHA-256 checksum sidecar fetch for HTTPUpdate.
 *
 * Linked only when a sketch calls setMD5sumUrl() / setSHA256sumUrl()
 * (referenced via a function pointer assigned in those setters).
 *
 * Copyright (c) 2026. All rights reserved.
 * This file is part of the ESP32 Http Updater.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "HTTPUpdate.h"

// Cap sidecar body scan so a hostile/oversized response cannot grow heap.
static const size_t HTTPUPDATE_SIDECAR_MAX_SCAN = 512;
static const uint8_t HTTPUPDATE_SIDECAR_MD5_FAILED = 0x01;
static const uint8_t HTTPUPDATE_SIDECAR_SHA256_FAILED = 0x02;

static uint8_t httpUpdateFetchChecksumSidecars(
  NetworkClient *client, const String &md5Url, const String &sha256Url, uint8_t requested, String &md5, String &sha256, int timeout, followRedirects_t follow
);

static bool httpUpdateIsHexDigit(int c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

class HTTPUpdateChecksumParser {
public:
  explicit HTTPUpdateChecksumParser(size_t expectedLen) : _expectedLen(expectedLen) {}

  bool push(uint8_t byte) {
    if (_scanned >= HTTPUPDATE_SIDECAR_MAX_SCAN) {
      // One byte beyond the scan window may only confirm that an exact-length
      // token ending at the boundary is properly delimited.
      if (_tokenLength == _expectedLen && !httpUpdateIsHexDigit(byte)) {
        _token[_expectedLen] = '\0';
        _found = true;
        return true;
      }
      _failed = true;
      return false;
    }

    _scanned++;
    if (httpUpdateIsHexDigit(byte)) {
      if (_tokenLength < _expectedLen) {
        _token[_tokenLength] = (char)byte;
      }
      _tokenLength++;
    } else {
      if (_tokenLength == _expectedLen) {
        _token[_expectedLen] = '\0';
        _found = true;
      } else {
        _tokenLength = 0;
      }
    }
    return !_failed;
  }

  bool finish() {
    if (!_failed && !_found && _tokenLength == _expectedLen) {
      _token[_expectedLen] = '\0';
      _found = true;
    }
    return _found && !_failed;
  }

  const char *token() const {
    return _token;
  }

  bool found() const {
    return _found;
  }

private:
  size_t _expectedLen;
  size_t _scanned = 0;
  size_t _tokenLength = 0;
  bool _found = false;
  bool _failed = false;
  char _token[65] = {};
};

enum HTTPUpdateReadResult {
  HTTPUPDATE_READ_OK,
  HTTPUPDATE_READ_EOF,
  HTTPUPDATE_READ_TIMEOUT,
};

static HTTPUpdateReadResult httpUpdateReadByte(NetworkClient &stream, uint32_t startedAt, uint32_t timeout, uint8_t &out) {
  while (!stream.available()) {
    if ((uint32_t)(millis() - startedAt) >= timeout) {
      return HTTPUPDATE_READ_TIMEOUT;
    }
    if (!stream.connected()) {
      return HTTPUPDATE_READ_EOF;
    }
    delay(1);
  }

  int value = stream.read();
  if (value < 0) {
    return stream.connected() ? HTTPUPDATE_READ_TIMEOUT : HTTPUPDATE_READ_EOF;
  }
  out = (uint8_t)value;
  return HTTPUPDATE_READ_OK;
}

static bool httpUpdateReadChunkSize(NetworkClient &stream, uint32_t startedAt, uint32_t timeout, size_t &chunkSize) {
  char line[32];
  size_t length = 0;
  while (true) {
    uint8_t byte;
    if (httpUpdateReadByte(stream, startedAt, timeout, byte) != HTTPUPDATE_READ_OK) {
      return false;
    }
    if (byte == '\n') {
      break;
    }
    if (byte != '\r') {
      if (length + 1 >= sizeof(line)) {
        return false;
      }
      line[length++] = (char)byte;
    }
  }
  line[length] = '\0';

  chunkSize = 0;
  bool hasDigit = false;
  for (size_t i = 0; i < length && line[i] != ';'; i++) {
    int digit;
    if (line[i] >= '0' && line[i] <= '9') {
      digit = line[i] - '0';
    } else if (line[i] >= 'a' && line[i] <= 'f') {
      digit = line[i] - 'a' + 10;
    } else if (line[i] >= 'A' && line[i] <= 'F') {
      digit = line[i] - 'A' + 10;
    } else {
      return false;
    }
    if (chunkSize > (SIZE_MAX - (size_t)digit) / 16) {
      return false;
    }
    chunkSize = chunkSize * 16 + (size_t)digit;
    hasDigit = true;
  }
  return hasDigit;
}

static bool httpUpdateParseIdentityBody(NetworkClient &stream, int contentLength, HTTPUpdateChecksumParser &parser, uint32_t startedAt, uint32_t timeout) {
  size_t remaining = contentLength >= 0 ? (size_t)contentLength : SIZE_MAX;
  while (remaining) {
    uint8_t byte;
    HTTPUpdateReadResult result = httpUpdateReadByte(stream, startedAt, timeout, byte);
    if (result != HTTPUPDATE_READ_OK) {
      return result == HTTPUPDATE_READ_EOF && contentLength < 0 && parser.finish();
    }
    if (!parser.push(byte)) {
      return false;
    }
    if (parser.found()) {
      return true;
    }
    if (contentLength >= 0) {
      remaining--;
    }
  }
  return parser.finish();
}

static bool httpUpdateParseChunkedBody(NetworkClient &stream, HTTPUpdateChecksumParser &parser, uint32_t startedAt, uint32_t timeout) {
  while (true) {
    size_t chunkSize;
    if (!httpUpdateReadChunkSize(stream, startedAt, timeout, chunkSize)) {
      return false;
    }
    if (chunkSize == 0) {
      return parser.finish();
    }

    for (size_t i = 0; i < chunkSize; i++) {
      uint8_t byte;
      if (httpUpdateReadByte(stream, startedAt, timeout, byte) != HTTPUPDATE_READ_OK || !parser.push(byte)) {
        return false;
      }
      if (parser.found()) {
        return true;
      }
    }

    uint8_t cr;
    uint8_t lf;
    if (httpUpdateReadByte(stream, startedAt, timeout, cr) != HTTPUPDATE_READ_OK || httpUpdateReadByte(stream, startedAt, timeout, lf) != HTTPUPDATE_READ_OK
        || cr != '\r' || lf != '\n') {
      return false;
    }
  }
}

void HTTPUpdate::setMD5sumUrl(const String &url) {
  _md5SumUrl = url;
  _checksumSidecarFetch = (!_md5SumUrl.isEmpty() || !_sha256SumUrl.isEmpty()) ? httpUpdateFetchChecksumSidecars : nullptr;
}

void HTTPUpdate::setSHA256sumUrl(const String &url) {
  _sha256SumUrl = url;
  _checksumSidecarFetch = (!_md5SumUrl.isEmpty() || !_sha256SumUrl.isEmpty()) ? httpUpdateFetchChecksumSidecars : nullptr;
}

static bool
  httpUpdateFetchChecksumSidecar(NetworkClient &client, const String &url, size_t digestLen, String &outDigest, int timeout, followRedirects_t follow) {
  outDigest = String();

  if (url.isEmpty() || (digestLen != 32 && digestLen != 64)) {
    return false;
  }

  // A new HTTPClient cannot determine which origin an existing socket belongs
  // to. Close it so the sidecar request always connects to its own URL.
  client.stop();

  HTTPClient http;
  if (!http.begin(client, url)) {
    return false;
  }

  // HTTP/1.0 disables keep-alive so the shared NetworkClient is free for the firmware GET.
  http.useHTTP10(true);
  http.setTimeout(timeout);
  http.setFollowRedirects(follow);
  http.setUserAgent("ESP32-http-Update");
  const char *headerKeys[] = {"Transfer-Encoding"};
  http.collectHeaders(headerKeys, 1);

  // Firmware authorization and request callbacks are intentionally not applied:
  // a sidecar URL or redirect target may belong to a different origin.
  uint32_t timeoutMs = timeout > 0 ? (uint32_t)timeout : 1;
  uint32_t startedAt = millis();
  int code = http.GET();
  if (code != HTTP_CODE_OK || (uint32_t)(millis() - startedAt) >= timeoutMs) {
    http.end();
    return false;
  }

  int contentLength = http.getSize();

  NetworkClient *stream = http.getStreamPtr();
  if (!stream) {
    http.end();
    return false;
  }

  HTTPUpdateChecksumParser parser(digestLen);
  bool chunked = http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
  bool ok = chunked ? httpUpdateParseChunkedBody(*stream, parser, startedAt, timeoutMs)
                    : httpUpdateParseIdentityBody(*stream, contentLength, parser, startedAt, timeoutMs);
  http.end();

  if (!ok) {
    return false;
  }

  outDigest = parser.token();
  return true;
}

static uint8_t httpUpdateFetchChecksumSidecars(
  NetworkClient *client, const String &md5Url, const String &sha256Url, uint8_t requested, String &md5, String &sha256, int timeout, followRedirects_t follow
) {
  uint8_t failures = 0;
  if ((requested & HTTPUPDATE_SIDECAR_MD5_FAILED) && (!client || !httpUpdateFetchChecksumSidecar(*client, md5Url, 32, md5, timeout, follow))) {
    failures |= HTTPUPDATE_SIDECAR_MD5_FAILED;
  }
  if ((requested & HTTPUPDATE_SIDECAR_SHA256_FAILED) && (!client || !httpUpdateFetchChecksumSidecar(*client, sha256Url, 64, sha256, timeout, follow))) {
    failures |= HTTPUPDATE_SIDECAR_SHA256_FAILED;
  }
  return failures;
}
