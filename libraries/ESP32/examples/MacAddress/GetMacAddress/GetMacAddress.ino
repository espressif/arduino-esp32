/*
 * GetMacAddress
 * 
 * This sketch prints out the MAC addresses for different interfaces.
 * 
 * Written by: Daniel Nebert
 * 
 * The first printed MAC address is obtained by calling 'esp_efuse_mac_get_default'
 * (https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html#_CPPv425esp_efuse_mac_get_defaultP7uint8_t)
 * which returns base MAC address which is factory-programmed by Espressif in EFUSE.
 * 
 * The remaining printed MAC addresses is obtained by calling 'esp_read_mac'
 * (https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html#_CPPv412esp_read_macP7uint8_t14esp_mac_type_t)
 * and passing in the 'esp_mac_type_t' type.
 * (https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html#esp__mac_8h_1a1b19aca597277a2179682869c140477a)
 * 

enum esp_mac_type_t

  Values:
  
  enumerator ESP_MAC_WIFI_STA
    MAC for WiFi Station (6 bytes)
  
  enumerator ESP_MAC_WIFI_SOFTAP
    MAC for WiFi Soft-AP (6 bytes)
  
  enumerator ESP_MAC_BT
    MAC for Bluetooth (6 bytes)
  
  enumerator ESP_MAC_ETH
    MAC for Ethernet (6 bytes)
  
  enumerator ESP_MAC_IEEE802154
    if CONFIG_SOC_IEEE802154_SUPPORTED=y, MAC for IEEE802154 (8 bytes)
  
  enumerator ESP_MAC_BASE
    Base MAC for that used for other MAC types (6 bytes)
  
  enumerator ESP_MAC_EFUSE_FACTORY
    MAC_FACTORY eFuse which was burned by Espressif in production (6 bytes)
  
  enumerator ESP_MAC_EFUSE_CUSTOM
    MAC_CUSTOM eFuse which was can be burned by customer (6 bytes)
  
  enumerator ESP_MAC_EFUSE_EXT
    if CONFIG_SOC_IEEE802154_SUPPORTED=y, MAC_EXT eFuse which is used as an extender for IEEE802154 MAC (2 bytes)

*/

void setup() {
	Serial.begin(115200);
  vTaskDelay(250 / portTICK_PERIOD_MS ); 
  
  Serial.println("Interface\t\t\t\t\t\tMAC address (6 bytes, 4 universally administered, default)");  
  Serial.print("Wi-Fi Station (using 'esp_efuse_mac_get_default')\t"); Serial.println(getDefaultMacAddress().c_str());  
  Serial.print("WiFi Station (using 'esp_read_mac')\t\t\t");  Serial.println(getInterfaceMacAddress(ESP_MAC_WIFI_STA).c_str());
  Serial.print("WiFi Soft-AP (using 'esp_read_mac')\t\t\t");  Serial.println(getInterfaceMacAddress(ESP_MAC_WIFI_SOFTAP).c_str());
  Serial.print("Bluetooth (using 'esp_read_mac')\t\t\t");  Serial.println(getInterfaceMacAddress(ESP_MAC_BT).c_str());
  Serial.print("Ethernet (using 'esp_read_mac')\t\t\t\t");  Serial.println(getInterfaceMacAddress(ESP_MAC_ETH).c_str());
}

void loop() { vTaskDelete(NULL); }

std::string getDefaultMacAddress() {
  std::string mac = "";
  unsigned char mac_base[6] = {0};
  if(esp_efuse_mac_get_default(mac_base) == ESP_OK) {
    char buffer[18]; // 6*2 characters for hex + 5 characters for colons + 1 character for null terminator
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0], mac_base[1], mac_base[2], mac_base[3], mac_base[4], mac_base[5]);
    mac = buffer;
  }
  return mac;
}

std::string getInterfaceMacAddress(esp_mac_type_t interface) {
  std::string mac = "";
  unsigned char mac_base[6] = {0};
  if(esp_read_mac(mac_base, interface) == ESP_OK) {
    char buffer[18]; // 6*2 characters for hex + 5 characters for colons + 1 character for null terminator
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0], mac_base[1], mac_base[2], mac_base[3], mac_base[4], mac_base[5]);
    mac = buffer;
  }
  return mac;
}
