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

#include <mbedtls/md.h> // for the hmac/ cookie

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

String demousers[] = { "admin", "user1", "user2" };
String demopasswords[] = { "secret", "secr!t", "s3cret" };
String demoseeds[] = { "ORUGKU3FMNZGK5CTMVSWI", "ORUGKU3FMNZGK5CTMVSWIMQ", "ORUGKU3FMNZGK5CTMVSWIMY"};

const unsigned long cookie_secret_seed[2] = { esp_random(), esp_random() };

const int LOGIN_TIMEOUT = 30; // 30 minute timeout.
#define LOGIN_COOKIE_NAME "token"

String hmac(time_t t) {
  unsigned char digest[32];
  char digest_hex[64 + 1];
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      (unsigned char *) & (cookie_secret_seed[0]), sizeof(cookie_secret_seed), // key
                      (unsigned char *) & t, sizeof(t), // input
                      digest))
    return ""; // safe - as we compare it with something known to be 64 hex bytes.

  for (int i = 0; i < sizeof(digest); i++)
    sprintf(digest_hex + i * 2, "%02X", digest[i]);

  return String(digest_hex);
}

// Issue 1) Once the user has logged in once; with a valid Time based OTP; that OTP will
//          only be valid for up to LOGIN_TIMEOUT seconds. Then it won't work anymore.
//          We solve this by setting a secure cookie that is valid for an hour.
//          And if we see that - we assume all is well.
//
bool hasValidCookie(WebServer &server) {
  if (!server.hasHeader("Cookie"))
    return false;

  String cookie = server.header("Cookie");
  int at = cookie.indexOf(LOGIN_COOKIE_NAME "=");
  if (at < 0)
    return false;

  String token = cookie.substring(at + 7, 16 + 64);
  if (token.length() != 16 + 64)
    return false;
  String time_str = token.substring(0, 16);
  String hmac_str = token.substring(16, 64);

  time_t issued = strtoul(time_str.c_str(), NULL, 10);
  time_t now = time(NULL);
  if (now - issued > LOGIN_TIMEOUT * 60)
    return false;

  return hmac(issued).equals(hmac_str);
}

void setCookie(WebServer &server) {
  char buff[16 + 64 + 1];
  snprintf(buff, sizeof(buff), "%016lu%s", time(NULL), hmac(time(NULL)).c_str());
  server.sendHeader("Set-Cookie", LOGIN_COOKIE_NAME "=" + String(buff));
}

String * checkOTP(HTTPAuthMethod mode, String username, String params[]) {
  if (mode != BASIC_AUTH) {
    Serial.println("Not basic auth - rejecting");
    return NULL;
  };

  // find the password and seed for this user.
  //
  String * passwd = NULL, * seed = NULL;
  for (int i = 0; i < sizeof(demousers) / sizeof(String); i++) {
    if (demousers[i] == username) {
      passwd = &demopasswords[i];
      seed = &demoseeds[i];
      break;
    }
  };

  if (!passwd || !seed) {
    Serial.println("Unknown username - rejecting");
    return NULL;
  };

  // Issue 2) We anticipate some clock skew - so try 30 seconds 'around' the current time';
  //          and if we're more clever; we could track which people use; and then adjust our
  //          idea of how much our clock is likely behind or ahead; along with that of
  //          people their typical clocks.
  //
  for (int i = -2 ; i <= 2; i++) {
    String * otp = TOTP::currentOTP(time(NULL) + i * TOTP::RFC6238_DEFAULT_interval, *seed);

    if (!params[0].startsWith(*otp))
      continue;

    // Issue 3) Use proper 2FA/MFA - and insist the user types the token followed by the password
    //
    if (!params[0].substring(6).equals(*passwd)) {
      Serial.println("Incorrect password - rejecting");
      return NULL;
    }

    // entirely `bypass' the authentication system; and return
    // the password as entered; as that is known to be 'ok'
    //
    Serial.print("User "); Serial.print(username); Serial.println(" logged in");
    return new String(params[0]);
  } // clockskew loop

  Serial.println("All OTPs wrong - rejecting");
  return NULL;
}

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
    if (!hasValidCookie(server) && !server.authenticate(&checkOTP)) {
      server.requestAuthentication();
      return;
    }
    // set or renew the cookie.
    setCookie(server);

    // Let the user in.
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
  time_t n = t / TOTP::RFC6238_DEFAULT_interval;
  if (n == lst_i)
    return;
  lst_i = n;

  Serial.print(ctime(&t));
  for (int i = 0; i < sizeof(demousers) / sizeof(String); i++) {
    String * otp = TOTP::currentOTP(demoseeds[i]);

    Serial.print("   ");
    Serial.print(demousers[i]);
    Serial.print(" - ");
    Serial.print(*otp);
    Serial.print(demopasswords[i]);
    Serial.print(" - (Seed for calculator: ");
    Serial.print(demoseeds[i]);
    Serial.println(")");

    delete otp;
  }
}
