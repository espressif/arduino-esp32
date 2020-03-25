/*
 * ClientHTTP example sketch
 * 
 * POST form data on a WiFiClientSecure with a CA certificate
 * 
 * This example sketch demonstrates how to use the ClientHTTP library
 * to send data to a web server using POST. HTTPS (TLS 1.2) secure
 * communication is used with a CA certificate. The Postman echo server
 * is used for this example. This example assumes that the length of the
 * payload is not known beforehand and needs to be computed. The utility
 * class Sizer is used for this.
 * 
 * Created by Jeroen Döll on 25 March 2020
 * This example is in the public domain
 */

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#if defined(ESP32)
#include <WiFi.h>
#endif

#include <WiFiClientSecure.h>

#include <ClientHTTP.h>

#ifndef STASSID
#define STASSID "your-ssid"
#define STAPSK  "your-password"
#endif

const char * ssid = STASSID;
const char * password = STAPSK;

// Use the command:
//   openssl s_client -showcerts -connect postman-echo.com:443
// and copy the last certificate from the chain
// This certificate expires on 
const char * rootCACert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIEdTCCA12gAwIBAgIJAKcOSkw0grd/MA0GCSqGSIb3DQEBCwUAMGgxCzAJBgNV
BAYTAlVTMSUwIwYDVQQKExxTdGFyZmllbGQgVGVjaG5vbG9naWVzLCBJbmMuMTIw
MAYDVQQLEylTdGFyZmllbGQgQ2xhc3MgMiBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0
eTAeFw0wOTA5MDIwMDAwMDBaFw0zNDA2MjgxNzM5MTZaMIGYMQswCQYDVQQGEwJV
UzEQMA4GA1UECBMHQXJpem9uYTETMBEGA1UEBxMKU2NvdHRzZGFsZTElMCMGA1UE
ChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjE7MDkGA1UEAxMyU3RhcmZp
ZWxkIFNlcnZpY2VzIFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEi
MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDVDDrEKvlO4vW+GZdfjohTsR8/
y8+fIBNtKTrID30892t2OGPZNmCom15cAICyL1l/9of5JUOG52kbUpqQ4XHj2C0N
Tm/2yEnZtvMaVq4rtnQU68/7JuMauh2WLmo7WJSJR1b/JaCTcFOD2oR0FMNnngRo
Ot+OQFodSk7PQ5E751bWAHDLUu57fa4657wx+UX2wmDPE1kCK4DMNEffud6QZW0C
zyyRpqbn3oUYSXxmTqM6bam17jQuug0DuDPfR+uxa40l2ZvOgdFFRjKWcIfeAg5J
Q4W2bHO7ZOphQazJ1FTfhy/HIrImzJ9ZVGif/L4qL8RVHHVAYBeFAlU5i38FAgMB
AAGjgfAwge0wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0O
BBYEFJxfAN+qAdcwKziIorhtSpzyEZGDMB8GA1UdIwQYMBaAFL9ft9HO3R+G9FtV
rNzXEMIOqYjnME8GCCsGAQUFBwEBBEMwQTAcBggrBgEFBQcwAYYQaHR0cDovL28u
c3MyLnVzLzAhBggrBgEFBQcwAoYVaHR0cDovL3guc3MyLnVzL3guY2VyMCYGA1Ud
HwQfMB0wG6AZoBeGFWh0dHA6Ly9zLnNzMi51cy9yLmNybDARBgNVHSAECjAIMAYG
BFUdIAAwDQYJKoZIhvcNAQELBQADggEBACMd44pXyn3pF3lM8R5V/cxTbj5HD9/G
VfKyBDbtgB9TxF00KGu+x1X8Z+rLP3+QsjPNG1gQggL4+C/1E2DUBc7xgQjB3ad1
l08YuW3e95ORCLp+QCztweq7dp4zBncdDQh/U90bZKuCJ/Fp1U1ervShw3WnWEQt
8jxwmKy6abaVd38PMV4s/KCHOkdp8Hlf9BRUpJVeEXgSYCfOn8J3/yNTd126/+pZ
59vPr5KW7ySaNRB6nJHGDn2Z9j8Z3/VyVOEVqQdZe4O/Ui5GjLIAZHYcSNPYeehu
VsyuLAOQ1xk4meTKCRlb/weWsKh/NEnfVqn3sF/tM+2MR7cwA130A4w=
-----END CERTIFICATE-----
)EOF";


void setup() {
  Serial.begin(115200);
  Serial.println();

  Serial.printf("Connecting to %s", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

#if defined(ESP8266)
  configTime("CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", "pool.ntp.org", "time.nist.gov");
#endif
#if defined(ESP32)
  configTzTime("CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", "pool.ntp.org", "time.nist.gov");
#endif

  Serial.print("Waiting for time");
  time_t epoch;
  for(;;) {
    Serial.write('.');
    time(&epoch);
    if(epoch >= 3600) break;
    delay(500);
  }
  Serial.print("Current time: ");
  Serial.println(asctime(localtime(&epoch)));
  
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
#if defined(ESP8266)
  BearSSL::X509List cert(rootCACert);
  client.setTrustAnchors(&cert);
#endif
#if defined(ESP32)
  client.setCACert(rootCACert);
#endif

  ClientHTTP http(client);
  http.setTimeout(10000);

  http.requestHeaders["Content-Type"] = "application/x-www-form-urlencoded";
  http.requestHeaders["Connection"] = "close";

  if (!http.connect("postman-echo.com", 443)) {
    Serial.println("Connection failed");
    return;
  }

  // If the Content-Type: application/x-www-form-urlencoded is used, like in this example, data should be url encoded!
  const char payload[] = "field1=value1&field2=value2";

  // Determine the size of the payload using the utility class Sizer
  Sizer sizer;
  sizer.print(payload);
  size_t payloadLength = sizer.getSize();
  Serial.printf("Payload length is %u\n", payloadLength);

  if(!http.POST("/post", payloadLength)) {
    Serial.println("Failed to POST");
    return;
  }

  http.print(payload);

  ClientHTTP::http_code_t status = http.status();

  if(status > 0) {
    Serial.printf("HTTP status code is %d\n", status);
    if(status == ClientHTTP::OK || status == ClientHTTP::NO_CONTENT) {
      Serial.println("Payload succesfully send!");
    }

    http.printTo(Serial);
    Serial.println();
  } else {
    Serial.printf("Request failed with error code %d\n", status);
  }
  
  Serial.println("Closing connection");
  http.stop();
}

void loop() {
}
