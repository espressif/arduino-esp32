/*
  server.ino - WebServer device for multi-DUT validation test.

  Runs a WiFi softAP and WebServer with test endpoints for the
  WebServer::send(code, content_type, Stream&) overload, plus a set of
  endpoints configured the same way as the upstream WebServer examples so the
  client device can exercise request-parsing robustness properties: malformed
  multipart bodies, oversized query strings, authentication headers,
  static-file path traversal, regex routes and cross-request argument state.

  Communicates with the Python test runner via serial protocol.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <uri/UriRegex.h>
#include "esp_system.h"

#define SERVER_PORT 80

static WebServer server(SERVER_PORT);
static Preferences prefs;

// Per-boot identifier reported by /alive. The client uses it to detect whether
// a request caused the server to reset (the id changes) versus hang (no reply).
static String bootId;

// Credentials for the authentication-protected endpoint.
static const char *www_username = "admin";
static const char *www_password = "SuperSecurePassword";

// Files used by the serveStatic() traversal check. The public file lives inside
// the configured static root; the secret file deliberately lives outside of it.
static const char *STATIC_ROOT = "/www/";
static const char *PUBLIC_PATH = "/www/public.txt";
static const char *PUBLIC_BODY = "PUBLIC_FILE_OK";
static const char *SECRET_PATH = "/secret.txt";
static const char *SECRET_BODY = "TOP_SECRET_OUTSIDE_ROOT";

// File served through a serveStatic() route whose filesystem root is "/", the
// mapping used by the WebServer example. Kept separate from the traversal check
// above so that check keeps a root it must not escape.
static const char *ROOT_FILE_PATH = "/root_ok.txt";
static const char *ROOT_FILE_BODY = "ROOT_FILE_OK";

// In-memory stream for testing (simulates a File)
class TestStream : public Stream {
  const uint8_t *_buf;
  size_t _size;
  size_t _pos;

public:
  TestStream(const uint8_t *buf, size_t size) : _buf(buf), _size(size), _pos(0) {}

  int available() override {
    return (int)(_size - _pos);
  }

  int read() override {
    if (_pos >= _size) {
      return -1;
    }
    return _buf[_pos++];
  }

  int peek() override {
    if (_pos >= _size) {
      return -1;
    }
    return _buf[_pos];
  }

  size_t write(uint8_t) override {
    return 0;
  }
};

// Test data
static const char test_body[] = "Hello from Stream!";
static const uint8_t test_data[] = {0xDE, 0xAD, 0xBE, 0xEF};

String ssid = "";
String password = "";

// Dummy upload/raw body sink. Mirrors the upstream FSBrowser upload-callback
// pattern: the callback reads server.upload() on every invocation. Used by the
// multipart and non-multipart upload robustness checks.
void handleUpload() {
  // Touch members through server.upload() the same way application callbacks do.
  // A bare (void)upload.status can be optimized away and hide a null dereference.
  HTTPUpload &upload = server.upload();
  Serial.printf("[SERVER] upload status=%d name=%s\n", (int)upload.status, upload.name.c_str());
}

// Raw (non-multipart) body sink, registered in the same callback slot as an
// upload handler. Mirrors the upstream UploadHugeFile example.
static size_t rawTotalSize = 0;
static bool rawSawStart = false;

void handleRawBody() {
  HTTPRaw &raw = server.raw();
  if (raw.status == RAW_START) {
    rawTotalSize = 0;
    rawSawStart = true;
  } else if (raw.status == RAW_WRITE) {
    rawTotalSize += raw.currentSize;
  } else if (raw.status == RAW_END) {
    Serial.printf("[SERVER] raw end total=%u\n", (unsigned)rawTotalSize);
  }
}

void readWiFiCredentials() {
  Serial.println("[SERVER] Send SSID:");
  while (ssid.length() == 0) {
    if (Serial.available()) {
      ssid = Serial.readStringUntil('\n');
      ssid.trim();
    }
    delay(100);
  }

  Serial.println("[SERVER] Send Password:");
  bool password_received = false;
  while (!password_received) {
    if (Serial.available()) {
      password = Serial.readStringUntil('\n');
      password.trim();
      password_received = true;
    }
    delay(100);
  }

  Serial.printf("[SERVER] SSID: %s\n", ssid.c_str());
  Serial.printf("[SERVER] Password: %s\n", password.c_str());
}

void setupFilesystem() {
  if (!LittleFS.begin(true)) {
    Serial.println("[SERVER] LittleFS mount failed");
    return;
  }
  LittleFS.mkdir("/www");
  File pub = LittleFS.open(PUBLIC_PATH, "w");
  if (pub) {
    pub.print(PUBLIC_BODY);
    pub.close();
    Serial.println("[SERVER] Wrote public file");
  } else {
    Serial.println("[SERVER] Failed to write public file");
  }
  File secret = LittleFS.open(SECRET_PATH, "w");
  if (secret) {
    secret.print(SECRET_BODY);
    secret.close();
    Serial.println("[SERVER] Wrote secret file");
  } else {
    Serial.println("[SERVER] Failed to write secret file");
  }
  File rootFile = LittleFS.open(ROOT_FILE_PATH, "w");
  if (rootFile) {
    rootFile.print(ROOT_FILE_BODY);
    rootFile.close();
    Serial.println("[SERVER] Wrote root file");
  } else {
    Serial.println("[SERVER] Failed to write root file");
  }
}

void registerEndpoints() {
  // --- Stream send() overload endpoints (existing coverage) ---
  server.on("/stream_text", HTTP_GET, []() {
    TestStream stream((const uint8_t *)test_body, strlen(test_body));
    server.send(200, "text/plain", stream);
    Serial.println("[SERVER] Served /stream_text");
  });

  server.on("/stream_binary", HTTP_GET, []() {
    TestStream stream(test_data, sizeof(test_data));
    server.send(200, "application/octet-stream", stream);
    Serial.println("[SERVER] Served /stream_binary");
  });

  server.on("/stream_explicit_len", HTTP_GET, []() {
    TestStream stream((const uint8_t *)test_body, strlen(test_body));
    server.send(200, "text/plain", stream, strlen(test_body));
    Serial.println("[SERVER] Served /stream_explicit_len");
  });

  server.on("/stream_empty", HTTP_GET, []() {
    uint8_t empty = 0;
    TestStream stream(&empty, 0);
    server.send(200, "text/plain", stream);
    Serial.println("[SERVER] Served /stream_empty");
  });

  server.on("/string", HTTP_GET, []() {
    server.send(200, "text/plain", "OK");
    Serial.println("[SERVER] Served /string");
  });

  // --- Liveness probe: used to confirm the server task survives each attack ---
  // The boot id lets the client distinguish "still alive" from "reset and
  // recovered" (id changes) and from "hung" (no response).
  server.on("/alive", HTTP_GET, []() {
    server.send(200, "text/plain", "ALIVE " + bootId);
  });

  // --- Query-argument echo: reports how many args were parsed (report 3) ---
  // Also logs the parsed argument count so an oversized query string that gets
  // truncated in transit can be told apart from one the parser fully consumed.
  server.on("/args", HTTP_ANY, []() {
    Serial.printf("[SERVER] /args count=%d uri_len=%u\n", server.args(), (unsigned)server.uri().length());
    server.send(200, "text/plain", String(server.args()));
  });

  // --- Authentication-protected endpoint (reports 2 and 8) ---
  // Mirrors the upstream HttpBasicAuth example. Returns the "path" argument so
  // the client can also observe cross-request argument state.
  server.on("/secure", HTTP_GET, []() {
    if (!server.authenticate(www_username, www_password)) {
      return server.requestAuthentication();
    }
    server.send(200, "text/plain", "PATH=" + server.arg("path"));
  });

  // --- Upload endpoints (reports 1 and 4) ---
  // Mirrors the upstream FSBrowser POST /edit route: a handler plus an upload
  // callback that touches server.upload().
  server.on(
    "/edit", HTTP_POST,
    []() {
      server.send(200, "text/plain", "UPLOAD_OK");
    },
    handleUpload
  );

  // --- Regex route (report 7) ---
  // Mirrors the upstream PathArgServer example: unbounded repetition consuming
  // attacker-controlled path characters.
  server.on(UriRegex("^\\/users\\/([0-9]+)$"), HTTP_GET, []() {
    server.send(200, "text/plain", "USER=" + server.pathArg(0));
  });

  // --- Regex route with nested quantifiers (report 7, second vector) ---
  // A recursive backtracking engine explores 2^n paths for a non-matching input
  // of n characters, blocking the server task for hours on a short request.
  server.on(UriRegex("^\\/nested\\/(a+)+$"), HTTP_GET, []() {
    server.send(200, "text/plain", "NESTED=" + server.pathArg(0));
  });

  // --- Raw (non-multipart) request body route ---
  // Same registration shape as an upload route, but the callback consumes
  // server.raw(). Confirms raw bodies still stream to the callback instead of
  // being buffered whole or dropped.
  server.on(
    "/raw", HTTP_PUT,
    []() {
      server.send(200, "text/plain", rawSawStart ? "RAW=" + String(rawTotalSize) : "RAW_CALLBACK_MISSING");
      rawSawStart = false;
    },
    handleRawBody
  );

  // --- Static file serving with a non-root mapping (report 6) ---
  server.serveStatic("/static", LittleFS, STATIC_ROOT);

  // --- Static file serving mapped to the filesystem root ---
  server.serveStatic("/root", LittleFS, "/");
}

// True when the device just came back from a non-power-on reset and previously
// stored valid credentials in NVS. Power-on / external / brownout resets always
// force the serial handshake so the test runner can hand over fresh credentials.
bool shouldAutoRecover() {
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("[SERVER] Reset reason: %d\n", (int)reason);
  if (reason == ESP_RST_POWERON || reason == ESP_RST_EXT || reason == ESP_RST_BROWNOUT) {
    return false;
  }
  prefs.begin("wstest", true);
  bool have = prefs.isKey("ssid") && prefs.isKey("pass");
  prefs.end();
  return have;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // New id for this boot so the client can detect resets.
  bootId = String(esp_random(), HEX);

  if (shouldAutoRecover()) {
    prefs.begin("wstest", true);
    ssid = prefs.getString("ssid", "");
    password = prefs.getString("pass", "");
    prefs.end();
    Serial.println("[SERVER] Recovering AP after reset");
  } else {
    Serial.println("[SERVER] Device ready for WiFi credentials");
    readWiFiCredentials();
    prefs.begin("wstest", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
    prefs.end();
  }

  setupFilesystem();

  // Start WiFi softAP
  WiFi.AP.begin();
  bool ok = WiFi.AP.create(ssid, password);
  if (!ok) {
    Serial.println("[SERVER] Failed to start AP");
    return;
  }

  IPAddress ip = WiFi.AP.localIP();
  Serial.printf("[SERVER] AP started IP=%s\n", ip.toString().c_str());

  registerEndpoints();

  server.begin();
  Serial.println("[SERVER] Server started");
}

void loop() {
  server.handleClient();
}
