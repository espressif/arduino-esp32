/*
  ESPWebServer.cpp - Dead simple web-server.
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
#include <libb64/cencode.h>
#include "WiFiServer.h"
#include "WiFiClient.h"
#include "ESPWebServer.h"
#include "FS.h"
#include "detail/RequestHandlersImpl.h"

//#define DEBUG_ESP_HTTP_SERVER
#ifdef DEBUG_ESP_PORT
#define DEBUG_OUTPUT DEBUG_ESP_PORT
#else
#define DEBUG_OUTPUT Serial
#endif

const char * AUTHORIZATION_HEADER = "Authorization";

ESPWebServer::ESPWebServer(IPAddress addr, int port)
: _server(addr, port)
, _currentMethod(HTTP_ANY)
, _currentVersion(0)
, _currentStatus(HC_NONE)
, _statusChange(0)
, _currentHandler(0)
, _firstHandler(0)
, _lastHandler(0)
, _currentArgCount(0)
, _currentArgs(0)
, _headerKeysCount(0)
, _currentHeaders(0)
, _contentLength(0)
, _chunked(false)
{
}

ESPWebServer::ESPWebServer(int port)
: _server(port)
, _currentMethod(HTTP_ANY)
, _currentVersion(0)
, _currentStatus(HC_NONE)
, _statusChange(0)
, _currentHandler(0)
, _firstHandler(0)
, _lastHandler(0)
, _currentArgCount(0)
, _currentArgs(0)
, _headerKeysCount(0)
, _currentHeaders(0)
, _contentLength(0)
, _chunked(false)
{
}

ESPWebServer::~ESPWebServer() {
  if (_currentHeaders)
    delete[]_currentHeaders;
  _headerKeysCount = 0;
  RequestHandler* handler = _firstHandler;
  while (handler) {
    RequestHandler* next = handler->next();
    delete handler;
    handler = next;
  }
  close();
}

void ESPWebServer::begin() {
  _currentStatus = HC_NONE;
  _server.begin();
  if(!_headerKeysCount)
    collectHeaders(0, 0);
}

bool ESPWebServer::authenticate(const char * username, const char * password){
  if(hasHeader(AUTHORIZATION_HEADER)){
    String authReq = header(AUTHORIZATION_HEADER);
    if(authReq.startsWith("Basic")){
      authReq = authReq.substring(6);
      authReq.trim();
      char toencodeLen = strlen(username)+strlen(password)+1;
      char *toencode = new char[toencodeLen + 1];
      if(toencode == NULL){
        authReq = String();
        return false;
      }
      char *encoded = new char[base64_encode_expected_len(toencodeLen)+1];
      if(encoded == NULL){
        authReq = String();
        delete[] toencode;
        return false;
      }
      sprintf(toencode, "%s:%s", username, password);
      if(base64_encode_chars(toencode, toencodeLen, encoded) > 0 && authReq.equals(encoded)){
        authReq = String();
        delete[] toencode;
        delete[] encoded;
        return true;
      }
      delete[] toencode;
      delete[] encoded;
    }
    authReq = String();
  }
  return false;
}

void ESPWebServer::requestAuthentication(){
  sendHeader("WWW-Authenticate", "Basic realm=\"Login Required\"");
  send(401);
}

void ESPWebServer::on(const String &uri, ESPWebServer::THandlerFunction handler) {
  on(uri, HTTP_ANY, handler);
}

void ESPWebServer::on(const String &uri, HTTPMethod method, ESPWebServer::THandlerFunction fn) {
  on(uri, method, fn, _fileUploadHandler);
}

void ESPWebServer::on(const String &uri, HTTPMethod method, ESPWebServer::THandlerFunction fn, ESPWebServer::THandlerFunction ufn) {
  _addRequestHandler(new FunctionRequestHandler(fn, ufn, uri, method));
}

void ESPWebServer::addHandler(RequestHandler* handler) {
    _addRequestHandler(handler);
}

void ESPWebServer::_addRequestHandler(RequestHandler* handler) {
    if (!_lastHandler) {
      _firstHandler = handler;
      _lastHandler = handler;
    }
    else {
      _lastHandler->next(handler);
      _lastHandler = handler;
    }
}

void ESPWebServer::serveStatic(const char* uri, FS& fs, const char* path, const char* cache_header) {
    _addRequestHandler(new StaticRequestHandler(fs, path, uri, cache_header));
}

void ESPWebServer::handleClient() {
  if (_currentStatus == HC_NONE) {
    WiFiClient client = _server.available();
    if (!client) {
      return;
    }

#ifdef DEBUG_ESP_HTTP_SERVER
    DEBUG_OUTPUT.println("New client");
#endif

    _currentClient = client;
    _currentStatus = HC_WAIT_READ;
    _statusChange = millis();
  }

  if (!_currentClient.connected()) {
    _currentClient = WiFiClient();
    _currentStatus = HC_NONE;
    return;
  }

  // Wait for data from client to become available
  if (_currentStatus == HC_WAIT_READ) {
    if (!_currentClient.available()) {
      if (millis() - _statusChange > HTTP_MAX_DATA_WAIT) {
        _currentClient = WiFiClient();
        _currentStatus = HC_NONE;
      }
      yield();
      return;
    }

    if (!_parseRequest(_currentClient)) {
      _currentClient = WiFiClient();
      _currentStatus = HC_NONE;
      return;
    }
    _currentClient.setTimeout(HTTP_MAX_SEND_WAIT);
    _contentLength = CONTENT_LENGTH_NOT_SET;
    _handleRequest();

    if (!_currentClient.connected()) {
      _currentClient = WiFiClient();
      _currentStatus = HC_NONE;
      return;
    } else {
      _currentStatus = HC_WAIT_CLOSE;
      _statusChange = millis();
      return;
    }
  }

  if (_currentStatus == HC_WAIT_CLOSE) {
    if (millis() - _statusChange > HTTP_MAX_CLOSE_WAIT) {
      _currentClient = WiFiClient();
      _currentStatus = HC_NONE;
    } else {
      yield();
      return;
    }
  }
}

void ESPWebServer::close() {
  #ifdef ESP32
  _server.end();
  #else
  _server.close();
  #endif
}

void ESPWebServer::stop() {
  close();
}

void ESPWebServer::sendHeader(const String& name, const String& value, bool first) {
  String headerLine = name;
  headerLine += ": ";
  headerLine += value;
  headerLine += "\r\n";

  if (first) {
    _responseHeaders = headerLine + _responseHeaders;
  }
  else {
    _responseHeaders += headerLine;
  }
}

void ESPWebServer::setContentLength(size_t contentLength) {
    _contentLength = contentLength;
}

void ESPWebServer::_prepareHeader(String& response, int code, const char* content_type, size_t contentLength) {
    response = "HTTP/1."+String(_currentVersion)+" ";
    response += String(code);
    response += " ";
    response += _responseCodeToString(code);
    response += "\r\n";

    if (!content_type)
        content_type = "text/html";

    sendHeader("Content-Type", content_type, true);
    if (_contentLength == CONTENT_LENGTH_NOT_SET) {
        sendHeader("Content-Length", String(contentLength));
    } else if (_contentLength != CONTENT_LENGTH_UNKNOWN) {
        sendHeader("Content-Length", String(_contentLength));
    } else if(_contentLength == CONTENT_LENGTH_UNKNOWN && _currentVersion){ //HTTP/1.1 or above client
      //let's do chunked
      _chunked = true;
      sendHeader("Accept-Ranges","none");
      sendHeader("Transfer-Encoding","chunked");
    }
    sendHeader("Connection", "close");

    response += _responseHeaders;
    response += "\r\n";
    _responseHeaders = String();
}

void ESPWebServer::send(int code, const char* content_type, const String& content) {
    String header;
    // Can we asume the following?
    //if(code == 200 && content.length() == 0 && _contentLength == CONTENT_LENGTH_NOT_SET)
    //  _contentLength = CONTENT_LENGTH_UNKNOWN;
    _prepareHeader(header, code, content_type, content.length());
    _currentClient.write(header.c_str(), header.length());
    if(content.length())
      sendContent(content);
}

void ESPWebServer::send_P(int code, PGM_P content_type, PGM_P content) {
    size_t contentLength = 0;

    if (content != NULL) {
        contentLength = strlen_P(content);
    }

    String header;
    char type[64];
    memccpy_P((void*)type, (PGM_VOID_P)content_type, 0, sizeof(type));
    _prepareHeader(header, code, (const char* )type, contentLength);
    _currentClient.write(header.c_str(), header.length());
    sendContent_P(content);
}

void ESPWebServer::send_P(int code, PGM_P content_type, PGM_P content, size_t contentLength) {
    String header;
    char type[64];
    memccpy_P((void*)type, (PGM_VOID_P)content_type, 0, sizeof(type));
    _prepareHeader(header, code, (const char* )type, contentLength);
    sendContent(header);
    sendContent_P(content, contentLength);
}

void ESPWebServer::send(int code, char* content_type, const String& content) {
  send(code, (const char*)content_type, content);
}

void ESPWebServer::send(int code, const String& content_type, const String& content) {
  send(code, (const char*)content_type.c_str(), content);
}

void ESPWebServer::sendContent(const String& content) {
  const char * footer = "\r\n";
  size_t len = content.length();
  if(_chunked) {
    char * chunkSize = (char *)malloc(11);
    if(chunkSize){
      sprintf(chunkSize, "%x%s", len, footer);
      _currentClient.write(chunkSize, strlen(chunkSize));
      free(chunkSize);
    }
  }
  _currentClient.write(content.c_str(), len);
  if(_chunked){
    _currentClient.write(footer, 2);
  }
}

void ESPWebServer::sendContent_P(PGM_P content) {
  sendContent_P(content, strlen_P(content));
}

void ESPWebServer::sendContent_P(PGM_P content, size_t size) {
  const char * footer = "\r\n";
  if(_chunked) {
    char * chunkSize = (char *)malloc(11);
    if(chunkSize){
      sprintf(chunkSize, "%x%s", size, footer);
      _currentClient.write(chunkSize, strlen(chunkSize));
      free(chunkSize);
    }
  }
  _currentClient.write_P(content, size);
  if(_chunked){
    _currentClient.write(footer, 2);
  }
}


String ESPWebServer::arg(String name) {
  for (int i = 0; i < _currentArgCount; ++i) {
    if ( _currentArgs[i].key == name )
      return _currentArgs[i].value;
  }
  return String();
}

String ESPWebServer::arg(int i) {
  if (i < _currentArgCount)
    return _currentArgs[i].value;
  return String();
}

String ESPWebServer::argName(int i) {
  if (i < _currentArgCount)
    return _currentArgs[i].key;
  return String();
}

int ESPWebServer::args() {
  return _currentArgCount;
}

bool ESPWebServer::hasArg(String  name) {
  for (int i = 0; i < _currentArgCount; ++i) {
    if (_currentArgs[i].key == name)
      return true;
  }
  return false;
}


String ESPWebServer::header(String name) {
  for (int i = 0; i < _headerKeysCount; ++i) {
    if (_currentHeaders[i].key.equalsIgnoreCase(name))
      return _currentHeaders[i].value;
  }
  return String();
}

void ESPWebServer::collectHeaders(const char* headerKeys[], const size_t headerKeysCount) {
  _headerKeysCount = headerKeysCount + 1;
  if (_currentHeaders)
     delete[]_currentHeaders;
  _currentHeaders = new RequestArgument[_headerKeysCount];
  _currentHeaders[0].key = AUTHORIZATION_HEADER;
  for (int i = 1; i < _headerKeysCount; i++){
    _currentHeaders[i].key = headerKeys[i-1];
  }
}

String ESPWebServer::header(int i) {
  if (i < _headerKeysCount)
    return _currentHeaders[i].value;
  return String();
}

String ESPWebServer::headerName(int i) {
  if (i < _headerKeysCount)
    return _currentHeaders[i].key;
  return String();
}

int ESPWebServer::headers() {
  return _headerKeysCount;
}

bool ESPWebServer::hasHeader(String name) {
  for (int i = 0; i < _headerKeysCount; ++i) {
    if ((_currentHeaders[i].key.equalsIgnoreCase(name)) &&  (_currentHeaders[i].value.length() > 0))
      return true;
  }
  return false;
}

String ESPWebServer::hostHeader() {
  return _hostHeader;
}

void ESPWebServer::onFileUpload(THandlerFunction fn) {
  _fileUploadHandler = fn;
}

void ESPWebServer::onNotFound(THandlerFunction fn) {
  _notFoundHandler = fn;
}

void ESPWebServer::_handleRequest() {
  bool handled = false;
  if (!_currentHandler){
#ifdef DEBUG_ESP_HTTP_SERVER
    DEBUG_OUTPUT.println("request handler not found");
#endif
  }
  else {
    handled = _currentHandler->handle(*this, _currentMethod, _currentUri);
#ifdef DEBUG_ESP_HTTP_SERVER
    if (!handled) {
      DEBUG_OUTPUT.println("request handler failed to handle request");
    }
#endif
  }

  if (!handled) {
    if(_notFoundHandler) {
      _notFoundHandler();
    }
    else {
      send(404, "text/plain", String("Not found: ") + _currentUri);
    }
  }

  _currentUri = String();
}

String ESPWebServer::_responseCodeToString(int code) {
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
    default:  return "";
  }
}
