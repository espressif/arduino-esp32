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
#include "freertos/xtensa_timer.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include "rom/rtc.h"
#include "soc/apb_ctrl_reg.h"
#include "esp32-hal.h"
#include "esp32-hal-cpu.h"

typedef struct apb_change_cb_s {
        struct apb_change_cb_s * next;
        void * arg;
        apb_change_cb_t cb;
} apb_change_t;

const uint32_t MHZ = 1000000;

static apb_change_t * apb_change_callbacks = NULL;
static xSemaphoreHandle apb_change_lock = NULL;

/*
void IRAM_ATTR twiddle( const char* pattern){
int len=strlen(pattern),pos=0;
digitalWrite(D_A,HIGH);
digitalWrite(D_B,LOW);
while(pos<len){
  if(pattern[pos++]=='1'){
    digitalWrite(D_B,HIGH);
  } else {
    digitalWrite(D_B,LOW);
  }
  digitalWrite(D_B,LOW);
}
digitalWrite(D_A,LOW);
digitalWrite(D_B,LOW);
}  
*/

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

static bool triggerApbChangeCallback(apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb){
    initApbChangeCallback();
    xSemaphoreTake(apb_change_lock, portMAX_DELAY);
    apb_change_t * r = apb_change_callbacks;
    bool success = true;
    while((r != NULL)&& success){
        success = r->cb(r->arg, ev_type, old_apb, new_apb);
        if(ev_type == APB_AFTER_CHANGE){
            r = r->next;
            success = true; // don't care after processor clock change
        } else { //APB_BEFORE_CHANGE
            if(success){
                r = r->next;
            } else { // unwind because a driver refused to accept the change
                apb_change_t * r1 = apb_change_callbacks;
                while( r1 != r ){ 
                    r1->cb(r1->arg, APB_ABORT_CHANGE, old_apb, new_apb);
                    r1 = r1->next;
                }
                break;
            }
        }
    }
    xSemaphoreGive(apb_change_lock);
    return success;
}

bool addApbChangeCallback(void * arg, apb_change_cb_t cb){
    initApbChangeCallback();
    xSemaphoreTake(apb_change_lock, portMAX_DELAY);
    apb_change_t * r = apb_change_callbacks;
    
    while(( r != NULL)&& !((r->arg == arg) && (r->cb == cb))) r=r->next;
    if( r == NULL ){ // not already in list
        r = (apb_change_t*)malloc(sizeof(apb_change_t));
        if( r == NULL ){
            log_e("Callback Object Malloc Failed");
            xSemaphoreGive(apb_change_lock);
            return false;
        }
        log_v("add callback %p(%p) at %p",cb,arg,r);
        r->arg = arg;
        r->cb = cb;
        r->next = apb_change_callbacks; // add at front
        apb_change_callbacks = r;
        
    } else {
        log_v("ReAdd callback %p(%p) at %p",cb,arg,r);
    }
    xSemaphoreGive(apb_change_lock);
    return true;
}

bool removeApbChangeCallback(void * arg, apb_change_cb_t cb){
    initApbChangeCallback();
    xSemaphoreTake(apb_change_lock, portMAX_DELAY);
    apb_change_t * r = apb_change_callbacks, *r1=NULL;
    if(r == NULL){
        xSemaphoreGive(apb_change_lock);
        return false;
    }
    bool found = false;
    bool done = false;
    while( !found && !done ){
        found = ((r->cb == cb)&&(r->arg == arg));
        if(!found) {
            r1 = r;
            r = r->next;
        }
        done = r==NULL;
    }
    if(found){
        log_v("remove callback %p",cb);
        if(r1 != NULL) {
            r1->next = r->next;
        } else {
            apb_change_callbacks = r->next;
            log_v("was head");
        }
        if(r->next == NULL) {
            log_v("was tail");
        }
        free(r);
    } else {
        xSemaphoreGive(apb_change_lock);
        return false; // did not exist
    }
    xSemaphoreGive(apb_change_lock);
    return true;
}

bool runCallbackList(){
    initApbChangeCallback();
    xSemaphoreTake(apb_change_lock, portMAX_DELAY);
    apb_change_t * r = apb_change_callbacks;
    if(r == NULL){
        xSemaphoreGive(apb_change_lock);
        return false;
    }
    log_i("APB callback List head=%p",apb_change_callbacks);
    while(r != NULL){
        log_i("[%p](cb=%p, arg=%p, next=%p)",r,r->cb,r->arg,r->next);
        r = r->next;
    }
    xSemaphoreGive(apb_change_lock);
}

static uint32_t calculateApb(rtc_cpu_freq_config_t * conf){
    if(conf->freq_mhz >= 80){
        return 80 * MHZ;
    }
    return (conf->source_freq_mhz * MHZ) / conf->div;
}

void esp_timer_impl_update_apb_freq(uint32_t apb_ticks_per_us); //private in IDF

bool setCpuFrequency(uint32_t cpu_freq_mhz){
    rtc_cpu_freq_config_t conf, cconf;
    uint32_t capb, apb;
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
    log_v("%s: %u / %u = %u Mhz, APB: %u Hz", (conf.source == RTC_CPU_FREQ_SRC_PLL)?"PLL":((conf.source == RTC_CPU_FREQ_SRC_APLL)?"APLL":((conf.source == RTC_CPU_FREQ_SRC_XTAL)?"XTAL":"8M")), conf.source_freq_mhz, conf.div, conf.freq_mhz, apb);
    //Call peripheral functions before the APB change
    if(apb_change_callbacks){
        if(!triggerApbChangeCallback(APB_BEFORE_CHANGE, capb, apb)){ // some driver refused to acceptchange;
            log_e("callback refused clockChange");
            return false;
        }
    }
    //Make the frequency change
    rtc_clk_cpu_freq_set_config_fast(&conf);
    if(capb != apb){
        // Update REF_TICK (uncomment if REF_TICK is different than 1MHz)
        // if(conf.freq_mhz < 80){
        //    ESP_REG(APB_CTRL_XTAL_TICK_CONF_REG) = conf.freq_mhz / (REF_CLK_FREQ / MHZ) - 1;
        // }
        //Update APB Freq REG
        rtc_clk_apb_freq_update(apb);
        //Update esp_timer divisor
        esp_timer_impl_update_apb_freq(apb / MHZ);
    }
    //Update FreeRTOS Tick Divisor
//    twiddle("10101");
    uint32_t fcpu = (conf.freq_mhz >= 80)?(conf.freq_mhz * MHZ):(apb);
    _xt_tick_divisor = fcpu / XT_TICK_PER_SEC;
    //Call peripheral functions after the APB change
    if(apb_change_callbacks){
        triggerApbChangeCallback(APB_AFTER_CHANGE, capb, apb);
    }
    return true;
}

uint32_t getCpuFrequencyMHz(){
    rtc_cpu_freq_config_t conf;
    rtc_clk_cpu_freq_get_config(&conf);
    return conf.freq_mhz;
}

uint32_t getApbFrequency(){
    rtc_cpu_freq_config_t conf;
    rtc_clk_cpu_freq_get_config(&conf);
    return calculateApb(&conf);
}
