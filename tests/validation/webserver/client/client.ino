/*
  client.ino - HTTP client device for multi-DUT WebServer validation test.

  Connects to the server's WiFi AP, then makes HTTP requests to test the
  WebServer::send(code, content_type, Stream&) overload and a set of
  request-parsing robustness properties (see server.ino). Malicious/malformed
  requests are crafted as raw bytes over WiFiClient so the server's parser is
  exercised directly.

  Communicates with the Python test runner via serial protocol.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define SERVER_PORT  80
#define TEST_TIMEOUT 5000
// Time to let the server task settle / recover after each robustness probe.
#define SETTLE_MS 1000

// Basic auth header value for admin:SuperSecurePassword (see server.ino).
static const char *VALID_BASIC_AUTH = "Basic YWRtaW46U3VwZXJTZWN1cmVQYXNzd29yZA==";

// Test data (must match server)
static const char test_body[] = "Hello from Stream!";
static const uint8_t test_data[] = {0xDE, 0xAD, 0xBE, 0xEF};

String ssid = "";
String password = "";
String serverIP = "";

void readWiFiCredentials() {
  Serial.println("[CLIENT] Send SSID:");
  while (ssid.length() == 0) {
    if (Serial.available()) {
      ssid = Serial.readStringUntil('\n');
      ssid.trim();
    }
    delay(100);
  }

  Serial.println("[CLIENT] Send Password:");
  bool password_received = false;
  while (!password_received) {
    if (Serial.available()) {
      password = Serial.readStringUntil('\n');
      password.trim();
      password_received = true;
    }
    delay(100);
  }

  Serial.printf("[CLIENT] SSID: %s\n", ssid.c_str());
  Serial.printf("[CLIENT] Password: %s\n", password.c_str());
}

void readServerIP() {
  Serial.println("[CLIENT] Send server IP:");
  while (serverIP.length() == 0) {
    if (Serial.available()) {
      serverIP = Serial.readStringUntil('\n');
      serverIP.trim();
    }
    delay(100);
  }
  Serial.printf("[CLIENT] Server IP: %s\n", serverIP.c_str());
}

// Send an HTTP GET request and return the raw response
String http_get(const char *path) {
  WiFiClient client;
  if (!client.connect(serverIP.c_str(), SERVER_PORT)) {
    Serial.printf("[CLIENT] Failed to connect to %s:%d\n", serverIP.c_str(), SERVER_PORT);
    return "";
  }

  client.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, serverIP.c_str());

  unsigned long start = millis();
  while (client.connected() && !client.available()) {
    if (millis() - start > TEST_TIMEOUT) {
      client.stop();
      return "";
    }
    delay(1);
  }

  String response;
  unsigned long readStart = millis();
  while (client.connected() || client.available()) {
    if (client.available()) {
      response += (char)client.read();
      readStart = millis();
    } else if (millis() - readStart > TEST_TIMEOUT) {
      break;
    } else {
      delay(1);
    }
  }
  client.stop();
  return response;
}

// Send an arbitrary raw request (already fully formed) and return the raw
// response. If close_early is true, the socket is closed right after writing
// without waiting for a full response (used to simulate a client that
// disconnects mid-request).
String http_raw(const String &request, unsigned long timeout_ms, bool close_early) {
  WiFiClient client;
  if (!client.connect(serverIP.c_str(), SERVER_PORT)) {
    return "";
  }
  // Write in chunks: a single large print can be truncated, which would silently
  // weaken the requests that depend on their full length reaching the parser.
  size_t written = 0;
  while (written < request.length()) {
    size_t chunk = min((size_t)512, request.length() - written);
    size_t n = client.write((const uint8_t *)request.c_str() + written, chunk);
    if (n == 0) {
      break;
    }
    written += n;
  }
  client.flush();

  if (close_early) {
    delay(50);
    client.stop();
    return "";
  }

  String response;
  unsigned long start = millis();
  while (millis() - start < timeout_ms) {
    if (client.available()) {
      response += (char)client.read();
      start = millis();
    } else if (!client.connected()) {
      break;
    } else {
      delay(1);
    }
  }
  client.stop();
  return response;
}

// Extract body from HTTP response (after \r\n\r\n)
String get_body(const String &response) {
  int idx = response.indexOf("\r\n\r\n");
  if (idx < 0) {
    return "";
  }
  return response.substring(idx + 4);
}

// Extract HTTP status code from response
int get_status_code(const String &response) {
  int idx = response.indexOf(' ');
  if (idx < 0) {
    return -1;
  }
  return response.substring(idx + 1).toInt();
}

// Extract Content-Length header value
int get_content_length(const String &response) {
  int idx = response.indexOf("Content-Length: ");
  if (idx < 0) {
    return -1;
  }
  int end = response.indexOf("\r\n", idx);
  String val = response.substring(idx + 16, end);
  return val.toInt();
}

static bool all_passed = true;

void report(const char *name, bool ok, const char *detail = nullptr) {
  if (ok) {
    Serial.printf("[CLIENT] PASS %s\n", name);
  } else {
    if (detail) {
      Serial.printf("[CLIENT] FAIL %s: %s\n", name, detail);
    } else {
      Serial.printf("[CLIENT] FAIL %s\n", name);
    }
    all_passed = false;
  }
}

// Return the server's current boot id, or "" if it is unreachable. Retries to
// tolerate the AP briefly disappearing while the server finishes a connection
// or (on a vulnerable build) reboots and recovers.
// SoftAP disappearances (server crash/recover) drop the client's STA association.
// Rejoin before probing so post-crash survival checks remain meaningful.
void ensureWifi() {
  if (WiFi.STA.status() == WL_CONNECTED) {
    return;
  }
  Serial.println("[CLIENT] Reconnecting to AP");
  WiFi.STA.disconnect();
  WiFi.STA.connect(ssid, password);
  for (int i = 0; i < 50 && WiFi.STA.status() != WL_CONNECTED; i++) {
    delay(200);
  }
  if (WiFi.STA.status() == WL_CONNECTED) {
    Serial.printf("[CLIENT] Reconnected IP=%s\n", WiFi.STA.localIP().toString().c_str());
  } else {
    Serial.println("[CLIENT] Reconnect failed");
  }
}

String get_boot_id(int max_attempts) {
  // SoftAP disappearances (server crash/recover) drop the client's STA association.
  // Rejoin before probing so post-crash survival checks remain meaningful.
  for (int i = 0; i < max_attempts; i++) {
    ensureWifi();
    String body = get_body(http_get("/alive"));
    if (body.startsWith("ALIVE ")) {
      return body.substring(6);
    }
    delay(SETTLE_MS);
  }
  return "";
}

// Run a destructive attack and confirm the server task survived it unchanged:
// same boot id (did not reset) and still responsive (did not hang).
void survivalTest(const char *name, void (*attack)()) {
  String before = get_boot_id(30);
  if (before.length() == 0) {
    report(name, false, "server not responsive before attack");
    return;
  }
  attack();
  // After the attack use a shorter probe window so a hung server is detected
  // before the pytest case timeout elapses.
  String after = get_boot_id(8);
  if (after.length() == 0) {
    report(name, false, "server unresponsive after attack (hang or down)");
  } else if (after != before) {
    report(name, false, "server reset (boot id changed)");
  } else {
    report(name, true);
  }
}

// ------------------------- stream send() overload tests -------------------------

void runStreamTests() {
  // Test 1: Stream with text data
  {
    Serial.println("[CLIENT] Testing /stream_text");
    String response = http_get("/stream_text");
    int cl = get_content_length(response);
    String body = get_body(response);
    if (response.length() == 0) {
      report("stream_text", false, "no response");
    } else if (cl != (int)strlen(test_body)) {
      report("stream_text", false, "content-length mismatch");
    } else if (!body.equals(test_body)) {
      report("stream_text", false, "body mismatch");
    } else {
      report("stream_text", true);
    }
  }

  // Test 2: Stream with binary data
  {
    Serial.println("[CLIENT] Testing /stream_binary");
    String response = http_get("/stream_binary");
    int cl = get_content_length(response);
    String body = get_body(response);
    if (response.length() == 0) {
      report("stream_binary", false, "no response");
    } else if (cl != 4) {
      report("stream_binary", false, "content-length mismatch");
    } else if (body.length() != 4 || (uint8_t)body[0] != 0xDE || (uint8_t)body[1] != 0xAD || (uint8_t)body[2] != 0xBE || (uint8_t)body[3] != 0xEF) {
      report("stream_binary", false, "body mismatch");
    } else {
      report("stream_binary", true);
    }
  }

  // Test 3: Stream with explicit content length
  {
    Serial.println("[CLIENT] Testing /stream_explicit_len");
    String response = http_get("/stream_explicit_len");
    int cl = get_content_length(response);
    if (response.length() == 0) {
      report("stream_explicit_len", false, "no response");
    } else if (cl != (int)strlen(test_body)) {
      report("stream_explicit_len", false, "content-length mismatch");
    } else {
      report("stream_explicit_len", true);
    }
  }

  // Test 4: Empty stream should return 204
  {
    Serial.println("[CLIENT] Testing /stream_empty");
    String response = http_get("/stream_empty");
    int status = get_status_code(response);
    String body = get_body(response);
    if (response.length() == 0) {
      report("stream_empty", false, "no response");
    } else if (status != 204) {
      report("stream_empty", false, "status mismatch");
    } else if (body.length() != 0) {
      report("stream_empty", false, "body should be empty");
    } else {
      report("stream_empty", true);
    }
  }

  // Test 5: String send regression
  {
    Serial.println("[CLIENT] Testing /string");
    String response = http_get("/string");
    String body = get_body(response);
    if (response.length() == 0) {
      report("string", false, "no response");
    } else if (!body.equals("OK")) {
      report("string", false, "body mismatch");
    } else {
      report("string", true);
    }
  }
}

// ------------------------- request-parsing robustness tests -------------------------

// Report 2: a bare "Authorization: <username>" header must not bypass the
// configured plaintext password.
void testAuthBypass() {
  Serial.println("[CLIENT] Testing auth_bypass");

  // Control: no credentials must be rejected.
  String control = http_raw("GET /secure HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false);
  if (get_status_code(control) != 401) {
    report("auth_bypass", false, "control not 401");
    return;
  }

  // Valid Basic credentials must be accepted (endpoint is functioning).
  String ok = http_raw(String("GET /secure HTTP/1.1\r\nHost: x\r\nAuthorization: ") + VALID_BASIC_AUTH + "\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false);
  if (get_status_code(ok) != 200) {
    report("auth_bypass", false, "valid basic auth rejected");
    return;
  }

  // Attack: a bare header whose value equals the username must NOT authenticate.
  String attack = http_raw("GET /secure HTTP/1.1\r\nHost: x\r\nAuthorization: admin\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false);
  int status = get_status_code(attack);
  report("auth_bypass", status == 401, status == 200 ? "bare username authenticated" : "unexpected status");
}

// Report 6: serveStatic() must not allow escaping the configured static root
// via dot segments.
void testPathTraversal() {
  Serial.println("[CLIENT] Testing path_traversal");

  // Sanity: the public file inside the root is served.
  String pub = http_raw("GET /static/public.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false);
  if (get_body(pub).indexOf("PUBLIC_FILE_OK") < 0) {
    report("path_traversal", false, "public file not served");
    return;
  }

  // Attack: traversal to a file outside the root must not disclose it.
  String attack = http_raw("GET /static/../secret.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false);
  bool leaked = get_body(attack).indexOf("TOP_SECRET_OUTSIDE_ROOT") >= 0;
  report("path_traversal", !leaked, leaked ? "secret disclosed via traversal" : nullptr);
}

// Report 8: a completed field from an aborted, unauthenticated multipart
// request must not shadow a later request's arguments.
void testArgPoison() {
  Serial.println("[CLIENT] Testing arg_poison");

  // Baseline authenticated request selects its own "path" argument.
  String baseline = http_raw(
    String("GET /secure?path=/legit HTTP/1.1\r\nHost: x\r\nAuthorization: ") + VALID_BASIC_AUTH + "\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false
  );
  if (get_body(baseline).indexOf("PATH=/legit") < 0) {
    report("arg_poison", false, "baseline path not selected");
    return;
  }

  // Poison: unauthenticated multipart to an unregistered route with a completed
  // "path" field, followed by a truncated file part; then disconnect.
  String boundary = "BND1";
  String body = "--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"path\"\r\n\r\n";
  body += "/post-poison\r\n";
  body += "--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"file\"; filename=\"f.txt\"\r\n";
  body += "Content-Type: text/plain\r\n\r\n";
  body += "truncated";  // no closing boundary -> aborted

  String req = "POST /missing HTTP/1.1\r\nHost: x\r\n";
  req += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  req += "Content-Length: 100000\r\n";  // claim more than we send
  req += "Connection: close\r\n\r\n";
  req += body;
  http_raw(req, TEST_TIMEOUT, true);

  delay(SETTLE_MS);

  // Repeat the authenticated request; it must still select its own argument.
  String after = http_raw(
    String("GET /secure?path=/legit HTTP/1.1\r\nHost: x\r\nAuthorization: ") + VALID_BASIC_AUTH + "\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false
  );
  String after_body = get_body(after);
  bool poisoned = after_body.indexOf("/post-poison") >= 0;
  report("arg_poison", !poisoned, poisoned ? "later request poisoned" : nullptr);
}

// ------------------------- compatibility regression tests -------------------------
// These cover request shapes that the robustness limits above must not refuse.

// A serveStatic() route whose filesystem root is "/" must still serve files.
void testStaticRootMapping() {
  Serial.println("[CLIENT] Testing static_root");
  String resp = http_raw("GET /root/root_ok.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false);
  bool ok = get_body(resp).indexOf("ROOT_FILE_OK") >= 0;
  report("static_root", ok, ok ? nullptr : "file not served from root-mapped serveStatic");
}

// A non-multipart body on a route registered with an upload-style callback must
// still be streamed to that callback.
void testRawBody() {
  Serial.println("[CLIENT] Testing raw_body");
  const int payload_len = 3000;
  String payload;
  payload.reserve(payload_len);
  for (int i = 0; i < payload_len; i++) {
    payload += 'A';
  }
  String req = "PUT /raw HTTP/1.1\r\nHost: x\r\nContent-Type: application/octet-stream\r\n";
  req += "Content-Length: " + String(payload_len) + "\r\nConnection: close\r\n\r\n";
  req += payload;

  String body = get_body(http_raw(req, TEST_TIMEOUT, false));
  bool ok = body.equals("RAW=" + String(payload_len));
  report("raw_body", ok, ok ? nullptr : body.c_str());
}

// A form with more fields than a conservative cap would allow must still be
// parsed in full rather than silently yielding no arguments.
void testManyArgs() {
  Serial.println("[CLIENT] Testing many_args");
  const int n = 100;
  String body;
  for (int i = 0; i < n; i++) {
    if (i > 0) {
      body += '&';
    }
    body += "k" + String(i) + "=v" + String(i);
  }
  String req = "POST /args HTTP/1.1\r\nHost: x\r\nContent-Type: application/x-www-form-urlencoded\r\n";
  req += "Content-Length: " + String(body.length()) + "\r\nConnection: close\r\n\r\n";
  req += body;

  String got = get_body(http_raw(req, TEST_TIMEOUT, false));
  bool ok = got.toInt() == n;
  report("many_args", ok, ok ? nullptr : got.c_str());
}

// A long-but-reasonable query string must be served; an over-long request-target
// must be refused with 414 rather than a silently dropped connection.
void testLongUri() {
  Serial.println("[CLIENT] Testing long_uri");
  String value;
  for (int i = 0; i < 900; i++) {
    value += 'a';
  }
  String resp = http_raw("GET /args?x=" + value + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false);
  if (get_status_code(resp) != 200 || get_body(resp).toInt() != 1) {
    report("long_uri", false, "long query string rejected");
    return;
  }

  String oversized;
  for (int i = 0; i < 4096; i++) {
    oversized += 'a';
  }
  String big = http_raw("GET /args?x=" + oversized + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false);
  int status = get_status_code(big);
  report("long_uri", status == 414, status < 0 ? "no response to over-long target" : "unexpected status");
}

// A protocol line that never terminates must not be buffered without bound: the
// server must refuse it outright while still accepting long-but-legal headers.
void testLongLine() {
  Serial.println("[CLIENT] Testing long_line");

  // A long but legitimate header (a fat cookie) must still be accepted.
  String cookie;
  for (int i = 0; i < 1000; i++) {
    cookie += 'c';
  }
  String legal = "GET /string HTTP/1.1\r\nHost: x\r\nCookie: " + cookie + "\r\nConnection: close\r\n\r\n";
  if (get_status_code(http_raw(legal, TEST_TIMEOUT, false)) != 200) {
    report("long_line", false, "long header rejected");
    return;
  }

  // Request line with no CRLF at all: must be answered 414, not accumulated
  // until the read times out.
  String unterminated = "GET /";
  for (int i = 0; i < 8000; i++) {
    unterminated += 'a';
  }
  int status = get_status_code(http_raw(unterminated, 12000, false));
  if (status != 414) {
    report("long_line", false, status < 0 ? "no response to unterminated request line" : "unexpected status for request line");
    return;
  }

  // Oversized header line: must be answered 431.
  String bigHeader = "GET /string HTTP/1.1\r\nHost: x\r\nX-Pad: ";
  for (int i = 0; i < 8000; i++) {
    bigHeader += 'p';
  }
  bigHeader += "\r\nConnection: close\r\n\r\n";
  int header_status = get_status_code(http_raw(bigHeader, 12000, false));
  report("long_line", header_status == 431, header_status < 0 ? "no response to oversized header" : "unexpected status for header");
}

// Drip-feed a request that is never completed, then report how long the server
// took to answer and what it answered. Returns the status code, or -1 if the
// server never replied while it was being fed.
static int dripAttack(const String &prologue, const String &drip, unsigned long dripIntervalMs, unsigned long giveUpMs, unsigned long *elapsedMs) {
  WiFiClient client;
  if (!client.connect(serverIP.c_str(), SERVER_PORT)) {
    return -2;
  }
  client.print(prologue);
  client.flush();

  String response;
  unsigned long start = millis();
  unsigned long lastDrip = millis();
  while (millis() - start < giveUpMs) {
    if (client.available()) {
      while (client.available()) {
        response += (char)client.read();
      }
      break;  // the server answered instead of staying blocked
    }
    if (!client.connected()) {
      break;
    }
    if (millis() - lastDrip >= dripIntervalMs) {
      client.print(drip);
      client.flush();
      lastDrip = millis();
    }
    delay(10);
  }
  *elapsedMs = millis() - start;
  client.stop();
  return response.length() ? get_status_code(response) : -1;
}

// A client that trickles bytes into a header line it never terminates must be
// dropped on a deadline, not kept alive one byte at a time. On a vulnerable
// build handleClient() stays inside the parser and nothing is answered.
// See https://github.com/espressif/arduino-esp32/issues/12788
void testSlowLine() {
  Serial.println("[CLIENT] Testing slow_line");
  unsigned long elapsed = 0;
  int status = dripAttack("GET /string HTTP/1.1\r\nHost: x\r\nUser-Agent: slow", "x", 1000, 20000, &elapsed);
  Serial.printf("[CLIENT] slow_line answered %d after %lu ms\n", status, elapsed);
  if (status < 0) {
    report("slow_line", false, "server never answered a stalled request line");
    return;
  }
  // The per-line deadline is 5 s; allow generous slack for scheduling.
  report("slow_line", elapsed < 15000, "answered but took too long");
}

// A client that keeps sending complete but tiny headers restarts the per-line
// deadline every time, so only a deadline covering the whole header phase stops
// it. The request is never completed, so the server must give up on its own.
void testSlowHeaders() {
  Serial.println("[CLIENT] Testing slow_headers");
  unsigned long elapsed = 0;
  int status = dripAttack("GET /string HTTP/1.1\r\nHost: x\r\n", "X-Pad: 1\r\n", 1000, 30000, &elapsed);
  Serial.printf("[CLIENT] slow_headers answered %d after %lu ms\n", status, elapsed);
  if (status < 0) {
    report("slow_headers", false, "server never answered a drip-fed header stream");
    return;
  }
  // The header-phase deadline is 10 s.
  report("slow_headers", elapsed < 25000, "answered but took too long");
}

// A regex route must still match a normal-length path.
void testRegexRoute() {
  Serial.println("[CLIENT] Testing regex_route");
  String body = get_body(http_raw("GET /users/12345 HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false));
  if (!body.equals("USER=12345")) {
    report("regex_route", false, body.c_str());
    return;
  }

  // Nested quantifiers must still match and capture normally.
  String nested = get_body(http_raw("GET /nested/aaa HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", TEST_TIMEOUT, false));
  bool ok = nested.equals("NESTED=aaa");
  report("regex_route", ok, ok ? nullptr : nested.c_str());
}

// Report 3: many separators must not crash the device via unbounded allocation.
// Written in chunks so the payload is not lost to a single giant String write.
void attackArgFlood() {
  WiFiClient client;
  if (!client.connect(serverIP.c_str(), SERVER_PORT)) {
    return;
  }
  const int n = 20000;
  client.print("POST /args HTTP/1.1\r\nHost: x\r\n");
  client.print("Content-Type: application/x-www-form-urlencoded\r\n");
  client.printf("Content-Length: %d\r\n", n);
  client.print("Connection: close\r\n\r\n");
  for (int i = 0; i < n; i++) {
    client.write('&');
  }
  client.flush();
  unsigned long start = millis();
  while (client.connected() && millis() - start < TEST_TIMEOUT) {
    if (client.available()) {
      while (client.available()) {
        client.read();
      }
      break;
    }
    delay(1);
  }
  client.stop();
}

// Report 4: a non-multipart POST to an upload route must not dereference a null
// upload object.
void attackUploadNullDeref() {
  String req = "POST /edit HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
  http_raw(req, TEST_TIMEOUT, false);
}

// Report 7: a long path against a regex route with unbounded repetition must
// not exhaust the task stack.
void attackRegexStack() {
  String req = "GET /users/";
  for (int i = 0; i < 2000; i++) {
    req += '9';
  }
  req += " HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
  http_raw(req, TEST_TIMEOUT, false);
}

// Report 7, second vector: a nested quantifier against a short input that
// cannot match. A recursive backtracking engine explores 2^30 paths here, which
// blocks the server task for roughly an hour; a bounded engine answers in
// milliseconds. The length limits do not help because the request is tiny.
void attackRegexBacktrack() {
  String req = "GET /nested/";
  for (int i = 0; i < 30; i++) {
    req += 'a';
  }
  req += "b HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
  http_raw(req, TEST_TIMEOUT, false);
}

// Report 1: a multipart body that ends immediately after the initial boundary
// must not spin the parser in an unbounded loop.
void attackMultipartEof() {
  String boundary = "AaB03x";
  String req = "POST /edit HTTP/1.1\r\nHost: x\r\n";
  req += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  req += "Content-Length: 100000\r\n";
  req += "Connection: close\r\n\r\n";
  req += "--" + boundary + "\r\n";  // initial boundary, then EOF
  http_raw(req, TEST_TIMEOUT, true);
}

void runTests() {
  runStreamTests();

  // Non-destructive robustness checks first (server stays responsive).
  testAuthBypass();
  testPathTraversal();

  // Compatibility checks: request shapes the robustness limits must still accept.
  testStaticRootMapping();
  testRawBody();
  testManyArgs();
  testLongUri();
  testLongLine();
  testRegexRoute();

  // Slow-client checks. A vulnerable build stays inside the parser for as long
  // as the attack runs, but recovers once the connection closes, so these are
  // safe to run before the destructive cases.
  testSlowLine();
  testSlowHeaders();

  // upload_null_deref must run before any multipart request: a prior multipart
  // parse can leave _currentUpload allocated and mask the null dereference.
  Serial.println("[CLIENT] Testing upload_null_deref");
  survivalTest("upload_null_deref", attackUploadNullDeref);

  testArgPoison();

  // Checks that, on a vulnerable build, crash or hang the server task. Each one
  // re-baselines the server boot id first, so they are independently meaningful
  // even if a previous attack reset (and auto-recovered) the server.
  Serial.println("[CLIENT] Testing arg_flood");
  survivalTest("arg_flood", attackArgFlood);
  Serial.println("[CLIENT] Testing regex_stack");
  survivalTest("regex_stack", attackRegexStack);
  // regex_backtrack before multipart_eof: on a vulnerable build it blocks the
  // server task for hours, so nothing after it can baseline against /alive.
  Serial.println("[CLIENT] Testing regex_backtrack");
  survivalTest("regex_backtrack", attackRegexBacktrack);
  // multipart_eof last: a vulnerable build hangs the server task permanently,
  // so no later case can baseline against /alive.
  Serial.println("[CLIENT] Testing multipart_eof");
  survivalTest("multipart_eof", attackMultipartEof);

  if (all_passed) {
    Serial.println("[CLIENT] All tests passed");
  } else {
    Serial.println("[CLIENT] Some tests failed");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  Serial.println("[CLIENT] Device ready for WiFi credentials");
  readWiFiCredentials();
  readServerIP();

  // Connect to server's AP
  WiFi.STA.begin();
  WiFi.STA.connect(ssid, password);

  Serial.printf("[CLIENT] Connecting to SSID=%s\n", ssid.c_str());

  int retries = 50;
  while (WiFi.STA.status() != WL_CONNECTED && retries--) {
    delay(200);
  }

  if (WiFi.STA.status() != WL_CONNECTED) {
    Serial.println("[CLIENT] Failed to connect");
    return;
  }

  Serial.printf("[CLIENT] Connected IP=%s\n", WiFi.STA.localIP().toString().c_str());

  // Run all tests
  runTests();
}

void loop() {}
