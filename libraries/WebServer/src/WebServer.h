/*
  WebServer.h - Dead simple web-server.
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

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <functional>
#include <memory>
#include "FS.h"
#include "Network.h"
#include "HTTP_Method.h"
#include "Uri.h"

enum HTTPUploadStatus {
  UPLOAD_FILE_START,
  UPLOAD_FILE_WRITE,
  UPLOAD_FILE_END,
  UPLOAD_FILE_ABORTED
};
enum HTTPRawStatus {
  RAW_START,
  RAW_WRITE,
  RAW_END,
  RAW_ABORTED
};
enum HTTPClientStatus {
  HC_NONE,
  HC_WAIT_READ,
  HC_WAIT_CLOSE
};
enum HTTPAuthMethod {
  BASIC_AUTH,
  DIGEST_AUTH,
  OTHER_AUTH
};

#define HTTP_DOWNLOAD_UNIT_SIZE 1436

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 1436
#endif

#ifndef HTTP_RAW_BUFLEN
#define HTTP_RAW_BUFLEN 1436
#endif

#define HTTP_MAX_DATA_WAIT      5000  //ms to wait for the client to send the request
#define HTTP_MAX_POST_WAIT      5000  //ms to wait for POST data to arrive
#define HTTP_MAX_SEND_WAIT      5000  //ms to wait for data chunk to be ACKed
#define HTTP_MAX_CLOSE_WAIT     5000  //ms to wait for the client to close the connection
#define HTTP_MAX_BASIC_AUTH_LEN 256   // maximum length of a basic Auth base64 encoded username:password string

// Request limits. All of these can be overridden from the build (e.g. with
// -DWEBSERVER_MAX_URI_LEN=8192) to trade robustness against memory use.

#ifndef WEBSERVER_MAX_URI_LEN
#define WEBSERVER_MAX_URI_LEN 2048  // max request-target length; longer requests get 414. 0 disables the check
#endif

#ifndef WEBSERVER_MAX_QUERY_ARGS
#define WEBSERVER_MAX_QUERY_ARGS 256  // max key=value args taken from a query string or urlencoded body
#endif

#ifndef WEBSERVER_MAX_MULTIPART_SKIP_LINES
#define WEBSERVER_MAX_MULTIPART_SKIP_LINES 64  // unrecognized multipart lines tolerated before giving up
#endif

#ifndef WEBSERVER_MAX_LINE_LEN
#define WEBSERVER_MAX_LINE_LEN 4096  // max length of a single protocol line (request line, header, multipart header)
#endif

#ifndef WEBSERVER_MAX_POST_ARG_LEN
#define WEBSERVER_MAX_POST_ARG_LEN 16384  // max length of one line of a non-file multipart field value
#endif

// Time limits on receiving a request. Without them a peer that trickles one byte
// per stream timeout keeps handleClient() from returning for as long as it likes.

#ifndef WEBSERVER_MAX_LINE_WAIT
#define WEBSERVER_MAX_LINE_WAIT 5000  // ms for a single protocol line to arrive complete. 0 disables the check
#endif

#ifndef WEBSERVER_MAX_HEADER_WAIT
#define WEBSERVER_MAX_HEADER_WAIT 10000  // ms for the request line and all headers together. 0 disables the check
#endif

#define CONTENT_LENGTH_UNKNOWN ((size_t) - 1)
#define CONTENT_LENGTH_NOT_SET ((size_t) - 2)

class WebServer;

typedef struct {
  HTTPUploadStatus status;
  String filename;
  String name;
  String type;
  size_t totalSize;    // file size
  size_t currentSize;  // size of data currently in buf
  uint8_t buf[HTTP_UPLOAD_BUFLEN];
} HTTPUpload;

typedef struct {
  HTTPRawStatus status;
  size_t totalSize;    // content size
  size_t currentSize;  // size of data currently in buf
  uint8_t buf[HTTP_RAW_BUFLEN];
  void *data;  // additional data
} HTTPRaw;

#include "middleware/Middleware.h"
#include "detail/RequestHandler.h"

namespace fs {
class FS;
}

class WebServer {
public:
  WebServer(IPAddress addr, int port = 80);
  WebServer(int port = 80);
  virtual ~WebServer();

  virtual void begin();
  virtual void begin(uint16_t port);
  virtual void handleClient();

  virtual void close();
  void stop();

  const String AuthTypeDigest = F("Digest");
  const String AuthTypeBasic = F("Basic");

  void chunkResponseBegin(const char *contentType = "text/plain");
  void chunkWrite(const char *data, size_t length);
  void chunkResponseEnd();

  /* Callbackhandler for authentication. The extra parameters depend on the
   * HTTPAuthMethod mode:
   *
   * BASIC_AUTH         enteredUsernameOrReq	contains the username entered by the user
   *                    param[0]		          password entered (in the clear)
   *                    param[1]		          authentication realm.
   *
   * To return - the password the user entered password is compared to. Or Null on fail.
   *
   * DIGEST_AUTH        enteredUsernameOrReq    contains the username entered by the user
   *                    param[0]                autenticaiton realm
   *                    param[1]                authentication URI
   *
   * To return - the password of which the digest will be based on for comparison. Or NULL
   * to fail.
   *
   * OTHER_AUTH         enteredUsernameOrReq    rest of the auth line.
   *                    params                  empty array
   *
   * To return - NULL to fail; or any string.
   */
  typedef std::function<String *(HTTPAuthMethod mode, String enteredUsernameOrReq, String extraParams[])> THandlerFunctionAuthCheck;

  bool authenticate(THandlerFunctionAuthCheck fn);
  bool authenticate(const char *username, const char *password);
  bool authenticateBasicSHA1(const char *_username, const char *_sha1AsBase64orHex);

  void requestAuthentication(HTTPAuthMethod mode = BASIC_AUTH, const char *realm = NULL, const String &authFailMsg = String(""));

  typedef std::function<void(void)> THandlerFunction;
  typedef std::function<bool(WebServer &server)> FilterFunction;
  RequestHandler &on(const Uri &uri, THandlerFunction fn);
  RequestHandler &on(const Uri &uri, HTTPMethod method, THandlerFunction fn);
  RequestHandler &on(const Uri &uri, HTTPMethod method, THandlerFunction fn, THandlerFunction ufn);  //ufn handles file uploads
  bool removeRoute(const char *uri);
  bool removeRoute(const char *uri, HTTPMethod method);
  bool removeRoute(const String &uri);
  bool removeRoute(const String &uri, HTTPMethod method);
  void addHandler(RequestHandler *handler);
  bool removeHandler(RequestHandler *handler);
  void serveStatic(const char *uri, fs::FS &fs, const char *path, const char *cache_header = NULL);
  void onNotFound(THandlerFunction fn);     //called when handler is not assigned
  void onFileUpload(THandlerFunction ufn);  //handle file uploads

  WebServer &addMiddleware(Middleware *middleware);
  WebServer &addMiddleware(Middleware::Function fn);
  WebServer &removeMiddleware(Middleware *middleware);

  String uri() const {
    return _currentUri;
  }
  HTTPMethod method() const {
    return _currentMethod;
  }
  virtual NetworkClient &client() {
    return _currentClient;
  }
  // A handler registered with an upload callback also receives raw
  // (non-multipart) bodies, so upload() may be reached with no upload in
  // progress and vice versa. Report an inactive object rather than
  // dereferencing null; use hasUpload()/hasRaw() to tell the contexts apart.
  HTTPUpload &upload() {
    if (!_currentUpload) {
      _currentUpload.reset(new HTTPUpload());
      _currentUpload->status = UPLOAD_FILE_ABORTED;
    }
    return *_currentUpload;
  }
  HTTPRaw &raw() {
    if (!_currentRaw) {
      _currentRaw.reset(new HTTPRaw());
      _currentRaw->status = RAW_ABORTED;
    }
    return *_currentRaw;
  }

  bool hasUpload() const {
    return static_cast<bool>(_currentUpload);
  }
  bool hasRaw() const {
    return static_cast<bool>(_currentRaw);
  }

  String pathArg(unsigned int i) const;                                         // get request path argument by number
  String arg(const String &name) const;                                         // get request argument value by name
  String arg(int i) const;                                                      // get request argument value by number
  String argName(int i) const;                                                  // get request argument name by number
  int args() const;                                                             // get arguments count
  bool hasArg(const String &name) const;                                        // check if argument exists
  void collectHeaders(const char *headerKeys[], const size_t headerKeysCount);  // set the request headers to collect
  void collectAllHeaders();                                                     // collect all request headers
  String header(const String &name) const;                                      // get request header value by name
  String header(int i) const;                                                   // get request header value by number
  String headerName(int i) const;                                               // get request header name by number
  int headers() const;                                                          // get header count
  bool hasHeader(const String &name) const;                                     // check if header exists

  int clientContentLength() const;  // return "content-length" of incoming HTTP header from "_currentClient"
  const String version() const;     // get the HTTP version string
  String hostHeader() const;        // get request host header if available or empty String if not

  int responseCode() const;                          // get the HTTP response code set
  int responseHeaders() const;                       // get the HTTP response headers count
  const String &responseHeader(String name) const;   // get the HTTP response header value by name
  const String &responseHeader(int i) const;         // get the HTTP response header value by number
  const String &responseHeaderName(int i) const;     // get the HTTP response header name by number
  bool hasResponseHeader(const String &name) const;  // check if response header exists

  // send response to the client
  // code - HTTP response code, can be 200 or 404
  // content_type - HTTP content type, like "text/plain" or "image/png"
  // content - actual content body
  void send(int code, const char *content_type = NULL, const String &content = String(""));
  void send(int code, char *content_type, const String &content);
  void send(int code, const String &content_type, const String &content);
  void send(int code, const char *content_type, const char *content);
  void send(int code, const char *content_type, Stream &stream, size_t content_length = 0);

  void send_P(int code, PGM_P content_type, PGM_P content);
  void send_P(int code, PGM_P content_type, PGM_P content, size_t contentLength);

  void enableDelay(boolean value);
  void enableCORS(boolean value = true);
  void enableCrossOrigin(boolean value = true);
  typedef std::function<String(FS &fs, const String &fName)> ETagFunction;
  void enableETag(bool enable, ETagFunction fn = nullptr);

  void setContentLength(const size_t contentLength);
  void sendHeader(const String &name, const String &value, bool first = false);
  void sendContent(const String &content);
  void sendContent(const char *content, size_t contentLength);
  void sendContent_P(PGM_P content);
  void sendContent_P(PGM_P content, size_t size);

  static String urlDecode(const String &text);

  template<typename T> size_t streamFile(T &file, const String &contentType, const int code = 200) {
    _streamFileCore(file.size(), file.name(), contentType, code);
    return _currentClient.write(file);
  }

  bool _eTagEnabled = false;
  ETagFunction _eTagFunction = nullptr;

  static String responseCodeToString(int code);

private:
  bool _chunkedResponseActive = false;
  NetworkClient _chunkedClient;  // Store by value, no dangling pointer

protected:
  virtual size_t _currentClientWrite(const char *b, size_t l) {
    return _currentClient.write(b, l);
  }
  virtual size_t _currentClientWrite_P(PGM_P b, size_t l) {
    return _currentClient.write_P(b, l);
  }
  void _addRequestHandler(RequestHandler *handler);
  bool _removeRequestHandler(RequestHandler *handler);
  bool _handleRequest();
  void _finalizeResponse();
  bool _parseRequest(NetworkClient &client);
  void _parseArguments(const String &data);
  bool _parseForm(NetworkClient &client, const String &boundary, uint32_t len);
  bool _parseFormUploadAborted();
  void _uploadWriteByte(uint8_t b);
  int _uploadReadByte(NetworkClient &client);
  void _prepareHeader(String &response, int code, const char *content_type, size_t contentLength);
  bool _collectHeader(const char *headerName, const char *headerValue);

  void _streamFileCore(const size_t fileSize, const String &fileName, const String &contentType, const int code = 200);

  String _getRandomHexString();
  // for extracting Auth parameters
  String _extractParam(String &authReq, const String &param, const char delimit = '"');

  void _clearResponseHeaders();
  void _clearRequestHeaders();

  struct RequestArgument {
    String key;
    String value;
    RequestArgument *next;
  };

  boolean _corsEnabled = false;
  NetworkServer _server;

  NetworkClient _currentClient;
  HTTPMethod _currentMethod = HTTP_ANY;
  String _currentUri;
  uint8_t _currentVersion = 0;
  HTTPClientStatus _currentStatus = HC_NONE;
  unsigned long _statusChange = 0;
  boolean _nullDelay = true;

  RequestHandler *_currentHandler = nullptr;
  RequestHandler *_firstHandler = nullptr;
  RequestHandler *_lastHandler = nullptr;
  THandlerFunction _notFoundHandler = nullptr;
  THandlerFunction _fileUploadHandler = nullptr;

  int _currentArgCount = 0;
  RequestArgument *_currentArgs = nullptr;
  int _postArgsLen = 0;
  RequestArgument *_postArgs = nullptr;

  std::unique_ptr<HTTPUpload> _currentUpload;
  std::unique_ptr<HTTPRaw> _currentRaw;

  int _headerKeysCount = 0;
  RequestArgument *_currentHeaders = nullptr;
  size_t _contentLength = 0;
  int _clientContentLength = 0;  // "Content-Length" from header of incoming POST or GET request
  RequestArgument *_responseHeaders = nullptr;

  String _hostHeader;
  bool _chunked = false;

  String _snonce;  // Store noance and opaque for future comparison
  String _sopaque;
  String _srealm;  // Store the Auth realm between Calls

  int _responseHeaderCount = 0;
  int _responseCode = 0;
  bool _collectAllHeaders = false;
  MiddlewareChain *_chain = nullptr;
};

#endif  //ESP8266WEBSERVER_H
