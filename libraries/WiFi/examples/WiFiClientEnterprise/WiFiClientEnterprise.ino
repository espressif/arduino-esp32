//Sketch edited by: martinius96 (Martin Chlebovec)
//Personal website: https://arduino.php5.sk
#include "esp_wpa2.h"
#include <WiFi.h>
#define EAP_IDENTITY "login@university.domain" //eduroam login --> identity@youruniversity.domain
#define EAP_PASSWORD "password" //your Eduroam password
String line; //variable for response
const char* ssid = "eduroam"; // Eduroam SSID
const char* host = "arduino.php5.sk"; //external server domain for HTTP connection after authentification
void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.print("Connecting to network: ");
  Serial.println(ssid);
  WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
  WiFi.mode(WIFI_STA);
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide identity
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide username
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)); //provide password
  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT(); //set config to default (fixed for 2018 and Arduino 1.8.5+)
  esp_wifi_sta_wpa2_ent_enable(&config); //set config to enable function (fixed for 2018 and Arduino 1.8.5+)
  WiFi.begin(ssid); //connect to Eduroam function
  WiFi.setHostname("ESP32Name"); //set Hostname for your device - not neccesary
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address set: "); 
  Serial.println(WiFi.localIP()); //print LAN IP

}
void loop() {
  delay(5000);
  if (WiFi.status() != WL_CONNECTED) { //if we lost connection, retry
    WiFi.begin(ssid);
    delay(500);        
}
  Serial.print("Connecting to website: ");
  Serial.println(host);
  WiFiClient client;
  if (!client.connect(host, 80)) { // HTTP connection on port 80
    Serial.println("Connection lost! - Failed response");
  }
  String url = "/rele/rele1.txt"; //read .txt file    
  Serial.print("Requesting URL: ");
  Serial.println(url);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("Client timed out! - retry");
    }
  }
  while(client.available()) {
    line = client.readStringUntil('\n');
    Serial.println(line);  
  }
  Serial.println();
  Serial.println("End connection");
  client.stop();
}
