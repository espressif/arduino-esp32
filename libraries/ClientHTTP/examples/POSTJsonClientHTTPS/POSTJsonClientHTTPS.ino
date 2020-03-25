/*
 * ClientHTTP example sketch
 * 
 * POST json data on a WiFiClientSecure with a CA certificate
 * 
 * This example sketch demonstrates how to use the ClientHTTP library
 * to send json data to a web server using POST. HTTPS (TLS 1.2) secure
 * communication is used with a CA certificate. ArduinoJson is used to
 * serialize and deserialize Json data. The Postman echo server is used
 * for this example.
 * 
 * Created by Jeroen Döll on 25 March 2020
 * This example is in the public domain
 */

// BECAUSE ArduinoJson IS NO PART OF THE STANDARD ESP LIBRARIES the #include <ArduinoJson.h> is COMMENTED OUT
// Remove the comment to use this example!

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#if defined(ESP32)
#include <WiFi.h>
#endif

#include <WiFiClientSecure.h>
//#include <ArduinoJson.h>

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

#if !defined(ARDUINOJSON_VERSION_MAJOR)
  Serial.println("Remove comment from #include <ArduinoJson.h> to test this example! ");
  return;
#endif

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

  http.requestHeaders["Content-Type"] = "application/json";
  http.requestHeaders["Connection"] = "close";
  http.responseHeaders["Content-Type"];

  if (!http.connect("postman-echo.com", 443)) {
    Serial.println("Connection failed");
    return;
  }

  // If the Content-Type: application/json is used, like in this example, data should be a serialized json!
  const char json[] = "{\"sensor\":\"gps\",\"time\":1351824120,\"data\":[48.756080,2.302038]}";
  size_t payloadLength = 0;
#if ARDUINOJSON_VERSION_MAJOR == 6
  const size_t requestCapacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
  DynamicJsonDocument requestDocument(requestCapacity);
  DeserializationError error = deserializeJson(requestDocument, json);
  // Test if parsing succeeds.
  if(error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
  payloadLength = measureJson(requestDocument);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 5
  const size_t requestCapacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
  DynamicJsonBuffer jsonBuffer(requestCapacity);
  JsonObject& root = jsonBuffer.parseObject(json);
  // Test if parsing succeeds.
  if(!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }  
  payloadLength = root.measureLength();
#endif  

  if(!http.POST("/post", payloadLength)) {
    Serial.println("Failed to POST");
    return;
  }

#if ARDUINOJSON_VERSION_MAJOR == 6
  serializeJson(requestDocument, http);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 5
  root.printTo(http);
#endif

  ClientHTTP::http_code_t status = http.status();

  if(status > 0) {
    Serial.printf("HTTP status code is %d\n", status);
    if(status == ClientHTTP::OK || status == ClientHTTP::NO_CONTENT) {
      Serial.println("Payload succesfully send!");
    }

    Serial.printf("Response header \"Content-Type: %s\"\n", http.responseHeaders["Content-Type"].c_str());

#if ARDUINOJSON_VERSION_MAJOR == 6
    const size_t responseCapacity = 2*JSON_ARRAY_SIZE(2) + 3*JSON_OBJECT_SIZE(0) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(7) + 618;
    DynamicJsonDocument responseDocument(responseCapacity);
    DeserializationError error = deserializeJson(responseDocument, http);
    // Test if parsing succeeds.
    if(error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }
    Serial.println("Received json:");
    serializeJsonPretty(responseDocument, Serial);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 5
    const size_t responseCapacity = 2*JSON_ARRAY_SIZE(2) + 3*JSON_OBJECT_SIZE(0) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(7) + 618;
    DynamicJsonBuffer jsonBuffer(responseCapacity);
    JsonObject& rootReceived = jsonBuffer.parseObject(http);
    // Test if parsing succeeds.
    if(!rootReceived.success()) {
      Serial.println("parseObject() failed");
      return;
    }  
    Serial.println("Received json:");
    rootReceived.prettyPrintTo(Serial);
#endif

    Serial.println("\n");
  } else {
    Serial.printf("Request failed with error code %d\n", status);
  }
  
  Serial.println("Closing connection");
  http.stop();
}

void loop() {
}
