#include <Arduino.h>

#include <WiFi.h>
#include <assert.h>
#include <HTTPClient.h>

#define USE_SERIAL Serial

// Enable or disable collecting all headers
#define COLLECT_ALL_HEADERS true

void setup() {

  USE_SERIAL.begin(115200);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  WiFi.begin("SSID", "PASSWORD");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    USE_SERIAL.print(".");
  }
  USE_SERIAL.println();
  USE_SERIAL.println("Connected to WiFi: " + WiFi.SSID());
}

void loop() {

  HTTPClient http;

  USE_SERIAL.print("[HTTP] Preparing HTTP request...\n");
  // This page will return the headers we want to test + some others
  http.begin("https://httpbingo.org/response-headers?x-custom-header=value:42");

#if COLLECT_ALL_HEADERS
  // Collect all headers
  http.collectAllHeaders();
#else
  // Collect specific headers, only that one will be stored
  const char *headerKeys[] = {"x-custom-header"};
  const size_t headerKeysCount = sizeof(headerKeys) / sizeof(headerKeys[0]);
  http.collectHeaders(headerKeys, headerKeysCount);
#endif

  USE_SERIAL.print("[HTTP] Sending HTTP GET request...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    USE_SERIAL.printf("[HTTP] GET response code: %d\n", httpCode);

    USE_SERIAL.println("[HTTP] Headers collected:");
    for (size_t i = 0; i < http.headers(); i++) {
      USE_SERIAL.printf("[HTTP] - '%s': '%s'\n", http.headerName(i).c_str(), http.header(i).c_str());
    }

    USE_SERIAL.println("[HTTP] Has header 'x-custom-header'? " + String(http.hasHeader("x-custom-header")) + " (expected true)");
    USE_SERIAL.printf("[HTTP] x-custom-header: '%s' (expected 'value:42')\n", http.header("x-custom-header").c_str());
    USE_SERIAL.printf("[HTTP] non-existing-header: '%s' (expected empty string)\n", http.header("non-existing-header").c_str());

#if COLLECT_ALL_HEADERS
    // Server response with multiple headers, one of them is 'server'
    USE_SERIAL.println("[HTTP] Has header 'server'? " + String(http.hasHeader("server")) + " (expected true)");
    USE_SERIAL.printf("[HTTP] server: '%s'\n", http.header("server").c_str());
#endif

  } else {
    USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();

  Serial.println("[HTTP] end connection\n\n");
  delay(5000);
}
