/*
 * GetMacAddress
 *
 * This sketch prints out the MAC addresses for different interfaces.
 *
 * Written by: Daniel Nebert
 *
 * The first printed MAC address is obtained by calling 'esp_efuse_mac_get_default'
 * (https://docs.espressif.com/projects/esp-idf/en/release-v5.1/esp32/api-reference/system/misc_system_api.html#_CPPv425esp_efuse_mac_get_defaultP7uint8_t)
 * which returns base MAC address which is factory-programmed by Espressif in EFUSE.
 *
 * The remaining printed MAC addresses is obtained by calling 'esp_read_mac'
 * (https://docs.espressif.com/projects/esp-idf/en/release-v5.1/esp32/api-reference/system/misc_system_api.html#_CPPv412esp_read_macP7uint8_t14esp_mac_type_t)
 * and passing in the 'esp_mac_type_t' type.
 * (https://docs.espressif.com/projects/esp-idf/en/release-v5.1/esp32/api-reference/system/misc_system_api.html#esp__mac_8h_1a1b19aca597277a2179682869c140477a)
 *

esp_mac_type_t values:

  ESP_MAC_WIFI_STA - MAC for WiFi Station (6 bytes)
  ESP_MAC_WIFI_SOFTAP - MAC for WiFi Soft-AP (6 bytes)
  ESP_MAC_BT - MAC for Bluetooth (6 bytes)
  ESP_MAC_ETH - MAC for Ethernet (6 bytes)
  ESP_MAC_IEEE802154 - if CONFIG_SOC_IEEE802154_SUPPORTED=y, MAC for IEEE802154 (8 bytes)
  ESP_MAC_BASE - Base MAC for that used for other MAC types (6 bytes)
  ESP_MAC_EFUSE_FACTORY - MAC_FACTORY eFuse which was burned by Espressif in production (6 bytes)
  ESP_MAC_EFUSE_CUSTOM - MAC_CUSTOM eFuse which was can be burned by customer (6 bytes)
  ESP_MAC_EFUSE_EXT - if CONFIG_SOC_IEEE802154_SUPPORTED=y, MAC_EXT eFuse which is used as an extender for IEEE802154 MAC (2 bytes)

*/

#include "esp_mac.h"  // required - exposes esp_mac_type_t values

void setup() {

  Serial.begin(115200);

  Serial.println("Interface\t\t\t\t\t\tMAC address (6 bytes, 4 universally administered, default)");

  Serial.print("Wi-Fi Station (using 'esp_efuse_mac_get_default')\t");
  Serial.println(getDefaultMacAddress());

  Serial.print("WiFi Station (using 'esp_read_mac')\t\t\t");
  Serial.println(getInterfaceMacAddress(ESP_MAC_WIFI_STA));

  Serial.print("WiFi Soft-AP (using 'esp_read_mac')\t\t\t");
  Serial.println(getInterfaceMacAddress(ESP_MAC_WIFI_SOFTAP));

  Serial.print("Bluetooth (using 'esp_read_mac')\t\t\t");
  Serial.println(getInterfaceMacAddress(ESP_MAC_BT));

  Serial.print("Ethernet (using 'esp_read_mac')\t\t\t\t");
  Serial.println(getInterfaceMacAddress(ESP_MAC_ETH));
}

void loop() { /* Nothing in loop */ }

String getDefaultMacAddress() {

  String mac = "";

  unsigned char mac_base[6] = {0};

  if (esp_efuse_mac_get_default(mac_base) == ESP_OK) {
    char buffer[18];  // 6*2 characters for hex + 5 characters for colons + 1 character for null terminator
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0], mac_base[1], mac_base[2], mac_base[3], mac_base[4], mac_base[5]);
    mac = buffer;
  }

  return mac;
}

String getInterfaceMacAddress(esp_mac_type_t interface) {

  String mac = "";

  unsigned char mac_base[6] = {0};

  if (esp_read_mac(mac_base, interface) == ESP_OK) {
    char buffer[18];  // 6*2 characters for hex + 5 characters for colons + 1 character for null terminator
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0], mac_base[1], mac_base[2], mac_base[3], mac_base[4], mac_base[5]);
    mac = buffer;
  }

  return mac;
}
