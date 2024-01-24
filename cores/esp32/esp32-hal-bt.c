// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp32-hal-bt.h"

#if SOC_BT_SUPPORTED
#ifdef CONFIG_BT_ENABLED

#if CONFIG_IDF_TARGET_ESP32
bool btInUse(){ return true; }
#else
// user may want to change it to free resources
__attribute__((weak)) bool btInUse(){ return true; }
#endif

#include "esp_bt.h"

#ifdef CONFIG_BTDM_CONTROLLER_MODE_BTDM
#define BT_MODE ESP_BT_MODE_BTDM
#elif defined(CONFIG_BTDM_CONTROLLER_MODE_BR_EDR_ONLY)
#define BT_MODE ESP_BT_MODE_CLASSIC_BT
#else
#define BT_MODE ESP_BT_MODE_BLE
#endif

bool btStarted(){
    return (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED);
}

bool btStart() {
    return btStartMode(BT_MODE);
}

bool btStartMode(bt_mode mode){
    esp_bt_mode_t esp_bt_mode;
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#if CONFIG_IDF_TARGET_ESP32
    switch(mode) {
        case BT_MODE_BLE: esp_bt_mode=ESP_BT_MODE_BLE;
        break;
        case BT_MODE_CLASSIC_BT: esp_bt_mode=ESP_BT_MODE_CLASSIC_BT;
        break;
        case BT_MODE_BTDM: esp_bt_mode=ESP_BT_MODE_BTDM;
        break;
        default: esp_bt_mode=BT_MODE;
        break;
    }
    // esp_bt_controller_enable(MODE) This mode must be equal as the mode in “cfg” of esp_bt_controller_init().
    cfg.mode=esp_bt_mode;
    if(cfg.mode == ESP_BT_MODE_CLASSIC_BT) {
        esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    }
#else
// other esp variants dont support BT-classic / DM.
    esp_bt_mode=BT_MODE;
#endif

    if(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED){
        return true;
    }
    esp_err_t ret;
    if(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE){
        if((ret = esp_bt_controller_init(&cfg)) != ESP_OK) {
            log_e("initialize controller failed: %s", esp_err_to_name(ret));
            return false;
        }
        while(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE){}
    }
    if(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED){
        if((ret = esp_bt_controller_enable(esp_bt_mode)) != ESP_OK) {
            log_e("BT Enable mode=%d failed %s", BT_MODE, esp_err_to_name(ret));
            return false;
        }
    }
    if(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED){
        return true;
    }
    log_e("BT Start failed");
    return false;
}

bool btStop(){
    if(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE){
        return true;
    }
    if(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED){
        if (esp_bt_controller_disable()) {
            log_e("BT Disable failed");
            return false;
        }
        while(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED);
    }
    if(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED){
        if (esp_bt_controller_deinit()) {
			log_e("BT deint failed");
			return false;
		}
		vTaskDelay(1);
		if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE) {			
			return false;		
		}
        return true;
    }
    log_e("BT Stop failed");
    return false;
}

#else // CONFIG_BT_ENABLED
bool btStarted()
{
    return false;
}

bool btStart()
{
    return false;
}

bool btStop()
{
    return false;
}

#endif /* CONFIG_BT_ENABLED */

#endif /* SOC_BT_SUPPORTED */