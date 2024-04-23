/*|-----------------------------------------------------------|*/
/*|WORKING EXAMPLE FOR HTTPS CONNECTION                       |*/
/*|Author: Bc. Martin Chlebovec                               |*/
/*|Technical University of Košice                             |*/
/*|TESTED BOARDS: Devkit v1 DOIT, Devkitc v4                  |*/
/*|CORE: 0.9x, 1.0.0, 1.0.1 tested, working (newer not tested)|*/
/*|Supported methods: PEAP + MsCHAPv2, EAP-TTLS + MsCHAPv2    |*/
/*|-----------------------------------------------------------|*/

// This example demonstrates a secure connection to a WiFi network using WPA/WPA2 Enterprise (for example eduroam),
// and establishing a secure HTTPS connection with an external server (for example arduino.php5.sk) using the defined anonymous identity, user identity, and password.

// Note: this example is outdated and may not work!
// For more examples see https://github.com/martinius96/ESP32-eduroam

#include <WiFi.h>
#include <NetworkClientSecure.h>
#if __has_include("esp_eap_client.h")
#include "esp_eap_client.h"
#else
#include "esp_wpa2.h"
#endif
#include <Wire.h>
#define EAP_ANONYMOUS_IDENTITY "anonymous@example.com"  //anonymous identity
#define EAP_IDENTITY           "id@example.com"         //user identity
#define EAP_PASSWORD           "password"               //eduroam user password
const char *ssid = "eduroam";                           // eduroam SSID
const char *host = "arduino.php5.sk";                   //external server domain for HTTPS connection
int counter = 0;
const char *test_root_ca = "-----BEGIN CERTIFICATE-----\n"
                           "MIIEsTCCA5mgAwIBAgIQCKWiRs1LXIyD1wK0u6tTSTANBgkqhkiG9w0BAQsFADBh\n"
                           "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
                           "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
                           "QTAeFw0xNzExMDYxMjIzMzNaFw0yNzExMDYxMjIzMzNaMF4xCzAJBgNVBAYTAlVT\n"
                           "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
                           "b20xHTAbBgNVBAMTFFJhcGlkU1NMIFJTQSBDQSAyMDE4MIIBIjANBgkqhkiG9w0B\n"
                           "AQEFAAOCAQ8AMIIBCgKCAQEA5S2oihEo9nnpezoziDtx4WWLLCll/e0t1EYemE5n\n"
                           "+MgP5viaHLy+VpHP+ndX5D18INIuuAV8wFq26KF5U0WNIZiQp6mLtIWjUeWDPA28\n"
                           "OeyhTlj9TLk2beytbtFU6ypbpWUltmvY5V8ngspC7nFRNCjpfnDED2kRyJzO8yoK\n"
                           "MFz4J4JE8N7NA1uJwUEFMUvHLs0scLoPZkKcewIRm1RV2AxmFQxJkdf7YN9Pckki\n"
                           "f2Xgm3b48BZn0zf0qXsSeGu84ua9gwzjzI7tbTBjayTpT+/XpWuBVv6fvarI6bik\n"
                           "KB859OSGQuw73XXgeuFwEPHTIRoUtkzu3/EQ+LtwznkkdQIDAQABo4IBZjCCAWIw\n"
                           "HQYDVR0OBBYEFFPKF1n8a8ADIS8aruSqqByCVtp1MB8GA1UdIwQYMBaAFAPeUDVW\n"
                           "0Uy7ZvCj4hsbw5eyPdFVMA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEF\n"
                           "BQcDAQYIKwYBBQUHAwIwEgYDVR0TAQH/BAgwBgEB/wIBADA0BggrBgEFBQcBAQQo\n"
                           "MCYwJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBCBgNVHR8E\n"
                           "OzA5MDegNaAzhjFodHRwOi8vY3JsMy5kaWdpY2VydC5jb20vRGlnaUNlcnRHbG9i\n"
                           "YWxSb290Q0EuY3JsMGMGA1UdIARcMFowNwYJYIZIAYb9bAECMCowKAYIKwYBBQUH\n"
                           "AgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwCwYJYIZIAYb9bAEBMAgG\n"
                           "BmeBDAECATAIBgZngQwBAgIwDQYJKoZIhvcNAQELBQADggEBAH4jx/LKNW5ZklFc\n"
                           "YWs8Ejbm0nyzKeZC2KOVYR7P8gevKyslWm4Xo4BSzKr235FsJ4aFt6yAiv1eY0tZ\n"
                           "/ZN18bOGSGStoEc/JE4ocIzr8P5Mg11kRYHbmgYnr1Rxeki5mSeb39DGxTpJD4kG\n"
                           "hs5lXNoo4conUiiJwKaqH7vh2baryd8pMISag83JUqyVGc2tWPpO0329/CWq2kry\n"
                           "qv66OSMjwulUz0dXf4OHQasR7CNfIr+4KScc6ABlQ5RDF86PGeE6kdwSQkFiB/cQ\n"
                           "ysNyq0jEDQTkfa2pjmuWtMCNbBnhFXBYejfubIhaUbEv2FOQB3dCav+FPg5eEveX\n"
                           "TVyMnGo=\n"
                           "-----END CERTIFICATE-----\n";
// You can use x.509 client certificates if you want
//const char* test_client_key = "";   //to verify the client
//const char* test_client_cert = "";  //to verify the client
NetworkClientSecure client;
void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.print("Connecting to network: ");
  Serial.println(ssid);
  WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
  WiFi.mode(WIFI_STA);    //init wifi mode
#if __has_include("esp_eap_client.h")
  esp_eap_client_set_identity((uint8_t *)EAP_ANONYMOUS_IDENTITY, strlen(EAP_ANONYMOUS_IDENTITY));  //provide identity
  esp_eap_client_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));                      //provide username
  esp_eap_client_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));                      //provide password
  esp_wifi_sta_enterprise_enable();
#else
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_ANONYMOUS_IDENTITY, strlen(EAP_ANONYMOUS_IDENTITY));  //provide identity
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));                      //provide username
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));                      //provide password
  esp_wifi_sta_wpa2_ent_enable();
#endif
  WiFi.begin(ssid);  //connect to wifi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    counter++;
    if (counter >= 60) {  //after 30 seconds timeout - reset board (on unsuccessful connection)
      ESP.restart();
    }
  }
  client.setCACert(test_root_ca);
  //client.setCertificate(test_client_cert); // for client verification - certificate
  //client.setPrivateKey(test_client_key);  // for client verification - private key
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address set: ");
  Serial.println(WiFi.localIP());  //print LAN IP
}
void loop() {
  if (WiFi.status() == WL_CONNECTED) {  //if we are connected to eduroam network
    counter = 0;                        //reset counter
    Serial.println("Wifi is still connected with IP: ");
    Serial.println(WiFi.localIP());            //inform user about his IP address
  } else if (WiFi.status() != WL_CONNECTED) {  //if we lost connection, retry
    WiFi.begin(ssid);
  }
  while (WiFi.status() != WL_CONNECTED) {  //during lost connection, print dots
    delay(500);
    Serial.print(".");
    counter++;
    if (counter >= 60) {  //30 seconds timeout - reset board
      ESP.restart();
    }
  }
  Serial.print("Connecting to website: ");
  Serial.println(host);
  if (client.connect(host, 443)) {
    String url = "/rele/rele1.txt";
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "User-Agent: ESP32\r\n" + "Connection: close\r\n\r\n");
    while (client.connected()) {
      String header = client.readStringUntil('\n');
      Serial.println(header);
      if (header == "\r") {
        break;
      }
    }
    String line = client.readStringUntil('\n');
    Serial.println(line);
  } else {
    Serial.println("Connection unsuccessful");
  }
  delay(5000);
}
