#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>

// Rather than specify the password as plaintext; we
// provide it as an (unsalted!) SHA1 hash. This is not
// much more secure (SHA1 is past its retirement age,
// and long obsolete/insecure) - but it helps a little.

const char* ssid = "........";
const char* password = "........";

WebServer server(80);

// Passwords as plaintext - human readable and easily visible in
// the sourcecode and in the firmware/binary.
const char* www_username = "admin";
const char* www_password = "esp32";

// The sha1 of 'esp32' (without the trailing \0) expressed as 20
// bytes of hex. Created by for example 'echo -n esp32 | openssl sha1'
// or http://www.sha1-online.com.
const char* www_username_hex = "hexadmin";
const char* www_password_hex = "8cb124f8c277c16ec0b2ee00569fd151a08e342b";

// The same; but now expressed as a base64 string (e.g. as commonly used
// by webservers). Created by ` echo -n esp32 | openssl sha1 -binary | openssl base64`
const char* www_username_base64 = "base64admin";
const char* www_password_base64 = "jLEk+MJ3wW7Asu4AVp/RUaCONCs=";

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Connect Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }
  ArduinoOTA.begin();

  server.on("/", []() {
    if (server.authenticate(www_username, www_password)) {
      server.send(200, "text/plain", "Login against cleartext password OK");
      return;
    }
    if (server.authenticateBasicSHA1(www_username_hex, www_password_hex)) {
      server.send(200, "text/plain", "Login against HEX of the SHA1 of the password OK");
      return;
    }
    if (server.authenticateBasicSHA1(www_username_base64, www_password_base64)) {
      server.send(200, "text/plain", "Login against Base64 of the SHA1 of the password OK");
      return;
    }
    Serial.println("No/failed authentication");
    return server.requestAuthentication();
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
