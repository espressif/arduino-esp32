#include "WiFi.h"
#include "AsyncUDP.h"

// To use secrets please read the documentation at https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/guides/secrets.html
#if __has_include("secrets.h")
  #include "secrets.h"
#endif

#ifdef SECRETS_WIFI_SSID_1
const char* ssid     = SECRETS_WIFI_SSID_1;
#else
const char* ssid     = "example-SSID1"; // Traditional way
#endif

#ifdef SECRETS_WIFI_PASSWORD_1
const char* password = SECRETS_WIFI_PASSWORD_1;
#else
const char* password = "example-password-1"; // Traditional way
#endif

AsyncUDP udp;

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("WiFi Failed");
        while(1) {
            delay(1000);
        }
    }
    if(udp.listenMulticast(IPAddress(239,1,2,3), 1234)) {
        Serial.print("UDP Listening on IP: ");
        Serial.println(WiFi.localIP());
        udp.onPacket([](AsyncUDPPacket packet) {
            Serial.print("UDP Packet Type: ");
            Serial.print(packet.isBroadcast()?"Broadcast":packet.isMulticast()?"Multicast":"Unicast");
            Serial.print(", From: ");
            Serial.print(packet.remoteIP());
            Serial.print(":");
            Serial.print(packet.remotePort());
            Serial.print(", To: ");
            Serial.print(packet.localIP());
            Serial.print(":");
            Serial.print(packet.localPort());
            Serial.print(", Length: ");
            Serial.print(packet.length());
            Serial.print(", Data: ");
            Serial.write(packet.data(), packet.length());
            Serial.println();
            //reply to the client
            packet.printf("Got %u bytes of data", packet.length());
        });
        //Send multicast
        udp.print("Hello!");
    }
}

void loop()
{
    delay(1000);
    //Send multicast
    udp.print("Anyone here?");
}
