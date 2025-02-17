#include "Middleware.h"

MiddlewareChain::~MiddlewareChain() {
  Middleware *current = _root;
  while (current) {
    Middleware *next = current->_next;
    if (current->_freeOnRemoval) {
      delete current;
    }
    current = next;
  }
  _root = nullptr;
}

void MiddlewareChain::addMiddleware(Middleware::Function fn) {
  MiddlewareFunction *middleware = new MiddlewareFunction(fn);
  middleware->_freeOnRemoval = true;
  addMiddleware(middleware);
}

void MiddlewareChain::addMiddleware(Middleware *middleware) {
  if (!_root) {
    _root = middleware;
    return;
  }
  Middleware *current = _root;
  while (current->_next) {
    current = current->_next;
  }
  current->_next = middleware;
}

bool MiddlewareChain::removeMiddleware(Middleware *middleware) {
  if (!_root) {
    return false;
  }
  if (_root == middleware) {
    _root = _root->_next;
    if (middleware->_freeOnRemoval) {
      delete middleware;
    }
    return true;
  }
  Middleware *current = _root;
  while (current->_next) {
    if (current->_next == middleware) {
      current->_next = current->_next->_next;
      if (middleware->_freeOnRemoval) {
        delete middleware;
      }
      return true;
    }
    current = current->_next;
  }
  return false;
}

bool MiddlewareChain::runChain(WebServer &server, Middleware::Callback finalizer) {
  if (!_root) {
    return finalizer();
  }
  _current = _root;
  Middleware::Callback next;
  next = [this, &server, &next, finalizer]() {
    if (!_current) {
      return finalizer();
    }
    Middleware *that = _current;
    _current = _current->_next;
    return that->run(server, next);
  };
  return next();
}
