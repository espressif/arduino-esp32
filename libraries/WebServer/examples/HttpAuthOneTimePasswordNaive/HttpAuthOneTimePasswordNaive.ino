#if 1
// bodge to get this example to compile in the default install.
void setup() {};
void loop() {};
#else
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <lwip/apps/sntp.h>

// https://github.com/dirkx/Arduino-Base32-Decode or simply from
// Sketch->Include Library->Library manager and search for
// "Base32-Decode".
//
#include <Base32-Decode.h>

// https://github.com/dirkx/Arduino-TOTP-RFC6238-generator or simply from
// Sketch->Include Library->Library manager and search for
// "TOTP-RC6236-generator".
//
#include <TOTP-RC6236-generator.hpp>

// Naive RFC 6238 one time password; with a couple of possibly flawed
// assumptionms:
//
// 1) we're going to assume that the clock is perfect!
//
// 2) we're going to assume you just need access for however long
//    your OTP code is valid for. So that is at the most 30 seconds.
//
// 3) we're dispensing with a password; all you need to know is
//    the username and the one time passcode of that moment. So this
//    is not exactly MFA.
//
// Which is not very useful. But keeps this demo simple.
// 

#include <mbedtls/bignum.h> // for the uint64_t  typedef
#include <mbedtls/md.h> // for the hmac/sha1 calculation

#ifndef WIFI_NETWORK
#define WIFI_NETWORK "mySecretWiFiPassword"
#warning "You propably want to change this line!"
#endif

#ifndef WIFI_PASSWD
#define WIFI_PASSWD "mySecretWiFiPassword"
#warning "You propably want to change this line!"
#endif

#ifndef NTP_SERVER
#define NTP_SERVER "nl.pool.ntp.org"
#warning "You MUST set an appropriate ntp pool - see http://ntp.org"
#endif

#ifndef NTP_DEFAULT_TZ
#define NTP_DEFAULT_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
#endif


const char* ssid = WIFI_NETWORK;
const char* password = WIFI_PASSWD;

WebServer server(80);

const String demouser = "admin";

// we only define one seed, for the user admin. Normally every user would
// have their own seed and also enter a password - to make this proper
// two factor authentication (see note 1 below)
//
const String seed = "theSecretSeed"; // must be a Base32 string; can be == padded.
const time_t interval = 30; // seconds (default)
const time_t epoch = 0; // epoch relative to the unix epoch (jan 1970 is the default)
const int digits = 6; // length (default is 0)

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

  // we need a reasonable accurate time for TOTP to work.
  //
  configTzTime(NTP_DEFAULT_TZ, NTP_SERVER);

  server.on("/", []() {
    if (!server.authenticate([](HTTPAuthMethod mode, String username, String params[]) -> String * {
    if (username == demouser)
        return TOTP::currentOTP(seed);
      return NULL;
    }))
    {
      server.requestAuthentication();
      return;
    }
    server.send(200, "text/plain", "Login OK");
  });
  server.begin();

  Serial.print("Open http://");
  Serial.print(WiFi.localIP());
  Serial.println("/ in your browser to see it working");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  static unsigned long last = millis();
  if (millis() - last < 2000)
    return;
  last = millis();
  time_t t = time(NULL);
  if (t < 1000000) {
    Serial.println("Not having a stable time yet.. TOTP is not going to work.");
    return;
  };
  static time_t lst_i = 0;
  time_t n = t / interval;
  if (n == lst_i)
    return;
  lst_i = n;

  Serial.print(ctime(&t));
  Serial.print("   TOTP at this time is: ");
  String * otp = TOTP::currentOTP(seed);
  Serial.println(*otp);
  Serial.println();
  delete otp;
}
#endif
