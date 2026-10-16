// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @brief This example demonstrates Zigbee weather sensors updated over Wi-Fi.
 *
 * The example fetches outdoor temperature, humidity and pressure from Open-Meteo
 * over Wi-Fi, then reports them on Zigbee endpoints. Zigbee.stop() / Zigbee.start()
 * free the shared radio for Wi-Fi on dual-radio SoCs (ESP32-C6, ESP32-S31).
 *
 * The device is a Zigbee end device so pausing Zigbee does not interrupt mesh routing.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include <WiFi.h>
#include <HTTPClient.h>

/* Zigbee temperature + humidity + pressure sensor configuration */
#define TEMP_HUM_SENSOR_ENDPOINT_NUMBER 1
#define PRESSURE_SENSOR_ENDPOINT_NUMBER 2

/* How often to pause Zigbee, fetch weather over Wi-Fi, and resume */
#define WIFI_FETCH_INTERVAL_MS (60 * 1000)

/* Fallback defaults if the first Wi-Fi fetch fails */
#define FALLBACK_TEMPERATURE 20.0f
#define FALLBACK_HUMIDITY    50.0f
#define FALLBACK_PRESSURE    1013.0f

/* Wi-Fi credentials — replace with your network */
const char *ssid = "your-ssid";
const char *password = "your-password";

/*
 * Open-Meteo forecast API (no API key). Change latitude/longitude as needed.
 * Default: Brno, Czech Republic
 */
const char *weatherUrl =
  "http://api.open-meteo.com/v1/forecast?latitude=49.1952&longitude=16.608&current=temperature_2m,relative_humidity_2m,pressure_msl";

uint8_t button = BOOT_PIN;

ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_HUM_SENSOR_ENDPOINT_NUMBER);
ZigbeePressureSensor zbPressureSensor = ZigbeePressureSensor(PRESSURE_SENSOR_ENDPOINT_NUMBER);

float lastTemperature = FALLBACK_TEMPERATURE;
float lastHumidity = FALLBACK_HUMIDITY;
float lastPressure = FALLBACK_PRESSURE;
unsigned long lastFetchMs = 0;

/********************* Wi-Fi + weather **************************/
// Parse a number from Open-Meteo's "current" object (skip "current_units" string values)
bool parseCurrentValue(const String &payload, const char *key, float &value) {
  int currentIdx = payload.indexOf("\"current\":{");
  if (currentIdx < 0) {
    Serial.println("Open-Meteo \"current\" object not found");
    return false;
  }
  int idx = payload.indexOf(key, currentIdx);
  if (idx < 0) {
    Serial.printf("%s not found in response\r\n", key);
    return false;
  }
  value = payload.substring(idx + strlen(key)).toFloat();
  return true;
}

bool fetchWeather(float &temperature, float &humidity, float &pressure) {
  HTTPClient http;
  if (!http.begin(weatherUrl)) {
    Serial.println("HTTP begin failed");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed: %s\r\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  if (!parseCurrentValue(payload, "\"temperature_2m\":", temperature)) {
    return false;
  }
  if (!parseCurrentValue(payload, "\"relative_humidity_2m\":", humidity)) {
    return false;
  }
  if (!parseCurrentValue(payload, "\"pressure_msl\":", pressure)) {
    return false;
  }
  return true;
}

// Connect Wi-Fi, fetch weather, then turn Wi-Fi off.
// Call only while Zigbee is not using the radio (before Zigbee.begin(), or after Zigbee.stop()).
bool connectWifiAndFetchWeather(float &temperature, float &humidity, float &pressure) {
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  bool ok = false;
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    ok = fetchWeather(temperature, humidity, pressure);
    if (ok) {
      Serial.printf("Outdoor weather: %.2f°C, %.1f%% RH, %.0f hPa\r\n", temperature, humidity, pressure);
    } else {
      Serial.println("Weather fetch failed");
    }
  } else {
    Serial.println("WiFi connection failed");
  }

  WiFi.disconnect(false, false, 1000);
  WiFi.mode(WIFI_OFF);
  delay(500);  // let Wi-Fi finish radio cleanup before Zigbee.start()
  Serial.println("WiFi disconnected");
  return ok;
}

void reportWeather() {
  zbTempSensor.setTemperature(lastTemperature);
  zbTempSensor.setHumidity(lastHumidity);
  zbPressureSensor.setPressure((int16_t)lastPressure);

  // Space reports slightly after Zigbee.start() so APS is ready
  zbTempSensor.reportTemperature();
  delay(50);
  zbTempSensor.reportHumidity();
  delay(50);
  zbPressureSensor.report();

  Serial.printf(
    "Reported temperature: %.2f°C, Humidity: %.2f%%, Pressure: %.0f hPa\r\n", lastTemperature, lastHumidity, lastPressure
  );
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Init button switch
  pinMode(button, INPUT_PULLUP);

  // Fetch weather before Zigbee starts, so defaults can be set from real data
  Serial.println("Fetching initial weather over Wi-Fi (before Zigbee)...");
  if (connectWifiAndFetchWeather(lastTemperature, lastHumidity, lastPressure)) {
    Serial.println("Using Open-Meteo values as Zigbee attribute defaults");
  } else {
    lastTemperature = FALLBACK_TEMPERATURE;
    lastHumidity = FALLBACK_HUMIDITY;
    lastPressure = FALLBACK_PRESSURE;
    Serial.println("Wi-Fi fetch failed — using fallback defaults");
  }

  // Initialize Zigbee stack as end device
  if (!Zigbee.role(ZIGBEE_END_DEVICE)) {
    Serial.println("Zigbee failed to init!");
    Serial.println("Rebooting...");
    delay(1000);
    ESP.restart();
  }

  // Optional: set Zigbee device name and model
  zbTempSensor.setManufacturerAndModel("Espressif", "ZigbeeWiFiWeather");

  // Set minimum and maximum temperature measurement value
  zbTempSensor.setMinMaxValue(-40, 80);

  // Optional: Set default (initial) value from the Wi-Fi fetch
  zbTempSensor.setDefaultValue(lastTemperature);

  // Optional: Set tolerance for temperature measurement in °C (lowest possible value is 0.01°C)
  zbTempSensor.setTolerance(0.1);

  // Add humidity cluster to the temperature sensor with min, max, tolerance and default values
  zbTempSensor.addHumiditySensor(0, 100, 1, lastHumidity);

  // Optional: set Zigbee device name and model
  zbPressureSensor.setManufacturerAndModel("Espressif", "ZigbeeWiFiWeather");

  // Set minimum and maximum pressure measurement value in hPa
  zbPressureSensor.setMinMaxValue(800, 1100);

  // Optional: Set default (initial) value from the Wi-Fi fetch
  zbPressureSensor.setDefaultValue((int16_t)lastPressure);

  // Optional: Set tolerance for pressure measurement in hPa
  zbPressureSensor.setTolerance(1);

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbTempSensor);
  Zigbee.addEndpoint(&zbPressureSensor);

  Serial.println("Starting Zigbee...");
  // When all EPs are registered, start Zigbee
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  } else {
    Serial.println("Zigbee started successfully!");
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Set reporting interval for measurements in seconds, must be called after Zigbee.begin()
  // min_interval and max_interval in seconds, delta (temp in 0.1 °C, humidity in 0.01 %, pressure in hPa)
  zbTempSensor.setReporting(0, 60, 0.5);
  zbTempSensor.setHumidityReporting(0, 60, 1.0);
  zbPressureSensor.setReporting(0, 60, 1);

  // Report the initial Wi-Fi values once the device is on the network
  reportWeather();

  lastFetchMs = millis();
}

void loop() {
  // Checking button for factory reset and reporting
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
    reportWeather();
  }

  // Pause Zigbee, fetch weather over Wi-Fi, then resume and report
  if ((millis() - lastFetchMs) >= WIFI_FETCH_INTERVAL_MS) {
    lastFetchMs = millis();

    Serial.println("Stopping Zigbee stack for Wi-Fi...");
    Zigbee.stop();
    delay(100);

    float temperature, humidity, pressure;
    if (connectWifiAndFetchWeather(temperature, humidity, pressure)) {
      lastTemperature = temperature;
      lastHumidity = humidity;
      lastPressure = pressure;
    }

    Serial.println("Starting Zigbee stack again...");
    Zigbee.start();

    // Wait until the end device can talk to its parent again
    unsigned long start = millis();
    while (!Zigbee.connected() && (millis() - start) < 10000) {
      delay(50);
    }

    reportWeather();
  }

  delay(100);
}
