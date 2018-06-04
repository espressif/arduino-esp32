//Github profile of editor: https://github.com/martinius96
//E-mail: martinius96@gmail.com
//Sketch was tested under Eduroam network and also under local LAN RADIUS server for 802.1x (WPA2 Enterprise AAA)
//Info: Sketch is able to connect to 802.1x network and then connect to internet address in client mode (STA) over port 80 HTTP.
//Please inform me about Eduroam networks if it working there (for my list of verified Eduroam places via E-mail)
#include "esp_wpa2.h"
#include <WiFi.h>
String line; //variable for response
const char* ssid = "eduroam"; // SSID of 802.1x network
const char* host = "arduino.php5.sk"; //external server domain for test HTTP connection after connecting to WPA2 Network
#define EAP_IDENTITY "login@university.com" //identity
#define EAP_PASSWORD "password" //password
void setup() {
    Serial.begin(115200);
    delay(10);
    Serial.println();
    Serial.print("Connecting to network ");
    Serial.println(ssid);
    WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide identity, identity==username
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide username
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)); //provide password
    esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
    esp_wifi_sta_wpa2_ent_enable(&config);
   Serial.println("MAC address: ");
   Serial.println(WiFi.macAddress()); //Print our MAC address of our device
    WiFi.begin(ssid); //connect to 802.1x (WPA2 Enterprise network)
 WiFi.setHostname("ESP32Name"); //set Hostname for your device - not neccesary, you can delete it
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: "); 
    Serial.println(WiFi.localIP()); //print LAN IP

}
void loop() {
 if (WiFi.status() != WL_CONNECTED) { //if we lost connection, retry
        WiFi.begin(ssid);        
    }
 delay(5000); 
    Serial.print("Connecting to website: ");
    Serial.println(host);
    WiFiClient client;
    if (!client.connect(host, 80)) { // HTTP connection on port 80
        Serial.println("Connection lost! - Failed response");
    }
    String url = "/rele/rele1.txt"; //read .txt file    
    Serial.print("Requesting URL: ");
    Serial.println(url);

    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            Serial.println("Client timed out! - retry");
            client.stop();
        }
    }
    while(client.available()) {
         line = client.readStringUntil('\n');
        Serial.println(line);  //print variable from .txt file
    }
    Serial.println();
    Serial.println("End connection");
}
