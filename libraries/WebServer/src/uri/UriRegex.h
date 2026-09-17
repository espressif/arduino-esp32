#ifndef URI_REGEX_H
#define URI_REGEX_H

#include "Uri.h"
#include <esp32-hal-log.h>
#include <regex>

// libstdc++ ships two regex executors. The default one is a recursive
// backtracker: matching a repeating group costs about 128 bytes of task stack
// per character of request-target, and a pattern such as "^(a+)+$" takes
// exponential time (measured on ESP32: 0.9 s for 18 characters, 13.8 s for 22).
// Both are reachable from an unauthenticated request.
//
// The __polynomial extension selects a breadth-first executor that keeps its
// state on the heap instead. Stack use is then flat (~2.5 KB regardless of
// request-target length) and matching is linear in the length, at roughly
// 26 us per character. Capture semantics are unchanged.
//
// That executor cannot handle back-references, so patterns using them keep the
// recursive one and the much smaller length limit that comes with it.

#ifndef WEBSERVER_MAX_REGEX_URI_LEN
#define WEBSERVER_MAX_REGEX_URI_LEN 2048  // matches WEBSERVER_MAX_URI_LEN; 0 disables the check
#endif

// Only used for patterns containing back-references. Budget ~128 bytes of stack
// per character in the task that calls handleClient(), on top of everything else
// that task does (CONFIG_ARDUINO_LOOP_STACK_SIZE for the Arduino loop task, or
// the stack passed to xTaskCreate for a dedicated task).
#ifndef WEBSERVER_MAX_BACKREF_REGEX_URI_LEN
#define WEBSERVER_MAX_BACKREF_REGEX_URI_LEN 64
#endif

class UriRegex : public Uri {

public:
  explicit UriRegex(const char *uri) : Uri(uri){};
  explicit UriRegex(const String &uri) : Uri(uri){};

  Uri *clone() const override final {
    return new UriRegex(_uri);
  };

  void initPathArgs(std::vector<String> &pathArgs) override final {
    std::regex rgx = compile(_uri + "|");
    std::smatch matches;
    std::string s{""};
    std::regex_search(s, matches, rgx);
    pathArgs.resize(matches.size() - 1);
  }

  bool canHandle(const String &requestUri, std::vector<String> &pathArgs) override final {
    if (Uri::canHandle(requestUri, pathArgs)) {
      return true;
    }

    if (!_compiled) {
      _rgx = compile(_uri);
      _compiled = true;
    }

    const size_t maxLen = _boundedStack ? WEBSERVER_MAX_REGEX_URI_LEN : WEBSERVER_MAX_BACKREF_REGEX_URI_LEN;
    if (maxLen && requestUri.length() > maxLen) {
      log_e(
        "Request-target of %u bytes exceeds %s (%u); not matched against %s", (unsigned)requestUri.length(),
        _boundedStack ? "WEBSERVER_MAX_REGEX_URI_LEN" : "WEBSERVER_MAX_BACKREF_REGEX_URI_LEN", (unsigned)maxLen, _uri.c_str()
      );
      return false;
    }

    unsigned int pathArgIndex = 0;
    std::smatch matches;
    std::string s(requestUri.c_str());
    // The bounded executor holds its pending states on the heap, so a request
    // arriving under memory pressure can fail to allocate. Treat that as "route
    // does not match" (the client gets a 404) rather than letting it reach the
    // top of the task.
#ifdef CONFIG_CXX_EXCEPTIONS
    try {
#endif
      if (std::regex_search(s, matches, _rgx)) {
        for (size_t i = 1; i < matches.size(); ++i) {  // skip first
          pathArgs[pathArgIndex] = String(matches[i].str().c_str());
          pathArgIndex++;
        }
        return true;
      }
#ifdef CONFIG_CXX_EXCEPTIONS
    } catch (const std::exception &e) {
      log_e("Matching %s against %s failed: %s", requestUri.c_str(), _uri.c_str(), e.what());
    }
#endif
    return false;
  }

private:
  // A back-reference is a backslash followed by a non-zero digit. Skipping the
  // character after a backslash keeps "\\1" (escaped backslash, literal 1) from
  // being mistaken for one. Octal escapes inside a character class are reported
  // as back-references, which only costs the stricter length limit.
  static bool hasBackreference(const String &pattern) {
    for (size_t i = 0; i + 1 < pattern.length(); ++i) {
      if (pattern[i] != '\\') {
        continue;
      }
      if (pattern[i + 1] >= '1' && pattern[i + 1] <= '9') {
        return true;
      }
      ++i;
    }
    return false;
  }

  std::regex compile(const String &pattern) {
    if (!hasBackreference(pattern)) {
      constexpr auto boundedFlags = std::regex::ECMAScript | std::regex_constants::__polynomial;
#ifdef CONFIG_CXX_EXCEPTIONS
      try {
        std::regex rgx(pattern.c_str(), boundedFlags);
        _boundedStack = true;
        return rgx;
      } catch (const std::regex_error &) {
        // The bounded executor rejected something in the pattern; fall back to
        // the recursive one rather than refusing to serve the route.
      }
#else
      _boundedStack = true;
      return std::regex(pattern.c_str(), boundedFlags);
#endif
    }
    _boundedStack = false;
    return std::regex(pattern.c_str());
  }

  std::regex _rgx;
  bool _compiled = false;
  bool _boundedStack = false;
};

#endif
