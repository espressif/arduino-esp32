#ifndef URI_H
#define URI_H

#include <Arduino.h>
#include <vector>

class Uri {

protected:
  const String _uri;

public:
  Uri(const char *uri) : _uri(uri) {}
  Uri(const String &uri) : _uri(uri) {}
  Uri(const __FlashStringHelper *uri) : _uri((const char *)uri) {}
  virtual ~Uri() {}

  virtual Uri *clone() const {
    return new Uri(_uri);
  };

  virtual void initPathArgs(__attribute__((unused)) std::vector<String> &pathArgs) {}

  virtual bool canHandle(const String &requestUri, __attribute__((unused)) std::vector<String> &pathArgs) {
    return _uri == requestUri;
  }
};

#endif
