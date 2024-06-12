#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#include <vector>
#include <assert.h>

class RequestHandler {
public:
  virtual ~RequestHandler() {}
  virtual bool canHandle(WebServer &server, HTTPMethod method, String uri) {
    (void)server;
    (void)method;
    (void)uri;
    return false;
  }
  virtual bool canUpload(WebServer &server, String uri) {
    (void)server;
    (void)uri;
    return false;
  }
  virtual bool canRaw(WebServer &server, String uri) {
    (void)server;
    (void)uri;
    return false;
  }
  virtual bool handle(WebServer &server, HTTPMethod requestMethod, String requestUri) {
    (void)server;
    (void)requestMethod;
    (void)requestUri;
    return false;
  }
  virtual void upload(WebServer &server, String requestUri, HTTPUpload &upload) {
    (void)server;
    (void)requestUri;
    (void)upload;
  }
  virtual void raw(WebServer &server, String requestUri, HTTPRaw &raw) {
    (void)server;
    (void)requestUri;
    (void)raw;
  }

  virtual RequestHandler& setFilter(std::function<bool(WebServer&)> filter) {
    (void)filter;
    return *this;
  }

  RequestHandler *next() {
    return _next;
  }
  void next(RequestHandler *r) {
    _next = r;
  }

private:
  RequestHandler *_next = nullptr;

protected:
  std::vector<String> pathArgs;

public:
  const String &pathArg(unsigned int i) {
    assert(i < pathArgs.size());
    return pathArgs[i];
  }
};

#endif  //REQUESTHANDLER_H
