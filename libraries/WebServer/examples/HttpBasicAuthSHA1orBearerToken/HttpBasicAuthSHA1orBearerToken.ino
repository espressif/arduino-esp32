#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <SHA1Builder.h>

// We have two options - we either come in with a bearer
// token - i.e. a special header or API token; or we
// get a normal HTTP style basic auth prompt.
//
// To do a bearer fetch - use something like Swagger or with curl:
//
//    curl https://myesp.com/  -H "Authorization: Bearer SecritToken"
//
// We avoid hardcoding this "SecritToken" into the code by
// using a SHA1 instead (which is not particularly secure).

// Create the secret token SHA1 with:
//        echo -n SecritToken | openssl sha1

String secret_token_hex = "d2cce6b472959484a21c3194080c609b8a2c910b";

// Wifi credentials

const char *ssid = "........";
const char *password = "........";

WebServer server(80);

// Rather than specify the admin password as plaintext; we
// provide it as an (unsalted!) SHA1 hash. This is not
// much more secure (SHA1 is past its retirement age,
// and long obsolete/insecure) - but it helps a little.

// The sha1 of 'esp32' (without the trailing \0) expressed as 20
// bytes of hex. Created by for example 'echo -n esp32 | openssl sha1'
// or http://www.sha1-online.com.
const char *www_username_hex = "admin";
const char *www_password_hex = "8cb124f8c277c16ec0b2ee00569fd151a08e342b";

static unsigned char _bearer[20];

String *check_bearer_or_auth(HTTPAuthMethod mode, String authReq, String params[]) {
  // we expect authReq to be "bearer some-secret"
  String lcAuthReq = authReq;
  lcAuthReq.toLowerCase();
  if (mode == OTHER_AUTH && (lcAuthReq.startsWith("bearer "))) {
    String secret = authReq.substring(7);
    secret.trim();

    uint8_t sha1[20];
    SHA1Builder sha_builder;

    sha_builder.begin();
    sha_builder.add((uint8_t *)secret.c_str(), secret.length());
    sha_builder.calculate();
    sha_builder.getBytes(sha1);

    if (memcmp(_bearer, sha1, sizeof(_bearer)) == 0) {
      Serial.println("Bearer token matches");
      return new String("anything non null");
    } else {
      Serial.println("Bearer token does not match");
    }
  } else if (mode == BASIC_AUTH) {
    bool ret = server.authenticateBasicSHA1(www_username_hex, www_password_hex);
    if (ret) {
      Serial.println("Basic auth succeeded");
      return new String(params[0]);
    } else {
      Serial.println("Basic auth failed");
    }
  }

  // No auth found
  return NULL;
};

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Connect Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }
  ArduinoOTA.begin();

  // Convert token to a convenient binary representation.
  size_t len = HEXBuilder::hex2bytes(_bearer, sizeof(_bearer), secret_token_hex);
  if (len != 20) {
    Serial.println("Bearer token does not look like a valid SHA1 hex string ?!");
  }

  server.on("/", []() {
    if (!server.authenticate(&check_bearer_or_auth)) {
      Serial.println("No/failed authentication");
      return server.requestAuthentication();
    }
    Serial.println("Authentication succeeded");
    server.send(200, "text/plain", "Login OK");
    return;
  });

  server.begin();

  Serial.print("Open http://");
  Serial.print(WiFi.localIP());
  Serial.println("/ in your browser to see it working");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  delay(2);  //allow the cpu to switch to other tasks
}
