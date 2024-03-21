/*
    ESP-NOW Serial Example - Unicast transmission
    Lucas Saavedra Vaz - 2024
    Send data between two ESP32s using the ESP-NOW protocol in one-to-one (unicast) configuration.
    Note that different MAC addresses are used for different interfaces.
    To properly visualize the data being sent, set the line ending in the Serial Monitor to "Both NL & CR".
*/

#include "ESP32_NOW_Serial.h"
#include "MacAddress.h"
#include "WiFi.h"

#define ESPNOW_WIFI_MODE_STATION 0
#define ESPNOW_WIFI_CHANNEL 1

#if ESPNOW_WIFI_MODE_STATION // ESP-NOW using WiFi Station mode
#define ESPNOW_WIFI_IF   WIFI_IF_STA
#define GET_IF_MAC       WiFi.macAddress

// Station mode MAC addresses of the devices in the network
// Device 1 - F4:12:FA:42:B6:E8
const MacAddress mac1({0xF4, 0x12, 0xFA, 0x42, 0xB6, 0xE8});
// Device 2 - F4:12:FA:40:64:4C
const MacAddress mac2({0xF4, 0x12, 0xFA, 0x40, 0x64, 0x4C});
#else // ESP-NOW using WiFi AP mode
#define ESPNOW_WIFI_IF   WIFI_IF_AP
#define GET_IF_MAC       WiFi.softAPmacAddress

// AP mode MAC addresses of the devices in the network
// Device 1 - F6:12:FA:42:B6:E8
const MacAddress mac1({0xF6, 0x12, 0xFA, 0x42, 0xB6, 0xE8});
// Device 2 - F6:12:FA:40:64:4C
const MacAddress mac2({0xF6, 0x12, 0xFA, 0x40, 0x64, 0x4C});
#endif

ESP_NOW_Serial_Class *esp_now_serial;

void setup() {
    MacAddress current_mac;

    Serial.begin(115200);

    Serial.print("WiFi Mode: ");

    #if ESPNOW_WIFI_MODE_STATION
    Serial.println("STA");
    WiFi.mode(ESPNOW_WIFI_MODE);
    esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    #else
    Serial.println("AP");
    WiFi.softAP(WiFi.getHostname(), NULL, ESPNOW_WIFI_CHANNEL, 1);
    #endif

    Serial.print("Channel: ");
    Serial.println(ESPNOW_WIFI_CHANNEL);

    String mac = GET_IF_MAC();
    Serial.print("MAC Address: ");
    current_mac.fromString(mac);
    Serial.println(mac);

    WiFi.disconnect();

    if (current_mac == mac1)
    {
        Serial.println("I'm the Device 1\n");
        // Pass the MAC address of the other device to the ESP_NOW_Serial_Class
        esp_now_serial = new ESP_NOW_Serial_Class(mac2, ESPNOW_WIFI_CHANNEL, ESPNOW_WIFI_IF);
    }
    else if (current_mac == mac2)
    {
        Serial.println("I'm the Device 2\n");
        // Pass the MAC address of the other device to the ESP_NOW_Serial_Class
        esp_now_serial = new ESP_NOW_Serial_Class(mac1, ESPNOW_WIFI_CHANNEL, ESPNOW_WIFI_IF);
    }
    else
    {
        Serial.println("Unknown MAC address. Please update the mac addresses in the sketch\n");
        while(1);
    }

    // Start the ESP-NOW communication
    Serial.println("ESP-NOW communication starting...");
    esp_now_serial->begin(115200);
    Serial.println("You can now send data between the devices using the Serial Monitor\n");
}

void loop() {
    while (esp_now_serial->available() > 0)
    {
        Serial.write(esp_now_serial->read());
    }

    while (Serial.available() && esp_now_serial->availableForWrite())
    {
        if (esp_now_serial->write(Serial.read()) <= 0)
        {
            Serial.println("Failed to send data");
            break;
        }
    }

    delay(1);
}
