/*
  WebServer.cpp - Dead simple web-server.
  Supports only one simultaneous client, knows how to handle GET and POST.

  Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.

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
#include <libb64/cdecode.h>
#include <libb64/cencode.h>
#include "esp_random.h"
#include "NetworkServer.h"
#include "NetworkClient.h"
#include "WebServer.h"
#include "FS.h"
#include "detail/RequestHandlersImpl.h"
#include "MD5Builder.h"
#include "SHA1Builder.h"
#include "base64.h"

static const char AUTHORIZATION_HEADER[] = "Authorization";
static const char qop_auth[] PROGMEM = "qop=auth";
static const char qop_auth_quoted[] PROGMEM = "qop=\"auth\"";
static const char WWW_Authenticate[] = "WWW-Authenticate";
static const char Content_Length[] = "Content-Length";
static const char ETAG_HEADER[] = "If-None-Match";

WebServer::WebServer(IPAddress addr, int port) : _server(addr, port) {
  log_v("WebServer::Webserver(addr=%s, port=%d)", addr.toString().c_str(), port);
}

WebServer::WebServer(int port) : _server(port) {
  log_v("WebServer::Webserver(port=%d)", port);
}

WebServer::~WebServer() {
  _server.close();

  _clearRequestHeaders();
  _clearResponseHeaders();
  delete _chain;

  RequestHandler *handler = _firstHandler;
  while (handler) {
    RequestHandler *next = handler->next();
    delete handler;
    handler = next;
  }
  _firstHandler = nullptr;
}

void WebServer::begin() {
  close();
  _server.begin();
  _server.setNoDelay(true);
}

void WebServer::begin(uint16_t port) {
  close();
  _server.begin(port);
  _server.setNoDelay(true);
}

String WebServer::_extractParam(String &authReq, const String &param, const char delimit) {
  int _begin = authReq.indexOf(param);
  if (_begin == -1) {
    return "";
  }
  return authReq.substring(_begin + param.length(), authReq.indexOf(delimit, _begin + param.length()));
}

static String md5str(String &in) {
  MD5Builder md5 = MD5Builder();
  md5.begin();
  md5.add(in);
  md5.calculate();
  return md5.toString();
}

bool WebServer::authenticateBasicSHA1(const char *_username, const char *_sha1Base64orHex) {
  return WebServer::authenticate([_username, _sha1Base64orHex](HTTPAuthMethod mode, String username, String params[]) -> String * {
    // rather than work on a password to compare with; we take the sha1 of the
    // password received over the wire and compare that to the base64 encoded
    // sha1 passed as _sha1base64. That way there is no need to have a
    // plaintext password in the code/binary (though note that SHA1 is well
    // past its retirement age). When that matches - we `cheat' by returning
    // the password we got in the first place; so the normal BasicAuth
    // can be completed. Note that this cannot work for a digest auth -
    // as there the password in the clear is part of the calculation.

    if (params == nullptr) {
      log_e("Something went wrong. params is NULL");
      return NULL;
    }

    uint8_t sha1[20];
    char sha1calc[48];  // large enough for base64 and Hex representation
    String ret;
    SHA1Builder sha_builder;
    base64 b64;

    log_v("Trying to authenticate user %s using SHA1.", username.c_str());
    sha_builder.begin();
    sha_builder.add((uint8_t *)params[0].c_str(), params[0].length());
    sha_builder.calculate();
    sha_builder.getBytes(sha1);

    // we can either decode _sha1base64orHex and then compare the 20 bytes;
    // or encode the sha we calculated. We pick the latter as encoding of a
    // fixed array of 20 bytes is safer than operating on something external.
    if (strlen(_sha1Base64orHex) == 20 * 2) {  // 2 chars per byte
      sha_builder.bytes2hex(sha1calc, sizeof(sha1calc), sha1, sizeof(sha1));
      log_v("Calculated SHA1 in hex: %s", sha1calc);
    } else {
      ret = b64.encode(sha1, sizeof(sha1));
      ret.toCharArray(sha1calc, sizeof(sha1calc));
      log_v("Calculated SHA1 in base64: %s", sha1calc);
    }

    return ((username.equalsConstantTime(_username)) && (String((char *)sha1calc).equalsConstantTime(_sha1Base64orHex))
            && (mode == BASIC_AUTH) /* to keep things somewhat time constant. */
           )
             ? new String(params[0])
             : NULL;
  });
}

bool WebServer::authenticate(const char *_username, const char *_password) {
  return WebServer::authenticate([_username, _password](HTTPAuthMethod mode, String username, String params[]) -> String * {
    return username.equalsConstantTime(_username) ? new String(_password) : NULL;
  });
}

bool WebServer::authenticate(THandlerFunctionAuthCheck fn) {
  if (!hasHeader(FPSTR(AUTHORIZATION_HEADER))) {
    return false;
  }

  String authReq = header(FPSTR(AUTHORIZATION_HEADER));
  if (authReq.startsWith(AuthTypeBasic)) {
    log_v("Trying to authenticate using Basic Auth");
    bool ret = false;

    authReq = authReq.substring(6);  // length of AuthTypeBasic including the space at the end.
    authReq.trim();

    /* base64 encoded string is always shorter (or equal) in length */
    char *decoded = (authReq.length() < HTTP_MAX_BASIC_AUTH_LEN) ? new char[authReq.length()] : NULL;
    if (decoded) {
      char *p;
      if (base64_decode_chars(authReq.c_str(), authReq.length(), decoded) && (p = index(decoded, ':')) && p) {
        authReq = "";
        /* Note: rfc7617 guarantees that there will not be an escaped colon in the username itself.
         * Note:  base64_decode_chars() guarantees a terminating \0
         */
        *p = '\0';
        char *_username = decoded, *_password = p + 1;
        String params[] = {_password, _srealm};
        String *password = fn(BASIC_AUTH, _username, params);

        if (password) {
          ret = password->equalsConstantTime(_password);
          // we're more concerned about the password; as the attacker already
          // knows the _pasword. Arduino's string handling is simple; it reallocs
          // even when smaller; so a memset is enough (no capacity/size).
          memset((void *)password->c_str(), 0, password->length());
          delete password;
        }
      }
      delete[] decoded;
    }
    authReq = "";
    log_v("Authentication %s", ret ? "Success" : "Failed");
    return ret;
  } else if (authReq.startsWith(AuthTypeDigest)) {
    log_v("Trying to authenticate using Digest Auth");
    authReq = authReq.substring(7);
    log_v("%s", authReq.c_str());

    // extracting required parameters for RFC 2069 simpler Digest
    String _username = _extractParam(authReq, F("username=\""), '\"');
    String _realm = _extractParam(authReq, F("realm=\""), '\"');
    String _uri = _extractParam(authReq, F("uri=\""), '\"');
    if (!_username.length()) {
      goto exf;
    }

    String params[] = {_realm, _uri};
    String *password = fn(DIGEST_AUTH, _username, params);
    if (!password) {
      goto exf;
    }

    String _H1 = md5str(String(_username) + ':' + _realm + ':' + *password);
    // we're extra concerned; as digest request us to know the password
    // in the clear.
    memset((void *)password->c_str(), 0, password->length());
    delete password;
    _username = "";

    String _nonce = _extractParam(authReq, F("nonce=\""), '\"');
    String _response = _extractParam(authReq, F("response=\""), '\"');
    String _opaque = _extractParam(authReq, F("opaque=\""), '\"');

    if ((!_realm.length()) || (!_nonce.length()) || (!_uri.length()) || (!_response.length()) || (!_opaque.length())) {
      goto exf;
    }

    if ((_opaque != _sopaque) || (_nonce != _snonce) || (_realm != _srealm)) {
      goto exf;
    }

    // parameters for the RFC 2617 newer Digest
    String _nc, _cnonce;
    if (authReq.indexOf(FPSTR(qop_auth)) != -1 || authReq.indexOf(FPSTR(qop_auth_quoted)) != -1) {
      _nc = _extractParam(authReq, F("nc="), ',');
      _cnonce = _extractParam(authReq, F("cnonce=\""), '\"');
    }

    log_v("Hash of user:realm:pass=%s", _H1.c_str());
    String _H2 = "";
    if (_currentMethod == HTTP_GET) {
      _H2 = md5str(String(F("GET:")) + _uri);
    } else if (_currentMethod == HTTP_POST) {
      _H2 = md5str(String(F("POST:")) + _uri);
    } else if (_currentMethod == HTTP_PUT) {
      _H2 = md5str(String(F("PUT:")) + _uri);
    } else if (_currentMethod == HTTP_DELETE) {
      _H2 = md5str(String(F("DELETE:")) + _uri);
    } else {
      _H2 = md5str(String(F("GET:")) + _uri);
    }
    log_v("Hash of GET:uri=%s", _H2.c_str());
    String _responsecheck = "";
    if (authReq.indexOf(FPSTR(qop_auth)) != -1 || authReq.indexOf(FPSTR(qop_auth_quoted)) != -1) {
      _responsecheck = md5str(_H1 + ':' + _nonce + ':' + _nc + ':' + _cnonce + F(":auth:") + _H2);
    } else {
      _responsecheck = md5str(_H1 + ':' + _nonce + ':' + _H2);
    }
    authReq = "";

    log_v("The Proper response=%s", _responsecheck.c_str());
    bool ret = _response == _responsecheck;
    log_v("Authentication %s", ret ? "Success" : "Failed");
    return ret;
  } else if (authReq.length()) {
    // OTHER_AUTH
    log_v("Trying to authenticate using Other Auth, authReq=%s", authReq.c_str());
    String *ret = fn(OTHER_AUTH, authReq, {});
    if (ret) {
      log_v("Authentication Success");
      delete ret;
      return true;
    }
  }
exf:
  authReq = "";
  log_v("Authentication Failed");
  return false;
}

String WebServer::_getRandomHexString() {
  char buffer[33];  // buffer to hold 32 Hex Digit + /0
  int i;
  for (i = 0; i < 4; i++) {
    sprintf(buffer + (i * 8), "%08lx", esp_random());
  }
  return String(buffer);
}

void WebServer::requestAuthentication(HTTPAuthMethod mode, const char *realm, const String &authFailMsg) {
  if (realm == NULL) {
    _srealm = String(F("Login Required"));
  } else {
    _srealm = String(realm);
  }
  if (mode == BASIC_AUTH) {
    sendHeader(String(FPSTR(WWW_Authenticate)), AuthTypeBasic + String(F(" realm=\"")) + _srealm + String(F("\"")));
  } else {
    _snonce = _getRandomHexString();
    _sopaque = _getRandomHexString();
    sendHeader(
      String(FPSTR(WWW_Authenticate)), AuthTypeDigest + String(F(" realm=\"")) + _srealm + String(F("\", qop=\"auth\", nonce=\"")) + _snonce
                                         + String(F("\", opaque=\"")) + _sopaque + String(F("\""))
    );
  }
  using namespace mime;
  send(401, String(FPSTR(mimeTable[html].mimeType)), authFailMsg);
}

RequestHandler &WebServer::on(const Uri &uri, WebServer::THandlerFunction handler) {
  return on(uri, HTTP_ANY, handler);
}

RequestHandler &WebServer::on(const Uri &uri, HTTPMethod method, WebServer::THandlerFunction fn) {
  return on(uri, method, fn, _fileUploadHandler);
}

RequestHandler &WebServer::on(const Uri &uri, HTTPMethod method, WebServer::THandlerFunction fn, WebServer::THandlerFunction ufn) {
  FunctionRequestHandler *handler = new FunctionRequestHandler(fn, ufn, uri, method);
  _addRequestHandler(handler);
  return *handler;
}

bool WebServer::removeRoute(const char *uri) {
  return removeRoute(String(uri), HTTP_ANY);
}

bool WebServer::removeRoute(const char *uri, HTTPMethod method) {
  return removeRoute(String(uri), method);
}

bool WebServer::removeRoute(const String &uri) {
  return removeRoute(uri, HTTP_ANY);
}

bool WebServer::removeRoute(const String &uri, HTTPMethod method) {
  bool anyHandlerRemoved = false;
  RequestHandler *handler = _firstHandler;
  RequestHandler *previousHandler = nullptr;

  while (handler) {
    if (handler->canHandle(method, uri)) {
      if (_removeRequestHandler(handler)) {
        anyHandlerRemoved = true;
        // Move to the next handler
        if (previousHandler) {
          handler = previousHandler->next();
        } else {
          handler = _firstHandler;
        }
        continue;
      }
    }
    previousHandler = handler;
    handler = handler->next();
  }

  return anyHandlerRemoved;
}

void WebServer::addHandler(RequestHandler *handler) {
  _addRequestHandler(handler);
}

bool WebServer::removeHandler(RequestHandler *handler) {
  return _removeRequestHandler(handler);
}

void WebServer::_addRequestHandler(RequestHandler *handler) {
  if (!_lastHandler) {
    _firstHandler = handler;
    _lastHandler = handler;
  } else {
    _lastHandler->next(handler);
    _lastHandler = handler;
  }
}

bool WebServer::_removeRequestHandler(RequestHandler *handler) {
  RequestHandler *current = _firstHandler;
  RequestHandler *previous = nullptr;

  while (current != nullptr) {
    if (current == handler) {
      if (previous == nullptr) {
        _firstHandler = current->next();
      } else {
        previous->next(current->next());
      }

      if (current == _lastHandler) {
        _lastHandler = previous;
      }

      // Delete 'matching' handler
      delete current;
      return true;
    }
    previous = current;
    current = current->next();
  }
  return false;
}

void WebServer::serveStatic(const char *uri, FS &fs, const char *path, const char *cache_header) {
  _addRequestHandler(new StaticRequestHandler(fs, path, uri, cache_header));
}

void WebServer::handleClient() {
  if (_currentStatus == HC_NONE) {
    _currentClient = _server.accept();
    if (!_currentClient) {
      if (_nullDelay) {
        delay(1);
      }
      return;
    }

    log_v("New client: client.localIP()=%s", _currentClient.localIP().toString().c_str());

    _currentStatus = HC_WAIT_READ;
    _statusChange = millis();
  }

  bool keepCurrentClient = false;
  bool callYield = false;

  if (_currentClient.connected()) {
    switch (_currentStatus) {
      case HC_NONE:
        // No-op to avoid C++ compiler warning
        break;
      case HC_WAIT_READ:
        // Wait for data from client to become available
        if (_currentClient.available()) {
          _currentClient.setTimeout(HTTP_MAX_SEND_WAIT); /* / 1000 removed, WifiClient setTimeout changed to ms */
          if (_parseRequest(_currentClient)) {
            _contentLength = CONTENT_LENGTH_NOT_SET;
            _responseCode = 0;
            _clearResponseHeaders();

            // Run server-level middlewares
            if (_chain) {
              _chain->runChain(*this, [this]() {
                return _handleRequest();
              });
            } else {
              _handleRequest();
            }

            if (_currentClient.isSSE()) {
              _currentStatus = HC_WAIT_CLOSE;
              _statusChange = millis();
              keepCurrentClient = true;
            }
            // Fix for issue with Chrome based browsers: https://github.com/espressif/arduino-esp32/issues/3652
            //           if (_currentClient.connected()) {
            //             _currentStatus = HC_WAIT_CLOSE;
            //             _statusChange = millis();
            //             keepCurrentClient = true;
            //           }
          }
        } else {  // !_currentClient.available()
          if (millis() - _statusChange <= HTTP_MAX_DATA_WAIT) {
            keepCurrentClient = true;
          }
          callYield = true;
        }
        break;
      case HC_WAIT_CLOSE:
        if (_currentClient.isSSE()) {
          // Never close connection
          _statusChange = millis();
        }
        // Wait for client to close the connection
        if (millis() - _statusChange <= HTTP_MAX_CLOSE_WAIT) {
          keepCurrentClient = true;
          callYield = true;
        }
    }
  }

  if (!keepCurrentClient) {
    _currentClient = NetworkClient();
    _currentStatus = HC_NONE;
    _currentUpload.reset();
    _currentRaw.reset();
  }

  if (callYield) {
    yield();
  }
}

void WebServer::close() {
  _server.close();
  _currentStatus = HC_NONE;
  if (!_headerKeysCount) {
    collectHeaders(0, 0);
  }
}

void WebServer::stop() {
  close();
}

void WebServer::sendHeader(const String &name, const String &value, bool first) {
  RequestArgument *header = new RequestArgument();
  header->key = name;
  header->value = value;

  if (!_responseHeaders || first) {
    header->next = _responseHeaders;
    _responseHeaders = header;
  } else {
    RequestArgument *last = _responseHeaders;
    while (last->next) {
      last = last->next;
    }
    last->next = header;
  }

  _responseHeaderCount++;
}

void WebServer::setContentLength(const size_t contentLength) {
  _contentLength = contentLength;
}

void WebServer::enableDelay(boolean value) {
  _nullDelay = value;
}

void WebServer::enableCORS(boolean value) {
  _corsEnabled = value;
}

void WebServer::enableCrossOrigin(boolean value) {
  enableCORS(value);
}

void WebServer::enableETag(bool enable, ETagFunction fn) {
  _eTagEnabled = enable;
  _eTagFunction = fn;
}

void WebServer::_prepareHeader(String &response, int code, const char *content_type, size_t contentLength) {
  _responseCode = code;

  response.concat(version());
  response.concat(' ');
  response.concat(String(code));
  response.concat(' ');
  response.concat(responseCodeToString(code));
  response.concat(F("\r\n"));

  using namespace mime;
  if (!content_type) {
    content_type = mimeTable[html].mimeType;
  }

  sendHeader(String(F("Content-Type")), String(FPSTR(content_type)), true);
  if (_contentLength == CONTENT_LENGTH_NOT_SET) {
    sendHeader(String(FPSTR(Content_Length)), String(contentLength));
  } else if (_contentLength != CONTENT_LENGTH_UNKNOWN) {
    sendHeader(String(FPSTR(Content_Length)), String(_contentLength));
  } else if (_contentLength == CONTENT_LENGTH_UNKNOWN && _currentVersion) {  //HTTP/1.1 or above client
    //let's do chunked
    _chunked = true;
    sendHeader(String(F("Accept-Ranges")), String(F("none")));
    sendHeader(String(F("Transfer-Encoding")), String(F("chunked")));
  }
  if (_corsEnabled) {
    sendHeader(String(FPSTR("Access-Control-Allow-Origin")), String("*"));
    sendHeader(String(FPSTR("Access-Control-Allow-Methods")), String("*"));
    sendHeader(String(FPSTR("Access-Control-Allow-Headers")), String("*"));
  }
  sendHeader(String(F("Connection")), String(F("close")));

  for (RequestArgument *header = _responseHeaders; header; header = header->next) {
    response.concat(header->key);
    response.concat(F(": "));
    response.concat(header->value);
    response.concat(F("\r\n"));
  }

  response.concat(F("\r\n"));
}

void WebServer::send(int code, const char *content_type, const String &content) {
  String header;
  // Can we assume the following?
  //if(code == 200 && content.length() == 0 && _contentLength == CONTENT_LENGTH_NOT_SET)
  //  _contentLength = CONTENT_LENGTH_UNKNOWN;
  _prepareHeader(header, code, content_type, content.length());
  _currentClientWrite(header.c_str(), header.length());
  if (content.length()) {
    sendContent(content);
  }
}

void WebServer::send(int code, char *content_type, const String &content) {
  send(code, (const char *)content_type, content);
}

void WebServer::send(int code, const String &content_type, const String &content) {
  send(code, (const char *)content_type.c_str(), content);
}

void WebServer::send(int code, const char *content_type, const char *content) {
  const String passStr = (String)content;
  if (strlen(content) != passStr.length()) {
    log_e("String cast failed.  Use send_P for long arrays");
  }
  send(code, content_type, passStr);
}

void WebServer::send_P(int code, PGM_P content_type, PGM_P content) {
  size_t contentLength = 0;

  if (content != NULL) {
    contentLength = strlen_P(content);
  }

  String header;
  char type[64];
  memccpy_P((void *)type, (PGM_VOID_P)content_type, 0, sizeof(type));
  _prepareHeader(header, code, (const char *)type, contentLength);
  _currentClientWrite(header.c_str(), header.length());
  sendContent_P(content);
}

void WebServer::send_P(int code, PGM_P content_type, PGM_P content, size_t contentLength) {
  String header;
  char type[64];
  memccpy_P((void *)type, (PGM_VOID_P)content_type, 0, sizeof(type));
  _prepareHeader(header, code, (const char *)type, contentLength);
  sendContent(header);
  sendContent_P(content, contentLength);
}

void WebServer::sendContent(const String &content) {
  sendContent(content.c_str(), content.length());
}

void WebServer::sendContent(const char *content, size_t contentLength) {
  const char *footer = "\r\n";
  if (_chunked) {
    char *chunkSize = (char *)malloc(11);
    if (chunkSize) {
      sprintf(chunkSize, "%x%s", contentLength, footer);
      _currentClientWrite(chunkSize, strlen(chunkSize));
      free(chunkSize);
    }
  }
  _currentClientWrite(content, contentLength);
  if (_chunked) {
    _currentClient.write(footer, 2);
    if (contentLength == 0) {
      _chunked = false;
    }
  }
}

void WebServer::sendContent_P(PGM_P content) {
  sendContent_P(content, strlen_P(content));
}

void WebServer::sendContent_P(PGM_P content, size_t size) {
  const char *footer = "\r\n";
  if (_chunked) {
    char *chunkSize = (char *)malloc(11);
    if (chunkSize) {
      sprintf(chunkSize, "%x%s", size, footer);
      _currentClientWrite(chunkSize, strlen(chunkSize));
      free(chunkSize);
    }
  }
  _currentClientWrite_P(content, size);
  if (_chunked) {
    _currentClient.write(footer, 2);
    if (size == 0) {
      _chunked = false;
    }
  }
}

void WebServer::_streamFileCore(const size_t fileSize, const String &fileName, const String &contentType, const int code) {
  using namespace mime;
  setContentLength(fileSize);
  if (fileName.endsWith(String(FPSTR(mimeTable[gz].endsWith))) && contentType != String(FPSTR(mimeTable[gz].mimeType))
      && contentType != String(FPSTR(mimeTable[none].mimeType))) {
    sendHeader(F("Content-Encoding"), F("gzip"));
  }
  send(code, contentType, "");
}

String WebServer::pathArg(unsigned int i) const {
  if (_currentHandler != nullptr) {
    return _currentHandler->pathArg(i);
  }
  return "";
}

String WebServer::arg(const String &name) const {
  for (int j = 0; j < _postArgsLen; ++j) {
    if (_postArgs[j].key == name) {
      return _postArgs[j].value;
    }
  }
  for (int i = 0; i < _currentArgCount; ++i) {
    if (_currentArgs[i].key == name) {
      return _currentArgs[i].value;
    }
  }
  return "";
}

String WebServer::arg(int i) const {
  if (i < _currentArgCount) {
    return _currentArgs[i].value;
  }
  return "";
}

String WebServer::argName(int i) const {
  if (i < _currentArgCount) {
    return _currentArgs[i].key;
  }
  return "";
}

int WebServer::args() const {
  return _currentArgCount;
}

bool WebServer::hasArg(const String &name) const {
  for (int j = 0; j < _postArgsLen; ++j) {
    if (_postArgs[j].key == name) {
      return true;
    }
  }
  for (int i = 0; i < _currentArgCount; ++i) {
    if (_currentArgs[i].key == name) {
      return true;
    }
  }
  return false;
}

String WebServer::header(const String &name) const {
  for (RequestArgument *current = _currentHeaders; current; current = current->next) {
    if (current->key.equalsIgnoreCase(name)) {
      return current->value;
    }
  }
  return "";
}

void WebServer::collectHeaders(const char *headerKeys[], const size_t headerKeysCount) {
  collectAllHeaders();
  _collectAllHeaders = false;

  _headerKeysCount += headerKeysCount;

  RequestArgument *last = _currentHeaders->next;

  for (int i = 2; i < _headerKeysCount; i++) {
    last->next = new RequestArgument();
    last->next->key = headerKeys[i - 2];
    last = last->next;
  }
}

String WebServer::header(int i) const {
  RequestArgument *current = _currentHeaders;
  while (current && i--) {
    current = current->next;
  }
  return current ? current->value : emptyString;
}

String WebServer::headerName(int i) const {
  RequestArgument *current = _currentHeaders;
  while (current && i--) {
    current = current->next;
  }
  return current ? current->key : emptyString;
}

int WebServer::headers() const {
  return _headerKeysCount;
}

bool WebServer::hasHeader(const String &name) const {
  return header(name).length() > 0;
}

String WebServer::hostHeader() const {
  return _hostHeader;
}

void WebServer::onFileUpload(THandlerFunction fn) {
  _fileUploadHandler = fn;
}

void WebServer::onNotFound(THandlerFunction fn) {
  _notFoundHandler = fn;
}

bool WebServer::_handleRequest() {
  bool handled = false;
  if (_currentHandler) {
    handled = _currentHandler->process(*this, _currentMethod, _currentUri);
    if (!handled) {
      log_e("request handler failed to handle request");
    }
  }
  // DO NOT LOG if _currentHandler == null !!
  // This is is valid use case to handle any other requests
  // Also, this is just causing log flooding
  if (!handled && _notFoundHandler) {
    _notFoundHandler();
    handled = true;
  }
  if (!handled) {
    using namespace mime;
    send(404, String(FPSTR(mimeTable[html].mimeType)), String(F("Not found: ")) + _currentUri);
    handled = true;
  }
  if (handled) {
    _finalizeResponse();
  }
  _currentUri = "";
  return handled;
}

void WebServer::_finalizeResponse() {
  if (_chunked) {
    sendContent("");
  }
}

String WebServer::responseCodeToString(int code) {
  switch (code) {
    case 100: return F("Continue");
    case 101: return F("Switching Protocols");
    case 200: return F("OK");
    case 201: return F("Created");
    case 202: return F("Accepted");
    case 203: return F("Non-Authoritative Information");
    case 204: return F("No Content");
    case 205: return F("Reset Content");
    case 206: return F("Partial Content");
    case 300: return F("Multiple Choices");
    case 301: return F("Moved Permanently");
    case 302: return F("Found");
    case 303: return F("See Other");
    case 304: return F("Not Modified");
    case 305: return F("Use Proxy");
    case 307: return F("Temporary Redirect");
    case 400: return F("Bad Request");
    case 401: return F("Unauthorized");
    case 402: return F("Payment Required");
    case 403: return F("Forbidden");
    case 404: return F("Not Found");
    case 405: return F("Method Not Allowed");
    case 406: return F("Not Acceptable");
    case 407: return F("Proxy Authentication Required");
    case 408: return F("Request Time-out");
    case 409: return F("Conflict");
    case 410: return F("Gone");
    case 411: return F("Length Required");
    case 412: return F("Precondition Failed");
    case 413: return F("Request Entity Too Large");
    case 414: return F("Request-URI Too Large");
    case 415: return F("Unsupported Media Type");
    case 416: return F("Requested range not satisfiable");
    case 417: return F("Expectation Failed");
    case 500: return F("Internal Server Error");
    case 501: return F("Not Implemented");
    case 502: return F("Bad Gateway");
    case 503: return F("Service Unavailable");
    case 504: return F("Gateway Time-out");
    case 505: return F("HTTP Version not supported");
    default:  return F("");
  }
}

void WebServer::_clearResponseHeaders() {
  _responseHeaderCount = 0;
  RequestArgument *current = _responseHeaders;
  while (current) {
    RequestArgument *next = current->next;
    delete current;
    current = next;
  }
  _responseHeaders = nullptr;
}

void WebServer::_clearRequestHeaders() {
  _headerKeysCount = 0;
  RequestArgument *current = _currentHeaders;
  while (current) {
    RequestArgument *next = current->next;
    delete current;
    current = next;
  }
  _currentHeaders = nullptr;
}

void WebServer::collectAllHeaders() {
  _clearRequestHeaders();

  _currentHeaders = new RequestArgument();
  _currentHeaders->key = FPSTR(AUTHORIZATION_HEADER);

  _currentHeaders->next = new RequestArgument();
  _currentHeaders->next->key = FPSTR(ETAG_HEADER);

  _headerKeysCount = 2;
  _collectAllHeaders = true;
}

const String &WebServer::responseHeader(String name) const {
  for (RequestArgument *current = _responseHeaders; current; current = current->next) {
    if (current->key.equalsIgnoreCase(name)) {
      return current->value;
    }
  }
  return emptyString;
}

const String &WebServer::responseHeader(int i) const {
  RequestArgument *current = _responseHeaders;
  while (current && i--) {
    current = current->next;
  }
  return current ? current->value : emptyString;
}

const String &WebServer::responseHeaderName(int i) const {
  RequestArgument *current = _responseHeaders;
  while (current && i--) {
    current = current->next;
  }
  return current ? current->key : emptyString;
}

bool WebServer::hasResponseHeader(const String &name) const {
  return header(name).length() > 0;
}

int WebServer::clientContentLength() const {
  return _clientContentLength;
}

const String WebServer::version() const {
  String v;
  v.reserve(8);
  v.concat(F("HTTP/1."));
  v.concat(_currentVersion);
  return v;
}
int WebServer::responseCode() const {
  return _responseCode;
}
int WebServer::responseHeaders() const {
  return _responseHeaderCount;
}

WebServer &WebServer::addMiddleware(Middleware *middleware) {
  if (!_chain) {
    _chain = new MiddlewareChain();
  }
  _chain->addMiddleware(middleware);
  return *this;
}

WebServer &WebServer::addMiddleware(Middleware::Function fn) {
  if (!_chain) {
    _chain = new MiddlewareChain();
  }
  _chain->addMiddleware(fn);
  return *this;
}

WebServer &WebServer::removeMiddleware(Middleware *middleware) {
  if (_chain) {
    _chain->removeMiddleware(middleware);
  }
  return *this;
}
