/*
 *  This sketch demonstrates how to scan WiFi networks.
 *  The API is almost the same as with the WiFi Shield library,
 *  the most obvious difference being the different file you need to include:
 */
#include "WiFi.h"

/* These are the GPIOs connected to the antenna switch on the ESP32-WROOM-DA.
 * Both GPIOs are not exposed to the module pins and cannot be used except to
 * control the antnnas switch.
 *
 * For more details, see the datashhet at:
 * https://www.espressif.com/sites/default/files/documentation/esp32-wroom-da_datasheet_en.pdf
 */

#define GPIO_ANT1 2   // GPIO for antenna 1
#define GPIO_ANT2 25  // GPIO for antenna 2 (default)

void setup() {
  bool err = false;
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);

  /* Attention: This is the manual procedure for the dual antenna configuration.
     * If you choose the ESP32-WROOM-DA module from the Tools -> Board, this configuration
     * is not necessary!
     *
     * Set WiFi dual antenna configuration by passing the GPIO and antenna mode for RX ant TX
     */
  err = WiFi.setDualAntennaConfig(GPIO_ANT1, GPIO_ANT2, WIFI_RX_ANT_AUTO, WIFI_TX_ANT_AUTO);

  /* For more details on how to use this feature, see our docs:
     * https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html
     */

  if (err == false) {
    Serial.println("Dual Antenna configuration failed!");
  } else {
    Serial.println("Dual Antenna configuration successfully done!");
  }

  WiFi.disconnect();
  delay(100);

  Serial.println("Setup done");
}

void loop() {
  Serial.println("scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");

  // Wait a bit before scanning again
  delay(5000);
}
