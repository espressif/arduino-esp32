/**
 * Basic example of using Middlewares with WebServer
 *
 * Middleware are common request/response processing functions that can be applied globally to all incoming requests or to specific handlers.
 * They allow for a common processing thus saving memory and space to avoid duplicating code or states on multiple handlers.
 *
 * Once the example is flashed (with the correct WiFi credentials), you can test the following scenarios with the listed curl commands:
 * - CORS Middleware: answers to OPTIONS requests with the specified CORS headers and also add CORS headers to the response when the request has the Origin header
 * - Logging Middleware: logs the request and response to an output in a curl-like format
 * - Authentication Middleware: test the authentication with Digest Auth
 *
 * You can also add your own Middleware by extending the Middleware class and implementing the run method.
 * When implementing a Middleware, you can decide when to call the next Middleware in the chain by calling next().
 *
 * Middleware are execute in order of addition, the ones attached to the server will be executed first.
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Middlewares.h>

// Your AP WiFi Credentials
// ( This is the AP your ESP will broadcast )
const char *ap_ssid = "ESP32_Demo";
const char *ap_password = "";

WebServer server(80);

LoggingMiddleware logger;
CorsMiddleware cors;
AuthenticationMiddleware auth;

void setup(void) {
  Serial.begin(115200);
  WiFi.softAP(ap_ssid, ap_password);

  Serial.print("IP address: ");
  Serial.println(WiFi.AP.localIP());

  // curl-like output example:
  //
  // > curl -v -X OPTIONS -H "origin: http://192.168.4.1" http://192.168.4.1/
  //
  //  Connection from 192.168.4.2:51683
  // > OPTIONS / HTTP/1.1
  // > Host: 192.168.4.1
  // > User-Agent: curl/8.10.0
  // > Accept: */*
  // > origin: http://192.168.4.1
  // >
  // * Processed in 5 ms
  // < HTTP/1.HTTP/1.1 200 OK
  // < Content-Type: text/html
  // < Access-Control-Allow-Origin: http://192.168.4.1
  // < Access-Control-Allow-Methods: POST,GET,OPTIONS,DELETE
  // < Access-Control-Allow-Headers: X-Custom-Header
  // < Access-Control-Allow-Credentials: false
  // < Access-Control-Max-Age: 600
  // < Content-Length: 0
  // < Connection: close
  // <
  logger.setOutput(Serial);

  cors.setOrigin("http://192.168.4.1");
  cors.setMethods("POST,GET,OPTIONS,DELETE");
  cors.setHeaders("X-Custom-Header");
  cors.setAllowCredentials(false);
  cors.setMaxAge(600);

  auth.setUsername("admin");
  auth.setPassword("admin");
  auth.setRealm("My Super App");
  auth.setAuthMethod(DIGEST_AUTH);
  auth.setAuthFailureMessage("Authentication Failed");

  server.addMiddleware(&logger);
  server.addMiddleware(&cors);

  // Not authenticated
  //
  // Test CORS preflight request with:
  // > curl -v -X OPTIONS -H "origin: http://192.168.4.1" http://192.168.4.1/
  //
  // Test cross-domain request with:
  // > curl -v -X GET -H "origin: http://192.168.4.1" http://192.168.4.1/
  //
  server.on("/", []() {
    server.send(200, "text/plain", "Home");
  });

  // Authenticated
  //
  // > curl -v -X GET -H "origin: http://192.168.4.1"  http://192.168.4.1/protected
  //
  // Outputs:
  //
  // * Connection from 192.168.4.2:51750
  // > GET /protected HTTP/1.1
  // > Host: 192.168.4.1
  // > User-Agent: curl/8.10.0
  // > Accept: */*
  // > origin: http://192.168.4.1
  // >
  // * Processed in 7 ms
  // < HTTP/1.HTTP/1.1 401 Unauthorized
  // < Content-Type: text/html
  // < Access-Control-Allow-Origin: http://192.168.4.1
  // < Access-Control-Allow-Methods: POST,GET,OPTIONS,DELETE
  // < Access-Control-Allow-Headers: X-Custom-Header
  // < Access-Control-Allow-Credentials: false
  // < Access-Control-Max-Age: 600
  // < WWW-Authenticate: Digest realm="My Super App", qop="auth", nonce="ac388a64184e3e102aae6fff1c9e8d76", opaque="e7d158f2b54d25328142d118ff0f932d"
  // < Content-Length: 21
  // < Connection: close
  // <
  //
  // > curl -v -X GET -H "origin: http://192.168.4.1" --digest -u admin:admin  http://192.168.4.1/protected
  //
  // Outputs:
  //
  // * Connection from 192.168.4.2:53662
  // > GET /protected HTTP/1.1
  // > Authorization: Digest username="admin", realm="My Super App", nonce="db9e6824eb2a13bc7b2bf8f3c43db896", uri="/protected", cnonce="NTliZDZiNTcwODM2MzAyY2JjMDBmZGJmNzFiY2ZmNzk=", nc=00000001, qop=auth, response="6ebd145ba0d3496a4a73f5ae79ff5264", opaque="23d739c22810282ff820538cba98bda4"
  // > Host: 192.168.4.1
  // > User-Agent: curl/8.10.0
  // > Accept: */*
  // > origin: http://192.168.4.1
  // >
  // Request handling...
  // * Processed in 7 ms
  // < HTTP/1.HTTP/1.1 200 OK
  // < Content-Type: text/plain
  // < Access-Control-Allow-Origin: http://192.168.4.1
  // < Access-Control-Allow-Methods: POST,GET,OPTIONS,DELETE
  // < Access-Control-Allow-Headers: X-Custom-Header
  // < Access-Control-Allow-Credentials: false
  // < Access-Control-Max-Age: 600
  // < Content-Length: 9
  // < Connection: close
  // <
  server
    .on(
      "/protected",
      []() {
        Serial.println("Request handling...");
        server.send(200, "text/plain", "Protected");
      }
    )
    .addMiddleware(&auth);

  // Not found is also handled by global middleware
  //
  // curl -v -X GET -H "origin: http://192.168.4.1" http://192.168.4.1/inexsting
  //
  // Outputs:
  //
  // * Connection from 192.168.4.2:53683
  // > GET /inexsting HTTP/1.1
  // > Host: 192.168.4.1
  // > User-Agent: curl/8.10.0
  // > Accept: */*
  // > origin: http://192.168.4.1
  // >
  // * Processed in 16 ms
  // < HTTP/1.HTTP/1.1 404 Not Found
  // < Content-Type: text/plain
  // < Access-Control-Allow-Origin: http://192.168.4.1
  // < Access-Control-Allow-Methods: POST,GET,OPTIONS,DELETE
  // < Access-Control-Allow-Headers: X-Custom-Header
  // < Access-Control-Allow-Credentials: false
  // < Access-Control-Max-Age: 600
  // < Content-Length: 14
  // < Connection: close
  // <
  server.onNotFound([]() {
    server.send(404, "text/plain", "Page not found");
  });

  server.collectAllHeaders();
  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  delay(2);  //allow the cpu to switch to other tasks
}
