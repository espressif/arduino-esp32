#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Arduino.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include "Adafruit_SHT31.h"
#include <NTPClient.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "inHouse";
const char* password = "9093495900";

const String postAddress = "http://192.168.1.79:3000/germination";

// Enables ntp server for time and date
WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP);

int currentTime = 0;
 
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// The pins which will be connected to the 2 relay
int humidPin = 2;
int tempPin = 14;

// GPIO where the DS18B20 is connected to
const int oneWireBus = 0;     

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// the temperature (Cecius) and humidity (%) threshholds to trigger relay
float tempLimit = 22.78;
float humidLimit = 97.0;

// variables to influence clock controll behavior
// records time of last change in pin voltage
int humLastTime = 0;
int temLastTime = 0;
// {timeOn, timeOff} in seconds
int humTimeDuration[2] = {10, 20};
int temTimeDuration[2] = {5, 15};
 
void setup() {
  Serial.begin(115200);
  // Enables DS18B20
  sensors.begin();

  // Sets the digital pins as output
  pinMode(humidPin, OUTPUT);
  pinMode(tempPin, OUTPUT);

  // Initializes current epoch time.
  timeClient.begin();
  currentTime = timeClient.getEpochTime();
  
  OTAinit();
}

// Initializes OTA functionality for the 8266
void OTAinit() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  //WiFi.setHostname(“MyArduino”);
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    switch (error) {
      case OTA_AUTH_ERROR:
        Serial.println("Auth Failed");
        break;
      case OTA_BEGIN_ERROR:
        Serial.println("Begin Failed");
        break;
      case OTA_CONNECT_ERROR:
        Serial.println("Connect Failed");
        break;
      case OTA_RECEIVE_ERROR:
        Serial.println("Receive Failed");
        break;
      case OTA_END_ERROR:
        Serial.println("End Failed");
        break;
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Outputs measured temperature and humidity to serial monitor
void printShtMeasurments(float temp, float hum) {
   if (! isnan(temp)) {
    Serial.print("Temp *C = "); Serial.println(temp);
    //postProtocol("T: " + (String) temp);
  } else { 
    Serial.println("Failed to read temperature");
  }
 
  if (! isnan(hum)) {
    Serial.print("Hum. % = "); Serial.println(hum);
    //postProtocol("H: " + (String) hum);
  } else { 
    Serial.println("Failed to read humidity");
  }
  postProtocol(temp, hum);
}

// Sets relay pins to high or low depending on measurements
void relayPower(int pin, float current, float limit, int *lastTime) {
  int oldPinPow = digitalRead(pin);
  
  if(current < limit) {
    digitalWrite(pin, LOW);
  } else {
    digitalWrite(pin, HIGH);
  }
  
  if(oldPinPow != digitalRead(pin)) {
    *lastTime = currentTime;
  }
}

void shtAlgorithm() {
  // Reads temperature and humidity from sht31 and stores it to appropriate variables
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();
  
  // Displays current readings
  printShtMeasurments(t, h);

  // Sets relay relevent pins to appropriate voltage levels
  relayPower(humidPin, h, humidLimit, &humLastTime);
  relayPower(tempPin, t, tempLimit, &temLastTime);
}

void DS18B20Algorithm() {
  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);

  Serial.print(temperatureC);

  if(10 < temperatureC && temperatureC < 35) {
    relayPower(tempPin, temperatureC, tempLimit, &temLastTime);
    Serial.print(temperatureC);
    Serial.println("ºC");
    postProtocol(temperatureC, -1);
  }else {
    clockManager(tempPin, temTimeDuration, &temLastTime);
  }
  clockManager(humidPin, humTimeDuration, &humLastTime);
}

void clockManager(int pin, int times[], int *lastTime) {
  if((!digitalRead(pin) && (*lastTime + times[0] <= currentTime)) || (digitalRead(pin) && (*lastTime + times[1] <= currentTime))) {
    digitalWrite(pin, !digitalRead(pin));
    *lastTime = currentTime;
  }
}

//void postProtocol(String message) {
void postProtocol(float tem, float hum) {
  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status
 
   HTTPClient http;    //Declare object of class HTTPClient
 
   http.begin(postAddress);      //Specify request destination
   http.addHeader("Content-Type", "application/json");  //Specify content-type header
   
   int httpCode = http.POST("{\"temperature\": " + (String)tem + ", \"humidity\": " + (String)hum +"}");   //Send the request
   String payload = http.getString();                  //Get the response payload

   //Serial.println(httpCode);   //Print HTTP return code
   //Serial.println(payload);    //Print request response payload
 
   http.end();  //Close connection
 }else{
    Serial.println("Error in WiFi connection"); 
 }
}
 
void loop() {
  // Updates and records time to current.
  timeClient.update();
  currentTime = timeClient.getEpochTime();
  
  if(sht31.begin(0x44)) {
    shtAlgorithm();
  } else {
    DS18B20Algorithm();
  }

  // Checks for FOTA update
  ArduinoOTA.handle();

  Serial.println();

//  Commented code for testing purposes.
  Serial.println(currentTime);
  Serial.print("Humidity: ");
  Serial.println(digitalRead(humidPin));
  Serial.print("Temperature: ");
  Serial.println(digitalRead(tempPin));
  
  Serial.println(); 
  //
  delay(1000);
}
