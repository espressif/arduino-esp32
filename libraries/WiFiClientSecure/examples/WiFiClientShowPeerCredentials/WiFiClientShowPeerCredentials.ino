// WiFiClientShowPeerCredentials
//
// Example of a establishing a secure connection and then
// showing the fingerprint of the certificate. This can
// be useful in an IoT setting to know for sure that you
// are connecting to the right server. Especally in 
// situations where you cannot hardcode a trusted root
// certificate for long periods of time (as they tend to
// get replaced more often than the lifecycle of IoT
// hardware).
//

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#ifndef WIFI_NETWORK
#define WIFI_NETWORK "MyWifiNetwork"
#endif

#ifndef WIFI_PASSWD
#define WIFI_PASSWD "MySecretWifiPassword"
#endif

#define URL "https://arduino.cc"

void demo() {
  WiFiClientSecure *client = new WiFiClientSecure;
  client->setInsecure(); // 

  HTTPClient https;
  if (!https.begin(*client, URL )) {
    Serial.println("HTTPS setup failed");
    return;
  };

  https.setTimeout(5000);

  int httpCode = https.GET();
  if (httpCode != 200) {
    Serial.print("Connect failed: ");
    Serial.println(https.errorToString(httpCode));
    return;
  }

  const mbedtls_x509_crt* peer = client->getPeerCertificate();

  // Show general output / certificate information
  //
  char buf[1024];
  int l = mbedtls_x509_crt_info (buf, sizeof(buf), "", peer);
  if (l <= 0) {
    Serial.println("Peer conversion to printable buffer failed");
    return;
  };
  Serial.println();
  Serial.println(buf);

  uint8_t fingerprint_remote[32];
  if (!client->getFingerprintSHA256(fingerprint_remote)) {
    Serial.println("Failed to get the fingerprint");
    return;
  }
  // Fingerprint late 2021
  Serial.println("Expecting Fingerprint (SHA256): 70 CF A4 B7 5D 09 E9 2A 52 A8 B6 85 B5 0B D6 BE 83 47 83 5B 3A 4D 3C 3E 32 30 EC 1D 61 98 D7 0F");
  Serial.print(  " Received Fingerprint (SHA256): ");

  for (int i = 0; i < 32; i++) {
    Serial.print(fingerprint_remote[i], HEX);
    Serial.print(" ");
  };
  Serial.println("");
};

void setup() {
  Serial.begin(115200);
  Serial.println("Started " __FILE__ " build " __DATE__ " " __TIME__);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NETWORK, WIFI_PASSWD);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Wifi fail - rebooting");
    delay(5000);
    ESP.restart();
  }
}

void loop() {
  bool already_tried = false;
  if ((millis() < 1000) || already_tried)
    return;
  already_tried = true;

  // Run the test just once.
  demo();
}
