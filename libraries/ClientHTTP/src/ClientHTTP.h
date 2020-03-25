/*
 *   ClientHTTP.h - A HTTP Client class using the standard Client and Stream interface,
 *                  while handling the request and response headers and chunked
 *                  Transfer-Encoding 'under the hood'
 *                  
 *   Copyright (C) 2020  Jeroen Döll
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// TO DO
// redirect to a different host
// Possibly add gzip stream

#pragma once

#include <Arduino.h>
#include <Client.h>
#include <map>

// If SUPPORT_REDIRECTS is not defined, HTTP requests are not automatically redirected if the server sends 
// a 'Location: ' response header. On an ESP32 this saves about 700 bytes of code. Redirects can still be handled
// by the sketch if redirects are not supported by the client.
#define SUPPORT_REDIRECTS

#if defined(ESP8266)
#ifdef DEBUG_ESP_PORT
#define DEBUG_HTTP_MSG(...) DEBUG_ESP_PORT.printf( __VA_ARGS__ )
#else
#define DEBUG_HTTP_MSG(...)
#endif
#endif // defined(ESP8266)

/*
 * class Sizer is a helper class to determine the payload size
 * Usage:
 *  - print all payload to a Sizer instance first
 *  - read the payload size with getSize()
 *  - use this size in httpClient::POST()
 *  - print the exact payload used in Sizer to the httpClient
 * See examples for an example
 */
class Sizer: public Print {
public:
                            Sizer(): _size(0) {}

  virtual size_t            write(uint8_t) {
    _size++;

    return 1;
  }

  virtual size_t            write(const uint8_t *buf, size_t size) {
    _size += size;

    return size;
  }

  void                      reset() {
    _size = 0;
  }

  size_t                    getSize() {
    return _size;
  }
private:
  size_t                    _size;
};

// ::readResponseHeaders() uses a small buffer on the stack. A full header line must fit into this buffer. HTTP does not specifiy a maximum size for headers
#define RESPONSE_HEADER_LINE_SIZE (128)

// ::prinTo() uses a small buffer on the stack
#define PRINT_BUFFER_SIZE (64)

class ClientHTTP: public Client {
public:
                            ClientHTTP(Client   &client);

                            ~ClientHTTP();

  // HTTP codes see RFC7231
  typedef enum {
    HTTP_ERROR = -1,
    REDIRECT_LIMIT_REACHED = -2,
    REDIRECT_CONNECTION_FAILED = -3,
    REDIRECT_UNKOWN_METHOD = -4,
    REDIRECT_FAILURE = -5,
    RESPONSE_HEADER_TIMEOUT = -6,
    OUT_OF_MEMORY = -7,
    CHUNK_HEADER_TIMEOUT = -8,
    CHUNK_HEADER_INVALID = -9,
    CONTINUE = 100,
    SWITCHING_PROTOCOLS = 101,
    PROCESSING = 102,
    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    NON_AUTHORITATIVE_INFORMATION = 203,
    NO_CONTENT = 204,
    RESET_CONTENT = 205,
    PARTIAL_CONTENT = 206,
    MULTI_STATUS = 207,
    ALREADY_REPORTED = 208,
    IM_USED = 226,
    MULTIPLE_CHOICES = 300,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    SEE_OTHER = 303,
    NOT_MODIFIED = 304,
    USE_PROXY = 305,
    TEMPORARY_REDIRECT = 307,
    PERMANENT_REDIRECT = 308,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    PAYMENT_REQUIRED = 402,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    NOT_ACCEPTABLE = 406,
    PROXY_AUTHENTICATION_REQUIRED = 407,
    REQUEST_TIMEOUT = 408,
    CONFLICT = 409,
    GONE = 410,
    LENGTH_REQUIRED = 411,
    PRECONDITION_FAILED = 412,
    PAYLOAD_TOO_LARGE = 413,
    URI_TOO_LONG = 414,
    UNSUPPORTED_MEDIA_TYPE = 415,
    RANGE_NOT_SATISFIABLE = 416,
    EXPECTATION_FAILED = 417,
    MISDIRECTED_REQUEST = 421,
    UNPROCESSABLE_ENTITY = 422,
    LOCKED = 423,
    FAILED_DEPENDENCY = 424,
    UPGRADE_REQUIRED = 426,
    PRECONDITION_REQUIRED = 428,
    TOO_MANY_REQUESTS = 429,
    REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
    GATEWAY_TIMEOUT = 504,
    HTTP_VERSION_NOT_SUPPORTED = 505,
    VARIANT_ALSO_NEGOTIATES = 506,
    INSUFFICIENT_STORAGE = 507,
    LOOP_DETECTED = 508,
    NOT_EXTENDED = 510,
    NETWORK_AUTHENTICATION_REQUIRED = 511
  } http_code_t;

  virtual int               connect(IPAddress ip, uint16_t port);

  virtual int               connect(const char * host, uint16_t port);

  virtual size_t            write(uint8_t);

  virtual size_t            write(const uint8_t *buf, size_t size);

  virtual int               available();

  virtual int               read();

  virtual int               read(uint8_t *buf, size_t size);

  virtual int               peek();

  virtual void              flush();

  virtual void              stop();

  virtual uint8_t           connected();

  virtual operator          bool();

  // Handle redirections as stated in RFC document:
  // https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
  typedef enum {
    DISABLE,
    STRICT,
    FORCE
  } follow_redirects_t;
  follow_redirects_t        redirects = ClientHTTP::STRICT;

  uint16_t                  redirectLimit = 10;

  bool                      GET(const char * uri);

  bool                      HEAD(const char * uri);

  bool                      POST(const char * uri, size_t contentLength = 0);

  bool                      PUT(const char * uri, size_t contentLength = 0);

  bool                      PATCH(const char * uri, size_t contentLength = 0);

  bool                      DELETE(const char * uri, size_t contentLength = 0);

  http_code_t               status();

  size_t                    leftToRead();

  size_t                    leftToWrite();

  void                      printTo(Print &printer);

//  std::map<char *, char *>  requestHeaders;
  std::map<std::string, std::string>  requestHeaders;

  // Helper struct for case insensitive comparison of keys in the response headers map
  struct cmp_str {
//    bool operator()(char const *a, char const *b) const {
//      return strcasecmp(a, b) < 0;
//    }
    bool operator()(const std::string & a, const std::string & b) const {
      return strcasecmp(a.c_str(), b.c_str()) < 0;
    }
  };
//  std::map<char *, char *, cmp_str>  responseHeaders;
  std::map<std::string, std::string, cmp_str>  responseHeaders;

  typedef enum {
    HTTP_10,
    HTTP_11
  } http_version_t;
  http_version_t            httpVersion = HTTP_11;

protected:
  bool                      writeRequestHeaders(const char * method, const char * uri, size_t contentLength = 0);

  http_code_t               readResponseHeaders();

/*
  const char *              errorToString(http_code_t error);

  http_code_t               returnError(http_code_t error);
*/

  Client &                  _client;

  IPAddress                 _ip;
  char *                    _host;
  uint16_t                  _port;

  // for GET and HEAD status changes from CLIENT_READY -> CLIENT_RESPONSE_HEADER -> CLIENT_RESPONSE_PAYLOAD -> CLIENT_READY
  // for POST, PUT and PATCH status changes from CLIENT_READY -> CLIENT_REQUEST_HEADER_SENT -> CLIENT_RESPONSE_HEADER -> CLIENT_RESPONSE_PAYLOAD -> CLIENT_READY
  typedef enum {
    CLIENT_READY,                 // No pending HTTP communication
    CLIENT_REQUEST_HEADER_SENT,   // Request header sent, ready to write payload that is to be send to the server via stream
    CLIENT_RESPONSE_HEADER,       // Ready to receive response header from server
    CLIENT_RESPONSE_PAYLOAD,      // Ready to read payload sent by server from stream
  } client_state_t;
  client_state_t            _state;

  bool                      _chunked;
  size_t                    _leftToRead;

  size_t                    _leftToWrite;
  bool                      _firstChunkToWrite;

  char *                    _location;
  uint16_t                  _redirectCount;
  char                      _originalMethod[16];
};
