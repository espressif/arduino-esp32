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

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "soc/rtc.h"
#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2)
#include "soc/rtc_cntl_reg.h"
#include "soc/apb_ctrl_reg.h"
#endif
#include "soc/efuse_reg.h"
#include "esp32-hal.h"
#include "esp32-hal-cpu.h"

#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "freertos/xtensa_timer.h"
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "freertos/xtensa_timer.h"
#include "esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "freertos/xtensa_timer.h"
#include "esp32s3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C2
#include "esp32c2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C6
#include "esp32c6/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32H2
#include "esp32h2/rom/rtc.h"
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/rtc.h"
#endif

typedef struct apb_change_cb_s {
        struct apb_change_cb_s * prev;
        struct apb_change_cb_s * next;
        void * arg;
        apb_change_cb_t cb;
} apb_change_t;


static apb_change_t * apb_change_callbacks = NULL;
static SemaphoreHandle_t apb_change_lock = NULL;

static void initApbChangeCallback(){
    static volatile bool initialized = false;
    if(!initialized){
        initialized = true;
        apb_change_lock = xSemaphoreCreateMutex();
        if(!apb_change_lock){
            initialized = false;
        }
    }
}

static void triggerApbChangeCallback(apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb){
    initApbChangeCallback();
    xSemaphoreTake(apb_change_lock, portMAX_DELAY);
    apb_change_t * r = apb_change_callbacks;
    if( r != NULL ){
        if(ev_type == APB_BEFORE_CHANGE )
            while(r != NULL){
                r->cb(r->arg, ev_type, old_apb, new_apb);
                r=r->next;
            }
        else { // run backwards through chain
            while(r->next != NULL) r = r->next; // find first added
            while( r != NULL){
                r->cb(r->arg, ev_type, old_apb, new_apb);
                r=r->prev;
            }
        }
    }
    xSemaphoreGive(apb_change_lock);
}

bool addApbChangeCallback(void * arg, apb_change_cb_t cb){
    initApbChangeCallback();
    apb_change_t * c = (apb_change_t*)malloc(sizeof(apb_change_t));
    if(!c){
        log_e("Callback Object Malloc Failed");
        return false;
    }
    c->next = NULL;
    c->prev = NULL;
    c->arg = arg;
    c->cb = cb;
    xSemaphoreTake(apb_change_lock, portMAX_DELAY);
    if(apb_change_callbacks == NULL){
        apb_change_callbacks = c;
    } else {
        apb_change_t * r = apb_change_callbacks;
        // look for duplicate callbacks
        while( (r != NULL ) && !((r->cb == cb) && ( r->arg == arg))) r = r->next;
        if (r) {
            log_e("duplicate func=%8p arg=%8p",c->cb,c->arg);
            free(c);
            xSemaphoreGive(apb_change_lock);
            return false;
        }
        else {
            c->next = apb_change_callbacks;
            apb_change_callbacks-> prev = c;
            apb_change_callbacks = c;
        }
    }
    xSemaphoreGive(apb_change_lock);
    return true;
}

bool removeApbChangeCallback(void * arg, apb_change_cb_t cb){
    initApbChangeCallback();
    xSemaphoreTake(apb_change_lock, portMAX_DELAY);
    apb_change_t * r = apb_change_callbacks;
    // look for matching callback
    while( (r != NULL ) && !((r->cb == cb) && ( r->arg == arg))) r = r->next;
    if ( r == NULL ) {
        log_e("not found func=%8p arg=%8p",cb,arg);
        xSemaphoreGive(apb_change_lock);
        return false;
        }
    else {
        // patch links
        if(r->prev) r->prev->next = r->next;
        else { // this is first link
           apb_change_callbacks = r->next;
        }
        if(r->next) r->next->prev = r->prev;
        free(r);
    }
    xSemaphoreGive(apb_change_lock);
    return true;
}

static uint32_t calculateApb(rtc_cpu_freq_config_t * conf){
#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32H2
	return APB_CLK_FREQ;
#else
    if(conf->freq_mhz >= 80){
        return 80 * MHZ;
    }
    return (conf->source_freq_mhz * MHZ) / conf->div;
#endif
}

void esp_timer_impl_update_apb_freq(uint32_t apb_ticks_per_us); //private in IDF

bool setCpuFrequencyMhz(uint32_t cpu_freq_mhz){
    rtc_cpu_freq_config_t conf, cconf;
    uint32_t capb, apb;
    //Get XTAL Frequency and calculate min CPU MHz
#ifndef CONFIG_IDF_TARGET_ESP32H2
    rtc_xtal_freq_t xtal = rtc_clk_xtal_freq_get();
#endif
#if CONFIG_IDF_TARGET_ESP32
    if(xtal > RTC_XTAL_FREQ_AUTO){
        if(xtal < RTC_XTAL_FREQ_40M) {
            if(cpu_freq_mhz <= xtal && cpu_freq_mhz != xtal && cpu_freq_mhz != (xtal/2)){
                log_e("Bad frequency: %u MHz! Options are: 240, 160, 80, %u and %u MHz", cpu_freq_mhz, xtal, xtal/2);
                return false;
            }
        } else if(cpu_freq_mhz <= xtal && cpu_freq_mhz != xtal && cpu_freq_mhz != (xtal/2) && cpu_freq_mhz != (xtal/4)){
            log_e("Bad frequency: %u MHz! Options are: 240, 160, 80, %u, %u and %u MHz", cpu_freq_mhz, xtal, xtal/2, xtal/4);
            return false;
        }
    }
#endif
#ifndef CONFIG_IDF_TARGET_ESP32H2
    if(cpu_freq_mhz > xtal && cpu_freq_mhz != 240 && cpu_freq_mhz != 160 && cpu_freq_mhz != 120 && cpu_freq_mhz != 80){
        if(xtal >= RTC_XTAL_FREQ_40M){
            log_e("Bad frequency: %u MHz! Options are: 240, 160, 120, 80, %u, %u and %u MHz", cpu_freq_mhz, xtal, xtal/2, xtal/4);
        } else {
            log_e("Bad frequency: %u MHz! Options are: 240, 160, 120, 80, %u and %u MHz", cpu_freq_mhz, xtal, xtal/2);
        }
        return false;
    }
#endif
#if CONFIG_IDF_TARGET_ESP32
    //check if cpu supports the frequency
    if(cpu_freq_mhz == 240){
        //Check if ESP32 is rated for a CPU frequency of 160MHz only
        if (REG_GET_BIT(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_CPU_FREQ_RATED) &&
            REG_GET_BIT(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_CPU_FREQ_LOW)) {
            log_e("Can not switch to 240 MHz! Chip CPU frequency rated for 160MHz.");
            cpu_freq_mhz = 160;
        }
    }
#endif
    //Get current CPU clock configuration
    rtc_clk_cpu_freq_get_config(&cconf);
    //return if frequency has not changed
    if(cconf.freq_mhz == cpu_freq_mhz){
        return true;
    }
    //Get configuration for the new CPU frequency
    if(!rtc_clk_cpu_freq_mhz_to_config(cpu_freq_mhz, &conf)){
        log_e("CPU clock could not be set to %u MHz", cpu_freq_mhz);
        return false;
    }
    //Current APB
    capb = calculateApb(&cconf);
    //New APB
    apb = calculateApb(&conf);
    
    //Call peripheral functions before the APB change
    if(apb_change_callbacks){
        triggerApbChangeCallback(APB_BEFORE_CHANGE, capb, apb);
    }
    //Make the frequency change
    rtc_clk_cpu_freq_set_config_fast(&conf);
#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2)
    if(capb != apb){
        //Update REF_TICK (uncomment if REF_TICK is different than 1MHz)
        //if(conf.freq_mhz < 80){
        //    ESP_REG(APB_CTRL_XTAL_TICK_CONF_REG) = conf.freq_mhz / (REF_CLK_FREQ / MHZ) - 1;
        // }
        //Update APB Freq REG
        rtc_clk_apb_freq_update(apb);
        //Update esp_timer divisor
        esp_timer_impl_update_apb_freq(apb / MHZ);
    }
#endif
    //Update FreeRTOS Tick Divisor
#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2

#elif CONFIG_IDF_TARGET_ESP32S3

#else
    uint32_t fcpu = (conf.freq_mhz >= 80)?(conf.freq_mhz * MHZ):(apb);
    _xt_tick_divisor = fcpu / XT_TICK_PER_SEC;
#endif
    //Call peripheral functions after the APB change
    if(apb_change_callbacks){
        triggerApbChangeCallback(APB_AFTER_CHANGE, capb, apb);
    }
#ifdef SOC_CLK_APLL_SUPPORTED
    log_d("%s: %u / %u = %u Mhz, APB: %u Hz", (conf.source == RTC_CPU_FREQ_SRC_PLL)?"PLL":((conf.source == RTC_CPU_FREQ_SRC_APLL)?"APLL":((conf.source == RTC_CPU_FREQ_SRC_XTAL)?"XTAL":"8M")), conf.source_freq_mhz, conf.div, conf.freq_mhz, apb);
#else
    log_d("%s: %u / %u = %u Mhz, APB: %u Hz", (conf.source == RTC_CPU_FREQ_SRC_PLL)?"PLL":((conf.source == RTC_CPU_FREQ_SRC_XTAL)?"XTAL":"17.5M"), conf.source_freq_mhz, conf.div, conf.freq_mhz, apb);
#endif
    return true;
}

uint32_t getCpuFrequencyMhz(){
    rtc_cpu_freq_config_t conf;
    rtc_clk_cpu_freq_get_config(&conf);
    return conf.freq_mhz;
}

uint32_t getXtalFrequencyMhz(){
    return rtc_clk_xtal_freq_get();
}

uint32_t getApbFrequency(){
    rtc_cpu_freq_config_t conf;
    rtc_clk_cpu_freq_get_config(&conf);
    return calculateApb(&conf);
}
