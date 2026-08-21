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
#include <new>

// Cap sidecar body scan so a hostile/oversized response cannot grow heap.
static const size_t HTTPUPDATE_SIDECAR_MAX_SCAN = 512;

static bool httpUpdateIsHexDigit(int c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/**
 * Read the first hex token of exactly expectedLen characters from stream.
 * Longer or shorter hex runs are skipped. Uses only a stack buffer.
 */
static bool httpUpdateParseFirstHexToken(NetworkClient &stream, size_t expectedLen, int contentLength, char *out) {
  if (!out || (expectedLen != 32 && expectedLen != 64)) {
    return false;
  }

  const size_t maxScan = contentLength >= 0 && contentLength < (int)HTTPUPDATE_SIDECAR_MAX_SCAN ? contentLength : HTTPUPDATE_SIDECAR_MAX_SCAN;
  size_t scanned = 0;
  size_t tokenLength = 0;

  while (scanned < maxScan) {
    uint8_t byte;
    if (stream.readBytes(&byte, 1) != 1) {
      break;
    }
    int c = byte;
    scanned++;

    if (httpUpdateIsHexDigit(c)) {
      if (tokenLength < expectedLen) {
        out[tokenLength] = (char)c;
      }
      tokenLength++;
      continue;
    }

    if (tokenLength == expectedLen) {
      out[expectedLen] = '\0';
      return true;
    }
    tokenLength = 0;
  }

  if (tokenLength == expectedLen) {
    out[expectedLen] = '\0';
    return true;
  }
  return false;
}

static void httpUpdateSetChecksumUrl(String *&target, bool &configured, const String &url) {
  configured = !url.isEmpty();
  if (url.isEmpty()) {
    delete target;
    target = nullptr;
  } else {
    if (!target) {
      target = new (std::nothrow) String();
    }
    if (target) {
      *target = url;
    }
  }
}

void HTTPUpdate::setMD5sumUrl(const String &url) {
  httpUpdateSetChecksumUrl(_md5SumUrl, _md5SumUrlSet, url);
  _checksumSidecarFetch = (_md5SumUrlSet || _sha256SumUrlSet) ? httpUpdateFetchChecksumSidecars : nullptr;
}

void HTTPUpdate::setSHA256sumUrl(const String &url) {
  httpUpdateSetChecksumUrl(_sha256SumUrl, _sha256SumUrlSet, url);
  _checksumSidecarFetch = (_md5SumUrlSet || _sha256SumUrlSet) ? httpUpdateFetchChecksumSidecars : nullptr;
}

static bool httpUpdateFetchChecksumSidecar(
  NetworkClient &client, const String &url, size_t digestLen, String &outDigest, int timeout, followRedirects_t follow, const String &user,
  const String &password, const String &auth, const HTTPUpdateRequestCB &requestCB
) {
  outDigest = String();

  if (url.isEmpty() || (digestLen != 32 && digestLen != 64)) {
    return false;
  }

  HTTPClient http;
  if (!http.begin(client, url)) {
    return false;
  }

  // HTTP/1.0 disables keep-alive so the shared NetworkClient is free for the firmware GET.
  http.useHTTP10(true);
  http.setTimeout(timeout);
  http.setFollowRedirects(follow);
  http.setUserAgent("ESP32-http-Update");

  if (requestCB) {
    requestCB(&http);
  }

  if (!user.isEmpty() && !password.isEmpty()) {
    http.setAuthorization(user.c_str(), password.c_str());
  }
  if (!auth.isEmpty()) {
    http.setAuthorization(auth.c_str());
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength > (int)HTTPUPDATE_SIDECAR_MAX_SCAN) {
    http.end();
    return false;
  }

  NetworkClient *stream = http.getStreamPtr();
  if (!stream) {
    http.end();
    return false;
  }

  char token[65];
  bool ok = httpUpdateParseFirstHexToken(*stream, digestLen, contentLength, token);
  http.end();

  if (!ok) {
    return false;
  }

  outDigest = token;
  return true;
}

uint8_t httpUpdateFetchChecksumSidecars(
  NetworkClient *client, const String *md5Url, const String *sha256Url, uint8_t requested, String &md5, String &sha256, int timeout, followRedirects_t follow,
  const String &user, const String &password, const String &auth, const HTTPUpdateRequestCB &requestCB
) {
  uint8_t failures = 0;
  if ((requested & HTTPUPDATE_SIDECAR_MD5_FAILED)
      && (!client || !md5Url || !httpUpdateFetchChecksumSidecar(*client, *md5Url, 32, md5, timeout, follow, user, password, auth, requestCB))) {
    failures |= HTTPUPDATE_SIDECAR_MD5_FAILED;
  }
  if ((requested & HTTPUPDATE_SIDECAR_SHA256_FAILED)
      && (!client || !sha256Url || !httpUpdateFetchChecksumSidecar(*client, *sha256Url, 64, sha256, timeout, follow, user, password, auth, requestCB))) {
    failures |= HTTPUPDATE_SIDECAR_SHA256_FAILED;
  }
  return failures;
}
