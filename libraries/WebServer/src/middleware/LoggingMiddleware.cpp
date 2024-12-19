#include "Middlewares.h"

void LoggingMiddleware::setOutput(Print &output) {
  _out = &output;
}

bool LoggingMiddleware::run(WebServer &server, Middleware::Callback next) {
  if (_out == nullptr) {
    return next();
  }

  _out->print(F("* Connection from "));
  _out->print(server.client().remoteIP().toString());
  _out->print(F(":"));
  _out->println(server.client().remotePort());

  _out->print(F("> "));
  const HTTPMethod method = server.method();
  if (method == HTTP_ANY) {
    _out->print(F("HTTP_ANY"));
  } else {
    _out->print(http_method_str(method));
  }
  _out->print(F(" "));
  _out->print(server.uri());
  _out->print(F(" "));
  _out->println(server.version());

  int n = server.headers();
  for (int i = 0; i < n; i++) {
    String v = server.header(i);
    if (!v.isEmpty()) {
      // because these 2 are always there, eventually empty: "Authorization", "If-None-Match"
      _out->print(F("> "));
      _out->print(server.headerName(i));
      _out->print(F(": "));
      _out->println(server.header(i));
    }
  }

  _out->println(F(">"));

  uint32_t elapsed = millis();
  const bool ret = next();
  elapsed = millis() - elapsed;

  if (ret) {
    _out->print(F("* Processed in "));
    _out->print(elapsed);
    _out->println(F(" ms"));
    _out->print(F("< "));
    _out->print(F("HTTP/1."));
    _out->print(server.version());
    _out->print(F(" "));
    _out->print(server.responseCode());
    _out->print(F(" "));
    _out->println(WebServer::responseCodeToString(server.responseCode()));

    n = server.responseHeaders();
    for (int i = 0; i < n; i++) {
      _out->print(F("< "));
      _out->print(server.responseHeaderName(i));
      _out->print(F(": "));
      _out->println(server.responseHeader(i));
    }

    _out->println(F("<"));

  } else {
    _out->println(F("* Not processed!"));
  }

  return ret;
}
