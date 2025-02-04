#include "Middlewares.h"

CorsMiddleware &CorsMiddleware::setOrigin(const char *origin) {
  _origin = origin;
  return *this;
}

CorsMiddleware &CorsMiddleware::setMethods(const char *methods) {
  _methods = methods;
  return *this;
}

CorsMiddleware &CorsMiddleware::setHeaders(const char *headers) {
  _headers = headers;
  return *this;
}

CorsMiddleware &CorsMiddleware::setAllowCredentials(bool credentials) {
  _credentials = credentials;
  return *this;
}

CorsMiddleware &CorsMiddleware::setMaxAge(uint32_t seconds) {
  _maxAge = seconds;
  return *this;
}

void CorsMiddleware::addCORSHeaders(WebServer &server) {
  server.sendHeader(F("Access-Control-Allow-Origin"), _origin.c_str());
  server.sendHeader(F("Access-Control-Allow-Methods"), _methods.c_str());
  server.sendHeader(F("Access-Control-Allow-Headers"), _headers.c_str());
  server.sendHeader(F("Access-Control-Allow-Credentials"), _credentials ? F("true") : F("false"));
  server.sendHeader(F("Access-Control-Max-Age"), String(_maxAge).c_str());
}

bool CorsMiddleware::run(WebServer &server, Middleware::Callback next) {
  // Origin header ? => CORS handling
  if (server.hasHeader(F("Origin"))) {
    addCORSHeaders(server);
    // check if this is a preflight request => handle it and return
    if (server.method() == HTTP_OPTIONS) {
      server.send(200);
      return true;
    }
  }
  return next();
}
