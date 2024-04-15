#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#include <vector>
#include <assert.h>

class RequestHandler {
public:
  virtual ~RequestHandler() {}
  virtual bool canHandle(HTTPMethod method, String uri) {
    (void)method;
    (void)uri;
    return false;
  }
  virtual bool canUpload(String uri) {
    (void)uri;
    return false;
  }
  virtual bool canRaw(String uri) {
    (void)uri;
    return false;
  }
  virtual bool handle(WebServer& server, HTTPMethod requestMethod, String requestUri) {
    (void)server;
    (void)requestMethod;
    (void)requestUri;
    return false;
  }
  virtual void upload(WebServer& server, String requestUri, HTTPUpload& upload) {
    (void)server;
    (void)requestUri;
    (void)upload;
  }
  virtual void raw(WebServer& server, String requestUri, HTTPRaw& raw) {
    (void)server;
    (void)requestUri;
    (void)raw;
  }

  RequestHandler* next() {
    return _next;
  }
  void next(RequestHandler* r) {
    _next = r;
  }

private:
  RequestHandler* _next = nullptr;

protected:
  std::vector<String> pathArgs;

public:
  const String& pathArg(unsigned int i) {
    assert(i < pathArgs.size());
    return pathArgs[i];
  }
};

#endif  //REQUESTHANDLER_H
