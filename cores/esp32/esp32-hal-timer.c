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

#include "esp32-hal-timer.h"
#include "driver/gptimer.h"
#include "soc/soc_caps.h"
#include "clk_tree.h"

inline uint64_t timerRead(hw_timer_t timer_handle){

    uint64_t value;
    gptimer_get_raw_count(timer_handle, &value);
    return value;
}
void timerWrite(hw_timer_t timer_handle, uint64_t val){
    gptimer_set_raw_count(timer_handle, val);
}

void timerAlarmWrite(hw_timer_t timer, uint64_t alarm_value, bool autoreload, uint64_t reload_count){
    esp_err_t err = ESP_OK;
    gptimer_alarm_config_t alarm_cfg = {
        .alarm_count = alarm_value,
        .reload_count = reload_count,
        .flags.auto_reload_on_alarm = autoreload,
    };
    err = gptimer_set_alarm_action(timer, &alarm_cfg);
    if (err != ESP_OK){
        log_e("Timer Alarm Write failed, error num=%d", err);
    } 
}

uint32_t timerGetResolution(hw_timer_t timer_handle){
    uint32_t resolution;
    gptimer_get_resolution(timer_handle, &resolution);
    return resolution;
}

void timerStart(hw_timer_t timer_handle){
    gptimer_start(timer_handle);
}

void timerStop(hw_timer_t timer_handle){
    gptimer_stop(timer_handle);
}

void timerRestart(hw_timer_t timer_handle){
    gptimer_set_raw_count(timer_handle,0);
}

hw_timer_t timerBegin(uint32_t resolution, bool countUp){
    
    esp_err_t err = ESP_OK;
    hw_timer_t timer_handle;
    uint32_t counter_src_hz = 0;
    uint32_t divider = 0;
    soc_periph_gptimer_clk_src_t clk;

    soc_periph_gptimer_clk_src_t gptimer_clks[] = SOC_GPTIMER_CLKS;
    for (size_t i = 0; i < sizeof(gptimer_clks) / sizeof(gptimer_clks[0]); i++){
        clk = gptimer_clks[i];
        clk_tree_src_get_freq_hz(clk, CLK_TREE_SRC_FREQ_PRECISION_CACHED, &counter_src_hz);
        divider = counter_src_hz / resolution;
        if((divider >= 2) && (divider <= 65536)){
            break;
        }
        else divider = 0;
    }

    if(divider == 0){
        log_e("Resolution cannot be reached with any clock source, aborting!");
        return NULL;
    }

    gptimer_config_t config = {
            .clk_src = clk,
            .direction = countUp,
            .resolution_hz = resolution,
            .flags.intr_shared = true,
        };

    err = gptimer_new_timer(&config, &timer_handle);
    if (err != ESP_OK){
        log_e("Failed to create a new GPTimer, error num=%d", err);
        return NULL;
    } 
    gptimer_enable(timer_handle);
    gptimer_start(timer_handle);
    return timer_handle;
}

void timerEnd(hw_timer_t timer_handle){
    esp_err_t err = ESP_OK;
    gptimer_disable(timer_handle);
    err = gptimer_del_timer(timer_handle);
    if (err != ESP_OK){
        log_e("Failed to destroy GPTimer, error num=%d", err);
    } 
}

bool IRAM_ATTR timerFnWrapper(hw_timer_t timer, const gptimer_alarm_event_data_t *edata, void *arg){
    void (*fn)(void) = arg;
    fn();

    // some additional logic or handling may be required here to approriately yield or not
    return false;
}

void timerAttachInterrupt(hw_timer_t timer, void (*fn)(void)){
    esp_err_t err = ESP_OK;
    gptimer_event_callbacks_t cbs = {
        .on_alarm = timerFnWrapper,
    };

    gptimer_disable(timer);
    err = gptimer_register_event_callbacks(timer, &cbs, fn);
    if (err != ESP_OK){
        log_e("Timer Attach Interrupt failed, error num=%d", err);
    } 
    gptimer_enable(timer);
}

void timerDetachInterrupt(hw_timer_t timer){
    esp_err_t err = ESP_OK;
    err = gptimer_set_alarm_action(timer, NULL);
    if (err != ESP_OK){
        log_e("Timer Detach Interrupt failed, error num=%d", err);
    } 
}

uint64_t timerReadMicros(hw_timer_t timer){
    uint64_t timer_val = timerRead(timer);
    uint32_t resolution = timerGetResolution(timer);
    return timer_val * 1000000 / resolution;
}

uint64_t timerReadMilis(hw_timer_t timer){
    uint64_t timer_val = timerRead(timer);
    uint32_t resolution = timerGetResolution(timer);
    return timer_val * 1000 / resolution;
}

double timerReadSeconds(hw_timer_t timer){
    uint64_t timer_val = timerRead(timer);
    uint32_t resolution = timerGetResolution(timer);
    return (double)timer_val / resolution;
}
