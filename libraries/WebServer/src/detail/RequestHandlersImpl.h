#ifndef REQUESTHANDLERSIMPL_H
#define REQUESTHANDLERSIMPL_H

#include "RequestHandler.h"
#include "mimetable.h"
#include "WString.h"
#include "Uri.h"
#include <MD5Builder.h>
#include <base64.h>

using namespace mime;

class FunctionRequestHandler : public RequestHandler {
public:
  FunctionRequestHandler(WebServer::THandlerFunction fn, WebServer::THandlerFunction ufn, const Uri &uri, HTTPMethod method)
    : _fn(fn), _ufn(ufn), _uri(uri.clone()), _method(method) {
    _uri->initPathArgs(pathArgs);
  }

  ~FunctionRequestHandler() {
    delete _uri;
  }

  bool canHandle(HTTPMethod requestMethod, const String &requestUri) override {
    if (_method != HTTP_ANY && _method != requestMethod) {
      return false;
    }

    return _uri->canHandle(requestUri, pathArgs);
  }

  bool canUpload(const String &requestUri) override {
    if (!_ufn || !canHandle(HTTP_POST, requestUri)) {
      return false;
    }

    return true;
  }

  bool canRaw(const String &requestUri) override {
    if (!_ufn || _method == HTTP_GET) {
      return false;
    }

    return true;
  }

  bool canHandle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    if (_method != HTTP_ANY && _method != requestMethod) {
      return false;
    }

    return _uri->canHandle(requestUri, pathArgs) && (_filter != NULL ? _filter(server) : true);
  }

  bool canUpload(WebServer &server, const String &requestUri) override {
    if (!_ufn || !canHandle(server, HTTP_POST, requestUri)) {
      return false;
    }

    return true;
  }

  bool canRaw(WebServer &server, const String &requestUri) override {
    if (!_ufn || _method == HTTP_GET || (_filter != NULL ? _filter(server) == false : false)) {
      return false;
    }

    return true;
  }

  bool handle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    if (!canHandle(server, requestMethod, requestUri)) {
      return false;
    }

    _fn();
    return true;
  }

  void upload(WebServer &server, const String &requestUri, HTTPUpload &upload) override {
    (void)upload;
    if (canUpload(server, requestUri)) {
      _ufn();
    }
  }

  void raw(WebServer &server, const String &requestUri, HTTPRaw &raw) override {
    (void)raw;
    if (canRaw(server, requestUri)) {
      _ufn();
    }
  }

  FunctionRequestHandler &setFilter(WebServer::FilterFunction filter) {
    _filter = filter;
    return *this;
  }

protected:
  WebServer::THandlerFunction _fn;
  WebServer::THandlerFunction _ufn;
  // _filter should return 'true' when the request should be handled
  // and 'false' when the request should be ignored
  WebServer::FilterFunction _filter;
  Uri *_uri;
  HTTPMethod _method;
};

class StaticRequestHandler : public RequestHandler {
public:
  StaticRequestHandler(FS &fs, const char *path, const char *uri, const char *cache_header) : _fs(fs), _uri(uri), _path(path), _cache_header(cache_header) {
    File f = fs.open(path);
    _isFile = (f && (!f.isDirectory()));
    log_v(
      "StaticRequestHandler: path=%s uri=%s isFile=%d, cache_header=%s\r\n", path, uri, _isFile, cache_header ? cache_header : ""
    );  // issue 5506 - cache_header can be nullptr
    _baseUriLength = _uri.length();
  }

  bool canHandle(HTTPMethod requestMethod, const String &requestUri) override {
    if (requestMethod != HTTP_GET) {
      return false;
    }

    if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri)) {
      return false;
    }

    return true;
  }

  bool canHandle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    if (requestMethod != HTTP_GET) {
      return false;
    }

    if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri)) {
      return false;
    }

    if (_filter != NULL ? _filter(server) == false : false) {
      return false;
    }

    return true;
  }

  bool handle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    if (!canHandle(server, requestMethod, requestUri)) {
      return false;
    }

    log_v("StaticRequestHandler::handle: request=%s _uri=%s\r\n", requestUri.c_str(), _uri.c_str());

    String path(_path);

    if (!_isFile) {
      // Base URI doesn't point to a file.
      // If a directory is requested, look for index file.
      if (requestUri.endsWith("/")) {
        return handle(server, requestMethod, String(requestUri + "index.htm"));
      }

      // Append whatever follows this URI in request to get the file path.
      path += requestUri.substring(_baseUriLength);
    }
    log_v("StaticRequestHandler::handle: path=%s, isFile=%d\r\n", path.c_str(), _isFile);

    String contentType = getContentType(path);

    // look for gz file, only if the original specified path is not a gz.  So part only works to send gzip via content encoding when a non compressed is asked for
    // if you point the the path to gzip you will serve the gzip as content type "application/x-gzip", not text or javascript etc...
    if (!path.endsWith(FPSTR(mimeTable[gz].endsWith)) && !_fs.exists(path)) {
      String pathWithGz = path + FPSTR(mimeTable[gz].endsWith);
      if (_fs.exists(pathWithGz)) {
        path += FPSTR(mimeTable[gz].endsWith);
      }
    }

    File f = _fs.open(path, "r");
    if (!f || !f.available()) {
      return false;
    }

    String eTagCode;

    if (server._eTagEnabled) {
      if (server._eTagFunction) {
        eTagCode = (server._eTagFunction)(_fs, path);
      } else {
        eTagCode = calcETag(_fs, path);
      }

      if (server.header("If-None-Match") == eTagCode) {
        server.send(304);
        return true;
      }
    }

    if (_cache_header.length() != 0) {
      server.sendHeader("Cache-Control", _cache_header);
    }

    if ((server._eTagEnabled) && (eTagCode.length() > 0)) {
      server.sendHeader("ETag", eTagCode);
    }

    server.streamFile(f, contentType);
    return true;
  }

  static String getContentType(const String &path) {
    char buff[sizeof(mimeTable[0].mimeType)];
    // Check all entries but last one for match, return if found
    for (size_t i = 0; i < sizeof(mimeTable) / sizeof(mimeTable[0]) - 1; i++) {
      strcpy_P(buff, mimeTable[i].endsWith);
      if (path.endsWith(buff)) {
        strcpy_P(buff, mimeTable[i].mimeType);
        return String(buff);
      }
    }
    // Fall-through and just return default type
    strcpy_P(buff, mimeTable[sizeof(mimeTable) / sizeof(mimeTable[0]) - 1].mimeType);
    return String(buff);
  }

  // calculate an ETag for a file in filesystem based on md5 checksum
  // that can be used in the http headers - include quotes.
  static String calcETag(FS &fs, const String &path) {
    String result;

    // calculate eTag using md5 checksum
    uint8_t md5_buf[16];
    File f = fs.open(path, "r");
    MD5Builder calcMD5;
    calcMD5.begin();
    calcMD5.addStream(f, f.size());
    calcMD5.calculate();
    calcMD5.getBytes(md5_buf);
    f.close();
    // create a minimal-length eTag using base64 byte[]->text encoding.
    result = "\"" + base64::encode(md5_buf, 16) + "\"";
    return (result);
  }  // calcETag

  StaticRequestHandler &setFilter(WebServer::FilterFunction filter) {
    _filter = filter;
    return *this;
  }

protected:
  // _filter should return 'true' when the request should be handled
  // and 'false' when the request should be ignored
  WebServer::FilterFunction _filter;
  FS _fs;
  String _uri;
  String _path;
  String _cache_header;
  bool _isFile;
  size_t _baseUriLength;
};

#endif  //REQUESTHANDLERSIMPL_H
