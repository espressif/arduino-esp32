/*
  Parsing.cpp - HTTP request parsing.

  Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  Modified 8 May 2015 by Hristo Gochkov (proper post and file upload handling)
*/

#include <Arduino.h>
#include <esp32-hal-log.h>
#include <new>
#include "NetworkServer.h"
#include "NetworkClient.h"
#include "WebServer.h"
#include "detail/mimetable.h"

#ifndef WEBSERVER_MAX_POST_ARGS
#define WEBSERVER_MAX_POST_ARGS 32
#endif

#define __STR(a) #a
#define _STR(a)  __STR(a)
static const char *_http_method_str[] = {
#define XX(num, name, string) _STR(name),
  HTTP_METHOD_MAP(XX)
#undef XX
};

static const char Content_Type[] PROGMEM = "Content-Type";
static const char filename[] PROGMEM = "filename";

static char *readBytesWithTimeout(NetworkClient &client, size_t maxLength, size_t &dataLength, int timeout_ms) {
  char *buf = nullptr;
  dataLength = 0;
  while (dataLength < maxLength) {
    int tries = timeout_ms;
    size_t newLength;
    while (!(newLength = client.available()) && tries--) {
      delay(1);
    }
    if (!newLength) {
      break;
    }
    if (!buf) {
      buf = (char *)malloc(newLength + 1);
      if (!buf) {
        return nullptr;
      }
    } else {
      char *newBuf = (char *)realloc(buf, dataLength + newLength + 1);
      if (!newBuf) {
        free(buf);
        return nullptr;
      }
      buf = newBuf;
    }
    client.readBytes(buf + dataLength, newLength);
    dataLength += newLength;
    buf[dataLength] = '\0';
  }
  return buf;
}

// Answer a request that is being dropped part-way through. The peer is usually
// still sending, and closing a connection with unread input resets it, which
// discards the response we just queued. Drain what already arrived first so the
// status actually reaches the client. The drain is bounded: a peer that keeps
// streaming is cut off.
static void sendErrorResponse(NetworkClient &client, const char *status) {
  client.printf("HTTP/1.1 %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", status);

  const size_t maxDrain = 2 * WEBSERVER_MAX_LINE_LEN;
  uint8_t discard[64];
  size_t drained = 0;
  unsigned long lastData = millis();
  while (drained < maxDrain && millis() - lastData < 50) {
    int available = client.available();
    if (available <= 0) {
      yield();
      continue;
    }
    size_t wanted = (size_t)available < sizeof(discard) ? (size_t)available : sizeof(discard);
    int read = client.read(discard, wanted);
    if (read <= 0) {
      break;
    }
    drained += (size_t)read;
    lastData = millis();
  }
}

enum class LineStatus {
  Ok,
  TooLong,   // reached maxLength before any line terminator
  TimedOut,  // took longer than the per-line or the caller's phase budget
};

// Read one CRLF-terminated protocol line, consuming the terminator.
//
// Stream::readStringUntil() bounds neither the length of the line nor the total
// time it may take: it only requires that another byte arrive within the stream
// timeout. A peer that never sends a terminator therefore grows the String until
// the heap is gone, and a peer that sends one byte just inside every timeout
// keeps the read alive for as long as it likes. Both keep handleClient() from
// returning.
//
// This bounds all three: the length of the line, the time one line may take, and
// (through phaseBudget) the time the caller's whole parse phase may take. The
// phase budget is what catches a peer that keeps sending complete but tiny lines,
// since each of those restarts the per-line budget.
static LineStatus readLineWithLimit(NetworkClient &client, String &line, size_t maxLength, unsigned long phaseStart = 0, unsigned long phaseBudget = 0) {
  line = "";
  const unsigned long idleTimeout = client.getTimeout();
  const unsigned long lineStart = millis();
  unsigned long lastActivity = lineStart;

  while (millis() - lastActivity < idleTimeout) {
#if WEBSERVER_MAX_LINE_WAIT > 0
    if (millis() - lineStart >= WEBSERVER_MAX_LINE_WAIT) {
      log_e("Protocol line still incomplete after %u ms", (unsigned)WEBSERVER_MAX_LINE_WAIT);
      return LineStatus::TimedOut;
    }
#endif
    if (phaseBudget && millis() - phaseStart >= phaseBudget) {
      log_e("Request headers still incomplete after %u ms", (unsigned)phaseBudget);
      return LineStatus::TimedOut;
    }

    int c = client.read();
    if (c < 0) {
      yield();
      continue;
    }
    lastActivity = millis();
    if (c == '\r') {
      // Consume the paired LF, waiting for it as readStringUntil('\n') would.
      while (millis() - lastActivity < idleTimeout) {
        int next = client.peek();
        if (next < 0) {
          yield();
          continue;
        }
        if (next == '\n') {
          client.read();
        }
        break;
      }
      return LineStatus::Ok;
    }
    if (line.length() >= maxLength) {
      log_e("Protocol line longer than %u bytes", (unsigned)maxLength);
      return LineStatus::TooLong;
    }
    line += (char)c;
  }
  // The peer stopped sending part-way through a line, so there is no complete
  // request to act on.
  return LineStatus::TimedOut;
}

bool WebServer::_parseRequest(NetworkClient &client) {
  // Upload, raw and multipart field state all belong to a single request. Drop
  // anything left over from an earlier request on this connection so it cannot
  // poison arg()/hasArg() lookups or be reported as an active upload.
  if (_postArgs) {
    delete[] _postArgs;
    _postArgs = nullptr;
    _postArgsLen = 0;
  }
  _currentUpload.reset();
  _currentRaw.reset();

  // The request line and the headers share one deadline. Only the body is
  // allowed to take longer, so a slow upload is not affected.
  const unsigned long headerPhaseStart = millis();

  // Read the first line of HTTP request
  String req;
  switch (readLineWithLimit(client, req, WEBSERVER_MAX_LINE_LEN, headerPhaseStart, WEBSERVER_MAX_HEADER_WAIT)) {
    case LineStatus::TooLong:  sendErrorResponse(client, "414 URI Too Long"); return false;
    case LineStatus::TimedOut: sendErrorResponse(client, "408 Request Timeout"); return false;
    case LineStatus::Ok:       break;
  }
  //reset header value
  if (_collectAllHeaders) {
    // clear previous headers
    collectAllHeaders();
  } else {
    // clear previous headers
    for (RequestArgument *header = _currentHeaders; header; header = header->next) {
      header->value = String();
    }
  }

  // First line of HTTP request looks like "GET /path HTTP/1.1"
  // Retrieve the "/path" part by finding the spaces
  int addr_start = req.indexOf(' ');
  int addr_end = req.indexOf(' ', addr_start + 1);
  if (addr_start == -1 || addr_end == -1) {
    log_e("Invalid request: %s", req.c_str());
    return false;
  }

  String methodStr = req.substring(0, addr_start);
  String url = req.substring(addr_start + 1, addr_end);
  String versionEnd = req.substring(addr_end + 8);
  _currentVersion = atoi(versionEnd.c_str());
  String searchStr = "";
  int hasSearch = url.indexOf('?');
  if (hasSearch != -1) {
    searchStr = url.substring(hasSearch + 1);
    url = url.substring(0, hasSearch);
  }
  _currentUri = url;
  _chunked = false;
  _clientContentLength = 0;  // not known yet, or invalid

  // Bound the request-target before it reaches route matching and argument
  // parsing. Answer with 414 so the peer sees why it was refused instead of
  // just having the connection dropped.
#if WEBSERVER_MAX_URI_LEN > 0
  if (url.length() + searchStr.length() > WEBSERVER_MAX_URI_LEN) {
    log_e("Request-target too long (%u bytes, max %u)", (unsigned)(url.length() + searchStr.length()), (unsigned)WEBSERVER_MAX_URI_LEN);
    sendErrorResponse(client, "414 URI Too Long");
    return false;
  }
#endif

  HTTPMethod method = HTTP_ANY;
  size_t num_methods = sizeof(_http_method_str) / sizeof(const char *);
  for (size_t i = 0; i < num_methods; i++) {
    if (methodStr == _http_method_str[i]) {
      method = (HTTPMethod)i;
      break;
    }
  }
  if (method == HTTP_ANY) {
    log_e("Unknown HTTP Method: %s", methodStr.c_str());
    return false;
  }
  _currentMethod = method;

  log_v("method: %s url: %s search: %s", methodStr.c_str(), url.c_str(), searchStr.c_str());

  //attach handler
  RequestHandler *handler;
  for (handler = _firstHandler; handler; handler = handler->next()) {
    if (handler->canHandle(*this, _currentMethod, _currentUri)) {
      break;
    }
  }
  _currentHandler = handler;

  String formData;
  // below is needed only when POST type request
  if (method == HTTP_POST || method == HTTP_PUT || method == HTTP_PATCH || method == HTTP_DELETE) {
    String boundaryStr;
    String headerName;
    String headerValue;
    bool isForm = false;
    bool isEncoded = false;
    //parse headers
    while (1) {
      switch (readLineWithLimit(client, req, WEBSERVER_MAX_LINE_LEN, headerPhaseStart, WEBSERVER_MAX_HEADER_WAIT)) {
        case LineStatus::TooLong:  sendErrorResponse(client, "431 Request Header Fields Too Large"); return false;
        case LineStatus::TimedOut: sendErrorResponse(client, "408 Request Timeout"); return false;
        case LineStatus::Ok:       break;
      }
      if (req == "") {
        break;  //no moar headers
      }
      int headerDiv = req.indexOf(':');
      if (headerDiv == -1) {
        break;
      }
      headerName = req.substring(0, headerDiv);
      headerValue = req.substring(headerDiv + 1);
      headerValue.trim();
      _collectHeader(headerName.c_str(), headerValue.c_str());

      if (headerName.equalsIgnoreCase(FPSTR(Content_Type))) {
        using namespace mime;
        if (headerValue.startsWith(FPSTR(mimeTable[txt].mimeType))) {
          isForm = false;
        } else if (headerValue.startsWith(F("application/x-www-form-urlencoded"))) {
          isForm = false;
          isEncoded = true;
        } else if (headerValue.startsWith(F("multipart/"))) {
          boundaryStr = headerValue.substring(headerValue.indexOf('=') + 1);
          boundaryStr.replace("\"", "");
          if (boundaryStr.length() > 70) {  // RFC 2046: max boundary length is 70
            log_e("Invalid boundary length: %s", boundaryStr.c_str());
            return false;
          }
          isForm = true;
        }
      } else if (headerName.equalsIgnoreCase(F("Content-Length"))) {
        _clientContentLength = headerValue.toInt();
      } else if (headerName.equalsIgnoreCase(F("Host"))) {
        _hostHeader = headerValue;
      }
    }

    if (!isForm && _currentHandler && _currentHandler->canRaw(*this, _currentUri)) {
      log_v("Parse raw");
      _currentRaw.reset(new HTTPRaw());
      _currentRaw->status = RAW_START;
      _currentRaw->totalSize = 0;
      _currentRaw->currentSize = 0;
      log_v("Start Raw");
      _currentHandler->raw(*this, _currentUri, *_currentRaw);
      _currentRaw->status = RAW_WRITE;

      while (_currentRaw->totalSize < (size_t)_clientContentLength) {
        size_t read_len = std::min((size_t)_clientContentLength - _currentRaw->totalSize, (size_t)HTTP_RAW_BUFLEN);
        _currentRaw->currentSize = client.readBytes(_currentRaw->buf, read_len);
        _currentRaw->totalSize += _currentRaw->currentSize;
        if (_currentRaw->currentSize == 0) {
          _currentRaw->status = RAW_ABORTED;
          _currentHandler->raw(*this, _currentUri, *_currentRaw);
          return false;
        }
        _currentHandler->raw(*this, _currentUri, *_currentRaw);
      }
      _currentRaw->status = RAW_END;
      _currentHandler->raw(*this, _currentUri, *_currentRaw);
      log_v("Finish Raw");
    } else if (!isForm) {
      size_t plainLength;
      char *plainBuf = readBytesWithTimeout(client, _clientContentLength, plainLength, HTTP_MAX_POST_WAIT);
      if (plainLength < (size_t)_clientContentLength) {
        free(plainBuf);
        return false;
      }
      if (_clientContentLength > 0) {
        if (isEncoded) {
          //url encoded form
          if (searchStr != "") {
            searchStr += '&';
          }
          searchStr += plainBuf;
        }
        _parseArguments(searchStr);
        if (!isEncoded && _currentArgs) {
          //plain post json or other data
          RequestArgument &arg = _currentArgs[_currentArgCount++];
          arg.key = F("plain");
          arg.value = String(plainBuf);
        }

        log_v("Plain: %s", plainBuf);
        free(plainBuf);
      } else {
        // No content - but we can still have arguments in the URL.
        _parseArguments(searchStr);
      }
    } else {
      // it IS a form
      _parseArguments(searchStr);
      if (!_parseForm(client, boundaryStr, _clientContentLength)) {
        return false;
      }
    }
  } else {
    String headerName;
    String headerValue;
    //parse headers
    while (1) {
      switch (readLineWithLimit(client, req, WEBSERVER_MAX_LINE_LEN, headerPhaseStart, WEBSERVER_MAX_HEADER_WAIT)) {
        case LineStatus::TooLong:  sendErrorResponse(client, "431 Request Header Fields Too Large"); return false;
        case LineStatus::TimedOut: sendErrorResponse(client, "408 Request Timeout"); return false;
        case LineStatus::Ok:       break;
      }
      if (req == "") {
        break;  //no moar headers
      }
      int headerDiv = req.indexOf(':');
      if (headerDiv == -1) {
        break;
      }
      headerName = req.substring(0, headerDiv);
      headerValue = req.substring(headerDiv + 2);
      _collectHeader(headerName.c_str(), headerValue.c_str());

      if (headerName.equalsIgnoreCase("Host")) {
        _hostHeader = headerValue;
      }
    }
    _parseArguments(searchStr);
  }
  client.clear();

  log_v("Request: %s", url.c_str());
  log_v(" Arguments: %s", searchStr.c_str());

  return true;
}

bool WebServer::_collectHeader(const char *headerName, const char *headerValue) {
  RequestArgument *last = nullptr;
  for (RequestArgument *header = _currentHeaders; header; header = header->next) {
    if (header->next == nullptr) {
      last = header;
    }
    if (header->key.equalsIgnoreCase(headerName)) {
      header->value = headerValue;
      log_v("header collected: %s: %s", headerName, headerValue);
      return true;
    }
  }
  assert(last);
  if (_collectAllHeaders) {
    last->next = new RequestArgument();
    last->next->key = headerName;
    last->next->value = headerValue;
    _headerKeysCount++;
    log_v("header collected: %s: %s", headerName, headerValue);
    return true;
  }

  log_v("header skipped: %s: %s", headerName, headerValue);

  return false;
}

void WebServer::_parseArguments(const String &data) {
  log_v("args: %s", data.c_str());
  if (_currentArgs) {
    delete[] _currentArgs;
  }
  _currentArgs = 0;
  _currentArgCount = 0;
  if (data.length() == 0) {
    _currentArgs = new (std::nothrow) RequestArgument[1];
    return;
  }

  // Size the allocation from the number of arguments the parse loop below will
  // actually accept, not from the number of separators. Counting separators
  // lets a body of bare '&'s request an arbitrarily large allocation while
  // yielding no arguments at all.
  for (int pos = 0; pos <= (int)data.length();) {
    int next_arg_index = data.indexOf('&', pos);
    int equal_sign_index = data.indexOf('=', pos);
    if (equal_sign_index != -1 && (next_arg_index == -1 || equal_sign_index < next_arg_index)) {
      ++_currentArgCount;
      if (_currentArgCount >= WEBSERVER_MAX_QUERY_ARGS) {
        log_w("Argument count capped at %u, ignoring the rest", (unsigned)WEBSERVER_MAX_QUERY_ARGS);
        break;
      }
    }
    if (next_arg_index == -1) {
      break;
    }
    pos = next_arg_index + 1;
  }
  log_v("args count: %d", _currentArgCount);

  _currentArgs = new (std::nothrow) RequestArgument[_currentArgCount + 1];
  if (!_currentArgs) {
    log_e("Failed to allocate %d request arguments", _currentArgCount);
    _currentArgCount = 0;
    _currentArgs = new (std::nothrow) RequestArgument[1];
    return;
  }
  int pos = 0;
  int iarg;
  for (iarg = 0; iarg < _currentArgCount;) {
    int equal_sign_index = data.indexOf('=', pos);
    int next_arg_index = data.indexOf('&', pos);
    log_v("pos %d =@%d &@%d", pos, equal_sign_index, next_arg_index);
    if ((equal_sign_index == -1) || ((equal_sign_index > next_arg_index) && (next_arg_index != -1))) {
      log_e("arg missing value: %d", iarg);
      if (next_arg_index == -1) {
        break;
      }
      pos = next_arg_index + 1;
      continue;
    }
    RequestArgument &arg = _currentArgs[iarg];
    arg.key = urlDecode(data.substring(pos, equal_sign_index));
    arg.value = urlDecode(data.substring(equal_sign_index + 1, next_arg_index));
    log_v("arg %d key: %s value: %s", iarg, arg.key.c_str(), arg.value.c_str());
    ++iarg;
    if (next_arg_index == -1) {
      break;
    }
    pos = next_arg_index + 1;
  }
  _currentArgCount = iarg;
  log_v("args count: %d", _currentArgCount);
}

void WebServer::_uploadWriteByte(uint8_t b) {
  if (_currentUpload->currentSize == HTTP_UPLOAD_BUFLEN) {
    if (_currentHandler && _currentHandler->canUpload(*this, _currentUri)) {
      _currentHandler->upload(*this, _currentUri, *_currentUpload);
    }
    _currentUpload->totalSize += _currentUpload->currentSize;
    _currentUpload->currentSize = 0;
  }
  _currentUpload->buf[_currentUpload->currentSize++] = b;
}

int WebServer::_uploadReadByte(NetworkClient &client) {
  int res = client.read();

  if (res < 0) {
    // keep trying until you either read a valid byte or timeout
    const unsigned long startMillis = millis();
    const long timeoutIntervalMillis = client.getTimeout();
    bool timedOut = false;
    for (;;) {
      if (!client.connected()) {
        return -1;
      }
      // loosely modeled after blinkWithoutDelay pattern
      while (!timedOut && !client.available() && client.connected()) {
        delay(2);
        timedOut = (millis() - startMillis) >= timeoutIntervalMillis;
      }

      res = client.read();
      if (res >= 0) {
        return res;  // exit on a valid read
      }
      // NOTE: it is possible to get here and have all of the following
      //       assertions hold true
      //
      //       -- client.available() > 0
      //       -- client.connected == true
      //       -- res == -1
      //
      //       a simple retry strategy overcomes this which is to say the
      //       assertion is not permanent, but the reason that this works
      //       is elusive, and possibly indicative of a more subtle underlying
      //       issue

      timedOut = (millis() - startMillis) >= timeoutIntervalMillis;
      if (timedOut) {
        return res;  // exit on a timeout
      }
    }
  }

  return res;
}

bool WebServer::_parseForm(NetworkClient &client, const String &boundary, uint32_t len) {
  (void)len;
  log_v("Parse Form: Boundary: %s Length: %" PRIu32, boundary.c_str(), len);
  String line;
  int retry = 0;
  do {
    if (readLineWithLimit(client, line, WEBSERVER_MAX_LINE_LEN) != LineStatus::Ok) {
      return false;
    }
    ++retry;
  } while (line.length() == 0 && retry < 3);

  //start reading the form
  if (line == ("--" + boundary)) {
    if (_postArgs) {
      delete[] _postArgs;
    }
    _postArgs = new (std::nothrow) RequestArgument[WEBSERVER_MAX_POST_ARGS];
    if (!_postArgs) {
      log_e("Failed to allocate post arguments");
      return false;
    }
    _postArgsLen = 0;
    size_t skippedLines = 0;
    bool inEmptyRun = false;
    uint32_t emptyRunStart = 0;
    while (1) {
      String argName;
      String argValue;
      String argType;
      String argFilename;
      bool argIsFile = false;

      // Exit when the client is gone or timed out instead of spinning forever
      // on empty reads after a truncated multipart body.
      if (!client.connected() && !client.available()) {
        log_e("Multipart parse aborted: client disconnected");
        return _parseFormUploadAborted();
      }

      if (readLineWithLimit(client, line, WEBSERVER_MAX_LINE_LEN) != LineStatus::Ok) {
        return _parseFormUploadAborted();
      }
      if (line.length() == 0) {
        // A bare CRLF is tolerated, but an unbroken run of empty reads means the
        // body was truncated or the peer stalled. Reads block for the client
        // timeout, so bound the run by time as well as by count.
        if (!inEmptyRun) {
          inEmptyRun = true;
          emptyRunStart = millis();
        } else if (millis() - emptyRunStart > HTTP_MAX_POST_WAIT) {
          log_e("Multipart parse aborted: no data for %u ms", (unsigned)HTTP_MAX_POST_WAIT);
          return _parseFormUploadAborted();
        }
        if (++skippedLines > WEBSERVER_MAX_MULTIPART_SKIP_LINES) {
          log_e("Multipart parse aborted: %u blank lines without a part", (unsigned)skippedLines);
          return _parseFormUploadAborted();
        }
        continue;
      }
      inEmptyRun = false;
      if (line.length() > (size_t)19 && line.substring(0, 19).equalsIgnoreCase(F("Content-Disposition"))) {
        skippedLines = 0;
        int nameStart = line.indexOf('=');
        if (nameStart != -1) {
          argName = line.substring(nameStart + 2);
          nameStart = argName.indexOf('=');
          if (nameStart == -1) {
            argName = argName.substring(0, argName.length() - 1);
          } else {
            argFilename = argName.substring(nameStart + 2, argName.length() - 1);
            argName = argName.substring(0, argName.indexOf('"'));
            argIsFile = true;
            log_v("PostArg FileName: %s", argFilename.c_str());
            //use GET to set the filename if uploading using blob
            if (argFilename == F("blob") && hasArg(FPSTR(filename))) {
              argFilename = arg(FPSTR(filename));
            }
          }
          log_v("PostArg Name: %s", argName.c_str());
          using namespace mime;
          argType = FPSTR(mimeTable[txt].mimeType);
          if (readLineWithLimit(client, line, WEBSERVER_MAX_LINE_LEN) != LineStatus::Ok) {
            return _parseFormUploadAborted();
          }
          while (line.length() > 0) {
            if (line.length() > (size_t)12 && line.substring(0, 12).equalsIgnoreCase(FPSTR(Content_Type))) {
              argType = line.substring(line.indexOf(':') + 2);
            }
            //skip over any other headers
            if (readLineWithLimit(client, line, WEBSERVER_MAX_LINE_LEN) != LineStatus::Ok) {
              return _parseFormUploadAborted();
            }
          }
          log_v("PostArg Type: %s", argType.c_str());
          if (!argIsFile) {
            while (1) {
              // Field values are the one place a long line is legitimate, so the
              // cap is its own, more generous limit. The time bound still applies.
              if (readLineWithLimit(client, line, WEBSERVER_MAX_POST_ARG_LEN) != LineStatus::Ok) {
                log_e("Multipart parse aborted: field value too long or too slow");
                return _parseFormUploadAborted();
              }
              if (line.startsWith("--" + boundary)) {
                break;
              }
              if (!client.connected() && !client.available() && line.length() == 0) {
                log_e("Multipart parse aborted: truncated field value");
                return _parseFormUploadAborted();
              }
              if (argValue.length() > (size_t)0) {
                argValue += "\n";
              }
              argValue += line;
            }
            log_v("PostArg Value: %s", argValue.c_str());

            RequestArgument &arg = _postArgs[_postArgsLen++];
            arg.key = argName;
            arg.value = argValue;

            if (line == ("--" + boundary + "--")) {
              log_v("Done Parsing POST");
              break;
            } else if (_postArgsLen >= WEBSERVER_MAX_POST_ARGS) {
              log_e("Too many PostArgs (max: %u) in request.", WEBSERVER_MAX_POST_ARGS);
              return _parseFormUploadAborted();
            }
          } else {
            _currentUpload.reset(new HTTPUpload());
            _currentUpload->status = UPLOAD_FILE_START;
            _currentUpload->name = argName;
            _currentUpload->filename = argFilename;
            _currentUpload->type = argType;
            _currentUpload->totalSize = 0;
            _currentUpload->currentSize = 0;
            log_v("Start File: %s Type: %s", _currentUpload->filename.c_str(), _currentUpload->type.c_str());
            if (_currentHandler && _currentHandler->canUpload(*this, _currentUri)) {
              _currentHandler->upload(*this, _currentUri, *_currentUpload);
            }
            _currentUpload->status = UPLOAD_FILE_WRITE;

            int fastBoundaryLen = 4 /* \r\n-- */ + boundary.length() + 1 /* \0 */;
            char fastBoundary[fastBoundaryLen];
            snprintf(fastBoundary, fastBoundaryLen, "\r\n--%s", boundary.c_str());
            int boundaryPtr = 0;
            while (true) {
              int ret = _uploadReadByte(client);
              if (ret < 0) {
                // Unexpected, we should have had data available per above
                return _parseFormUploadAborted();
              }
              char in = (char)ret;
              if (in == fastBoundary[boundaryPtr]) {
                // The input matched the current expected character, advance and possibly exit this file
                boundaryPtr++;
                if (boundaryPtr == fastBoundaryLen - 1) {
                  // We read the whole boundary line, we're done here!
                  break;
                }
              } else {
                // The char doesn't match what we want, so dump whatever matches we had, the read in char, and reset ptr to start
                for (int i = 0; i < boundaryPtr; i++) {
                  _uploadWriteByte(fastBoundary[i]);
                }
                if (in == fastBoundary[0]) {
                  // This could be the start of the real end, mark it so and don't emit/skip it
                  boundaryPtr = 1;
                } else {
                  // Not the 1st char of our pattern, so emit and ignore
                  _uploadWriteByte(in);
                  boundaryPtr = 0;
                }
              }
            }
            // Found the boundary string, finish processing this file upload
            if (_currentHandler && _currentHandler->canUpload(*this, _currentUri)) {
              _currentHandler->upload(*this, _currentUri, *_currentUpload);
            }
            _currentUpload->totalSize += _currentUpload->currentSize;
            _currentUpload->status = UPLOAD_FILE_END;
            if (_currentHandler && _currentHandler->canUpload(*this, _currentUri)) {
              _currentHandler->upload(*this, _currentUri, *_currentUpload);
            }
            log_v("End File: %s Type: %s Size: %lu", _currentUpload->filename.c_str(), _currentUpload->type.c_str(), (unsigned long)_currentUpload->totalSize);
            if (!client.connected()) {
              return _parseFormUploadAborted();
            }
            if (readLineWithLimit(client, line, WEBSERVER_MAX_LINE_LEN) != LineStatus::Ok) {
              return _parseFormUploadAborted();
            }
            if (line == "--") {  // extra two dashes mean we reached the end of all form fields
              log_v("Done Parsing POST");
              break;
            }
            continue;
          }
        }
      } else if (line == ("--" + boundary + "--")) {
        break;
      } else if (++skippedLines > WEBSERVER_MAX_MULTIPART_SKIP_LINES) {
        // A preamble or epilogue around the parts is legal (RFC 2046), so
        // unrecognized lines are skipped -- but not indefinitely.
        log_e("Multipart parse aborted: %u lines without a part", (unsigned)skippedLines);
        return _parseFormUploadAborted();
      }
    }

    int iarg;
    int totalArgs = ((WEBSERVER_MAX_POST_ARGS - _postArgsLen) < _currentArgCount) ? (WEBSERVER_MAX_POST_ARGS - _postArgsLen) : _currentArgCount;
    for (iarg = 0; iarg < totalArgs; iarg++) {
      RequestArgument &arg = _postArgs[_postArgsLen++];
      arg.key = _currentArgs[iarg].key;
      arg.value = _currentArgs[iarg].value;
    }
    if (_currentArgs) {
      delete[] _currentArgs;
    }
    _currentArgs = new (std::nothrow) RequestArgument[_postArgsLen ? _postArgsLen : 1];
    if (!_currentArgs) {
      return _parseFormUploadAborted();
    }
    for (iarg = 0; iarg < _postArgsLen; iarg++) {
      RequestArgument &arg = _currentArgs[iarg];
      arg.key = _postArgs[iarg].key;
      arg.value = _postArgs[iarg].value;
    }
    _currentArgCount = iarg;
    if (_postArgs) {
      delete[] _postArgs;
      _postArgs = nullptr;
      _postArgsLen = 0;
    }
    return true;
  }
  log_e("Error: line: %s", line.c_str());
  return false;
}

String WebServer::urlDecode(const String &text) {
  String decoded = "";
  char temp[] = "0x00";
  unsigned int len = text.length();
  unsigned int i = 0;
  while (i < len) {
    char decodedChar;
    char encodedChar = text.charAt(i++);
    if ((encodedChar == '%') && (i + 1 < len)) {
      temp[2] = text.charAt(i++);
      temp[3] = text.charAt(i++);

      decodedChar = strtol(temp, NULL, 16);
    } else {
      if (encodedChar == '+') {
        decodedChar = ' ';
      } else {
        decodedChar = encodedChar;  // normal ascii char
      }
    }
    decoded += decodedChar;
  }
  return decoded;
}

bool WebServer::_parseFormUploadAborted() {
  if (_currentUpload) {
    _currentUpload->status = UPLOAD_FILE_ABORTED;
    if (_currentHandler && _currentHandler->canUpload(*this, _currentUri)) {
      _currentHandler->upload(*this, _currentUri, *_currentUpload);
    }
  }
  // Drop any fields collected before the abort so they cannot shadow later
  // requests via arg()/hasArg().
  if (_postArgs) {
    delete[] _postArgs;
    _postArgs = nullptr;
    _postArgsLen = 0;
  }
  return false;
}
