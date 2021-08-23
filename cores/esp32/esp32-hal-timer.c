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
#include "freertos/FreeRTOS.h"
#ifndef CONFIG_IDF_TARGET_ESP32C3
#include "freertos/xtensa_api.h"
#include "soc/dport_reg.h"
#endif
#include "freertos/task.h"
#include "soc/timer_group_struct.h"
#include "esp_attr.h"
#include "driver/periph_ctrl.h"

#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/rom/ets_sys.h"
#include "esp_intr_alloc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/ets_sys.h"
#include "esp_intr_alloc.h"
#include "soc/periph_defs.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/ets_sys.h"
#include "esp_intr_alloc.h"
#include "soc/periph_defs.h"
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/ets_sys.h"
#include "esp_intr.h"
#endif

#define HWTIMER_LOCK()      portENTER_CRITICAL(timer->lock)
#define HWTIMER_UNLOCK()    portEXIT_CRITICAL(timer->lock)

typedef volatile struct {
    union {
        struct {
            uint32_t reserved0:   10;
            uint32_t alarm_en:     1;             /*When set  alarm is enabled*/
            uint32_t level_int_en: 1;             /*When set  level type interrupt will be generated during alarm*/
            uint32_t edge_int_en:  1;             /*When set  edge type interrupt will be generated during alarm*/
            uint32_t divider:     16;             /*Timer clock (T0/1_clk) pre-scale value.*/
            uint32_t autoreload:   1;             /*When set  timer 0/1 auto-reload at alarming is enabled*/
            uint32_t increase:     1;             /*When set  timer 0/1 time-base counter increment. When cleared timer 0 time-base counter decrement.*/
            uint32_t enable:       1;             /*When set  timer 0/1 time-base counter is enabled*/
        };
        uint32_t val;
    } config;
    uint32_t cnt_low;                             /*Register to store timer 0/1 time-base counter current value lower 32 bits.*/
    uint32_t cnt_high;                            /*Register to store timer 0 time-base counter current value higher 32 bits.*/
    uint32_t update;                              /*Write any value will trigger a timer 0 time-base counter value update (timer 0 current value will be stored in registers above)*/
    uint32_t alarm_low;                           /*Timer 0 time-base counter value lower 32 bits that will trigger the alarm*/
    uint32_t alarm_high;                          /*Timer 0 time-base counter value higher 32 bits that will trigger the alarm*/
    uint32_t load_low;                            /*Lower 32 bits of the value that will load into timer 0 time-base counter*/
    uint32_t load_high;                           /*higher 32 bits of the value that will load into timer 0 time-base counter*/
    uint32_t reload;                              /*Write any value will trigger timer 0 time-base counter reload*/
} hw_timer_reg_t;

typedef struct hw_timer_s {
        hw_timer_reg_t * dev;
        uint8_t num;
        uint8_t group;
        uint8_t timer;
        portMUX_TYPE lock;
} hw_timer_t;

static hw_timer_t hw_timer[4] = {
        {(hw_timer_reg_t *)(DR_REG_TIMERGROUP0_BASE),0,0,0,portMUX_INITIALIZER_UNLOCKED},
        {(hw_timer_reg_t *)(DR_REG_TIMERGROUP0_BASE + 0x0024),1,0,1,portMUX_INITIALIZER_UNLOCKED},
        {(hw_timer_reg_t *)(DR_REG_TIMERGROUP0_BASE + 0x1000),2,1,0,portMUX_INITIALIZER_UNLOCKED},
        {(hw_timer_reg_t *)(DR_REG_TIMERGROUP0_BASE + 0x1024),3,1,1,portMUX_INITIALIZER_UNLOCKED}
};

typedef void (*voidFuncPtr)(void);
static voidFuncPtr __timerInterruptHandlers[4] = {0,0,0,0};

void ARDUINO_ISR_ATTR __timerISR(void * arg){
#if CONFIG_IDF_TARGET_ESP32
    uint32_t s0 = TIMERG0.int_st_timers.val;
    uint32_t s1 = TIMERG1.int_st_timers.val;
    TIMERG0.int_clr_timers.val = s0;
    TIMERG1.int_clr_timers.val = s1;
#else
    uint32_t s0 = TIMERG0.int_st.val;
    uint32_t s1 = TIMERG1.int_st.val;
    TIMERG0.int_clr.val = s0;
    TIMERG1.int_clr.val = s1;
#endif
    uint8_t status = (s1 & 3) << 2 | (s0 & 3);
    uint8_t i = 4;
    //restart the timers that should autoreload
    while(i--){
        hw_timer_reg_t * dev = hw_timer[i].dev;
        if((status & (1 << i)) && dev->config.autoreload){
            dev->config.alarm_en = 1;
        }
    }
    i = 4;
    //call callbacks
    while(i--){
        if(__timerInterruptHandlers[i] && (status & (1 << i))){
            __timerInterruptHandlers[i]();
        }
    }
}

uint64_t inline timerRead(hw_timer_t *timer){
    timer->dev->update = 1;
    while (timer->dev->update) {};
    uint64_t h = timer->dev->cnt_high;
    uint64_t l = timer->dev->cnt_low;
    return (h << 32) | l;
}

uint64_t timerAlarmRead(hw_timer_t *timer){
    uint64_t h = timer->dev->alarm_high;
    uint64_t l = timer->dev->alarm_low;
    return (h << 32) | l;
}

void timerWrite(hw_timer_t *timer, uint64_t val){
    timer->dev->load_high = (uint32_t) (val >> 32);
    timer->dev->load_low = (uint32_t) (val);
    timer->dev->reload = 1;
}

void timerAlarmWrite(hw_timer_t *timer, uint64_t alarm_value, bool autoreload){
    timer->dev->alarm_high = (uint32_t) (alarm_value >> 32);
    timer->dev->alarm_low = (uint32_t) alarm_value;
    timer->dev->config.autoreload = autoreload;
}

void timerSetConfig(hw_timer_t *timer, uint32_t config){
    timer->dev->config.val = config;
}

uint32_t timerGetConfig(hw_timer_t *timer){
    return timer->dev->config.val;
}

void timerSetCountUp(hw_timer_t *timer, bool countUp){
    timer->dev->config.increase = countUp;
}

bool timerGetCountUp(hw_timer_t *timer){
    return timer->dev->config.increase;
}

void timerSetAutoReload(hw_timer_t *timer, bool autoreload){
    timer->dev->config.autoreload = autoreload;
}

bool timerGetAutoReload(hw_timer_t *timer){
    return timer->dev->config.autoreload;
}

void timerSetDivider(hw_timer_t *timer, uint16_t divider){//2 to 65536
    if(!divider){
        divider = 0xFFFF;
    } else if(divider == 1){
        divider = 2;
    }
    int timer_en = timer->dev->config.enable;
    timer->dev->config.enable = 0;
    timer->dev->config.divider = divider;
    timer->dev->config.enable = timer_en;
}

uint16_t timerGetDivider(hw_timer_t *timer){
    return timer->dev->config.divider;
}

void timerStart(hw_timer_t *timer){
    timer->dev->config.enable = 1;
}

void timerStop(hw_timer_t *timer){
    timer->dev->config.enable = 0;
}

void timerRestart(hw_timer_t *timer){
    timer->dev->config.enable = 0;
    timer->dev->reload = 1;
    timer->dev->config.enable = 1;
}

bool timerStarted(hw_timer_t *timer){
    return timer->dev->config.enable;
}

void timerAlarmEnable(hw_timer_t *timer){
    timer->dev->config.alarm_en = 1;
}

void timerAlarmDisable(hw_timer_t *timer){
    timer->dev->config.alarm_en = 0;
}

bool timerAlarmEnabled(hw_timer_t *timer){
    return timer->dev->config.alarm_en;
}

static void _on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb){
    hw_timer_t * timer = (hw_timer_t *)arg;
    if(ev_type == APB_BEFORE_CHANGE){
        timer->dev->config.enable = 0;
    } else {
        old_apb /= 1000000;
        new_apb /= 1000000;
        timer->dev->config.divider = (new_apb * timer->dev->config.divider) / old_apb;
        timer->dev->config.enable = 1;
    }
}

hw_timer_t * timerBegin(uint8_t num, uint16_t divider, bool countUp){
    if(num > 3){
        return NULL;
    }
    hw_timer_t * timer = &hw_timer[num];
    if(timer->group) {
    	periph_module_enable(PERIPH_TIMG1_MODULE);
    } else {
    	periph_module_enable(PERIPH_TIMG0_MODULE);
    }
    timer->dev->config.enable = 0;
    if(timer->group) {
        TIMERG1.int_ena.val &= ~BIT(timer->timer);
#if CONFIG_IDF_TARGET_ESP32
            TIMERG1.int_clr_timers.val |= BIT(timer->timer);
#else
            TIMERG1.int_clr.val = BIT(timer->timer);
#endif
    } else {
        TIMERG0.int_ena.val &= ~BIT(timer->timer);
#if CONFIG_IDF_TARGET_ESP32
            TIMERG0.int_clr_timers.val |= BIT(timer->timer);
#else
            TIMERG0.int_clr.val = BIT(timer->timer);
#endif
    }
#ifdef TIMER_GROUP_SUPPORTS_XTAL_CLOCK
    timer->dev->config.use_xtal = 0;
#endif
    timerSetDivider(timer, divider);
    timerSetCountUp(timer, countUp);
    timerSetAutoReload(timer, false);
    timerAttachInterrupt(timer, NULL, false);
    timerWrite(timer, 0);
    timer->dev->config.enable = 1;
    addApbChangeCallback(timer, _on_apb_change);
    return timer;
}

void timerEnd(hw_timer_t *timer){
    timer->dev->config.enable = 0;
    timerAttachInterrupt(timer, NULL, false);
    removeApbChangeCallback(timer, _on_apb_change);
}

void timerAttachInterrupt(hw_timer_t *timer, void (*fn)(void), bool edge){
#if CONFIG_IDF_TARGET_ESP32
    if(edge){
        log_w("EDGE timer interrupt does not work properly on ESP32! Setting to LEVEL...");
        edge = false;
    }
#endif
    static bool initialized = false;
    static intr_handle_t intr_handle = NULL;
    if(intr_handle){
        esp_intr_disable(intr_handle);
    }
    if(fn == NULL){
        timer->dev->config.level_int_en = 0;
        timer->dev->config.edge_int_en = 0;
        timer->dev->config.alarm_en = 0;
        if(timer->num & 2){
            TIMERG1.int_ena.val &= ~BIT(timer->timer);
#if CONFIG_IDF_TARGET_ESP32
            TIMERG1.int_clr_timers.val |= BIT(timer->timer);
#else
            TIMERG1.int_clr.val = BIT(timer->timer);
#endif
        } else {
            TIMERG0.int_ena.val &= ~BIT(timer->timer);
#if CONFIG_IDF_TARGET_ESP32
            TIMERG0.int_clr_timers.val |= BIT(timer->timer);
#else
            TIMERG0.int_clr.val = BIT(timer->timer);
#endif
        }
        __timerInterruptHandlers[timer->num] = NULL;
    } else {
        __timerInterruptHandlers[timer->num] = fn;
        timer->dev->config.level_int_en = edge?0:1;//When set, an alarm will generate a level type interrupt.
        timer->dev->config.edge_int_en = edge?1:0;//When set, an alarm will generate an edge type interrupt.
        int intr_source = 0;
#ifndef CONFIG_IDF_TARGET_ESP32C3
        if(!edge){
#endif
            if(timer->group){
                intr_source = ETS_TG1_T0_LEVEL_INTR_SOURCE + timer->timer;
            } else {
                intr_source = ETS_TG0_T0_LEVEL_INTR_SOURCE + timer->timer;
            }
#ifndef CONFIG_IDF_TARGET_ESP32C3
        } else {
            if(timer->group){
                intr_source = ETS_TG1_T0_EDGE_INTR_SOURCE + timer->timer;
            } else {
                intr_source = ETS_TG0_T0_EDGE_INTR_SOURCE + timer->timer;
            }
        }
#endif
        if(!initialized){
            initialized = true;
            esp_intr_alloc(intr_source, (int)(ARDUINO_ISR_FLAG|ESP_INTR_FLAG_LOWMED), __timerISR, NULL, &intr_handle);
        } else {
            intr_matrix_set(esp_intr_get_cpu(intr_handle), intr_source, esp_intr_get_intno(intr_handle));
        }
        if(timer->group){
            TIMERG1.int_ena.val |= BIT(timer->timer);
        } else {
            TIMERG0.int_ena.val |= BIT(timer->timer);
        }
    }
    if(intr_handle){
        esp_intr_enable(intr_handle);
    }
}

void timerDetachInterrupt(hw_timer_t *timer){
    timerAttachInterrupt(timer, NULL, false);
}

uint64_t timerReadMicros(hw_timer_t *timer){
    uint64_t timer_val = timerRead(timer);
    uint16_t div = timerGetDivider(timer);
    return timer_val * div / (getApbFrequency() / 1000000);
}

double timerReadSeconds(hw_timer_t *timer){
    uint64_t timer_val = timerRead(timer);
    uint16_t div = timerGetDivider(timer);
    return (double)timer_val * div / getApbFrequency();
}

uint64_t timerAlarmReadMicros(hw_timer_t *timer){
    uint64_t timer_val = timerAlarmRead(timer);
    uint16_t div = timerGetDivider(timer);
    return timer_val * div / (getApbFrequency() / 1000000);
}

double timerAlarmReadSeconds(hw_timer_t *timer){
    uint64_t timer_val = timerAlarmRead(timer);
    uint16_t div = timerGetDivider(timer);
    return (double)timer_val * div / getApbFrequency();
}
