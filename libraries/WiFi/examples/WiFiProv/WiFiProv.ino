#include "WiFi.h"
void SysProvEvent(system_event_t *sys_event,wifi_prov_event_t *prov_event)
{
    if(sys_event) {
      switch (sys_event->event_id) {
      case SYSTEM_EVENT_STA_GOT_IP:
          Serial.print("\nConnected IP address : ");
          Serial.println(ip4addr_ntoa(&sys_event->event_info.got_ip.ip_info.ip));
          break;
      case SYSTEM_EVENT_STA_DISCONNECTED:
          Serial.println("\nDisconnected. Connecting to the AP again... ");
          break;
      default:
          break;
      }      
    }

    if(prov_event) {
        switch (prov_event->event) {
        case WIFI_PROV_START:
            Serial.println("\nProvisioning started\nGive Credentials of your access point using \" Android app \"");
            break;
        case WIFI_PROV_CRED_RECV: { 
            Serial.println("\nReceived Wi-Fi credentials");
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)prov_event->event_data;
            Serial.print("\tSSID : ");
            Serial.println((const char *) wifi_sta_cfg->ssid);
            Serial.print("\tPassword : ");
            Serial.println((char const *) wifi_sta_cfg->password);
            break;
        }
        case WIFI_PROV_CRED_FAIL: { 
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)prov_event->event_data;
            Serial.println("\nProvisioning failed!\nPlease reset to factory and retry provisioning\n");
            if(*reason == WIFI_PROV_STA_AUTH_ERROR) 
                Serial.println("\nWi-Fi AP password incorrect");
            else
                Serial.println("\nWi-Fi AP not found....Add API \" nvs_flash_erase() \" before beginProvision()");        
            break;
        }
        case WIFI_PROV_CRED_SUCCESS:
            Serial.println("\nProvisioning Successful");
            break;
        case WIFI_PROV_END:
            Serial.println("\nProvisioning Ends");
            break;
        default:
            break;
        }      
    }
}

void setup() {
  Serial.begin(115200);
  //Sample uuid that user can pass during provisioning using BLE
  /* uint8_t uuid[16] = {0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
                   0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02 };*/
  WiFi.onEvent(SysProvEvent);
  WiFi.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, "abcd1234", "BLE_XXX", NULL, NULL);
}

void loop() {
}
