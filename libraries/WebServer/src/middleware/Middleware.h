#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include <assert.h>
#include <functional>

class MiddlewareChain;
class WebServer;

class Middleware {
public:
  typedef std::function<bool(void)> Callback;
  typedef std::function<bool(WebServer &server, Callback next)> Function;

  virtual ~Middleware() {}

  virtual bool run(WebServer &server, Callback next) {
    return next();
  };

private:
  friend MiddlewareChain;
  Middleware *_next = nullptr;
  bool _freeOnRemoval = false;
};

class MiddlewareFunction : public Middleware {
public:
  MiddlewareFunction(Middleware::Function fn) : _fn(fn) {}

  bool run(WebServer &server, Middleware::Callback next) override {
    return _fn(server, next);
  }

private:
  Middleware::Function _fn;
};

class MiddlewareChain {
public:
  ~MiddlewareChain();

  void addMiddleware(Middleware::Function fn);
  void addMiddleware(Middleware *middleware);
  bool removeMiddleware(Middleware *middleware);

  bool runChain(WebServer &server, Middleware::Callback finalizer);

private:
  Middleware *_root = nullptr;
  Middleware *_current = nullptr;
};

#endif
