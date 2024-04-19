/*
  Wifi secure connection example for ESP32 using a pre-shared key (PSK)
  This is useful with MQTT servers instead of using a self-signed cert, tested with mosquitto.
  Running on TLS 1.2 using mbedTLS

  To test run a test server using: openssl s_server -accept 8443 -psk 1a2b3c4d -nocert
  It will show the http request made, but there's no easy way to send a reply back...

  2017 - Evandro Copercini - Apache 2.0 License.
  2018 - Adapted for PSK by Thorsten von Eicken
*/

#include <NetworkClientSecure.h>
#include <WiFi.h>

#if 0
const char* ssid     = "your-ssid";     // your network SSID (name of wifi network)
const char* password = "your-password"; // your network password
#else
const char *ssid = "test";            // your network SSID (name of wifi network)
const char *password = "securetest";  // your network password
#endif

//const char*  server = "server.local";  // Server hostname
const IPAddress server = IPAddress(192, 168, 0, 14);  // Server IP address
const int port = 8443;                                // server's port (8883 for MQTT)

const char *pskIdent = "Client_identity";  // PSK identity (sometimes called key hint)
const char *psKey = "1a2b3c4d";            // PSK Key (must be hex string without 0x)

NetworkClientSecure client;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  delay(100);

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
  }

  Serial.print("Connected to ");
  Serial.println(ssid);

  client.setPreSharedKey(pskIdent, psKey);

  Serial.println("\nStarting connection to server...");
  if (!client.connect(server, port)) {
    Serial.println("Connection failed!");
  } else {
    Serial.println("Connected to server!");
    // Make a HTTP request:
    client.println("GET /a/check HTTP/1.0");
    client.print("Host: ");
    client.println(server);
    client.println("Connection: close");
    client.println();

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }
    // if there are incoming bytes available
    // from the server, read them and print them:
    while (client.available()) {
      char c = client.read();
      Serial.write(c);
    }

    client.stop();
  }
}

void loop() {
  // do nothing
}
