// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
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
#include "freertos/xtensa_api.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "soc/timer_group_struct.h"
#include "soc/dport_reg.h"
#include "esp_attr.h"
#include "esp_intr.h"

#define HWTIMER_LOCK()      portENTER_CRITICAL(timer->lock)
#define HWTIMER_UNLOCK()    portEXIT_CRITICAL(timer->lock)

typedef struct {
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

void IRAM_ATTR __timerISR(void * arg){
    uint32_t s0 = TIMERG0.int_st_timers.val;
    uint32_t s1 = TIMERG1.int_st_timers.val;
    TIMERG0.int_clr_timers.val = s0;
    TIMERG1.int_clr_timers.val = s1;
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

uint64_t IRAM_ATTR timerRead(hw_timer_t *timer){
    timer->dev->update = 1;
    uint64_t h = timer->dev->cnt_high;
    uint64_t l = timer->dev->cnt_low;
    return (h << 32) | l;
}

uint64_t IRAM_ATTR timerAlarmRead(hw_timer_t *timer){
    uint64_t h = timer->dev->alarm_high;
    uint64_t l = timer->dev->alarm_low;
    return (h << 32) | l;
}

void IRAM_ATTR timerWrite(hw_timer_t *timer, uint64_t val){
    timer->dev->load_high = (uint32_t) (val >> 32);
    timer->dev->load_low = (uint32_t) (val);
    timer->dev->reload = 1;
}

void IRAM_ATTR timerAlarmWrite(hw_timer_t *timer, uint64_t alarm_value, bool autoreload){
    timer->dev->alarm_high = (uint32_t) (alarm_value >> 32);
    timer->dev->alarm_low = (uint32_t) alarm_value;
    timer->dev->config.autoreload = autoreload;
}

void IRAM_ATTR timerSetConfig(hw_timer_t *timer, uint32_t config){
    timer->dev->config.val = config;
}

uint32_t IRAM_ATTR timerGetConfig(hw_timer_t *timer){
    return timer->dev->config.val;
}

void IRAM_ATTR timerSetCountUp(hw_timer_t *timer, bool countUp){
    timer->dev->config.increase = countUp;
}

bool IRAM_ATTR timerGetCountUp(hw_timer_t *timer){
    return timer->dev->config.increase;
}

void IRAM_ATTR timerSetAutoReload(hw_timer_t *timer, bool autoreload){
    timer->dev->config.autoreload = autoreload;
}

bool IRAM_ATTR timerGetAutoReload(hw_timer_t *timer){
    return timer->dev->config.autoreload;
}

void IRAM_ATTR timerSetDivider(hw_timer_t *timer, uint16_t divider){//2 to 65536
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

uint16_t IRAM_ATTR timerGetDivider(hw_timer_t *timer){
    return timer->dev->config.divider;
}

void IRAM_ATTR timerStart(hw_timer_t *timer){
    timer->dev->config.enable = 1;
}

void IRAM_ATTR timerStop(hw_timer_t *timer){
    timer->dev->config.enable = 0;
}

void IRAM_ATTR timerRestart(hw_timer_t *timer){
    timer->dev->config.enable = 0;
    timer->dev->reload = 1;
    timer->dev->config.enable = 1;
}

bool IRAM_ATTR timerStarted(hw_timer_t *timer){
    return timer->dev->config.enable;
}

void IRAM_ATTR timerAlarmEnable(hw_timer_t *timer){
    timer->dev->config.alarm_en = 1;
}

void IRAM_ATTR timerAlarmDisable(hw_timer_t *timer){
    timer->dev->config.alarm_en = 0;
}

bool IRAM_ATTR timerAlarmEnabled(hw_timer_t *timer){
    return timer->dev->config.alarm_en;
}

static void IRAM_ATTR _on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb){
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

hw_timer_t * IRAM_ATTR timerBegin(uint8_t num, uint16_t divider, bool countUp){
    if(num > 3){
        return NULL;
    }
    hw_timer_t * timer = &hw_timer[num];
    if(timer->group) {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_TIMERGROUP1_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_TIMERGROUP1_RST);
        TIMERG1.int_ena.val &= ~BIT(timer->timer);
    } else {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_TIMERGROUP_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_TIMERGROUP_RST);
        TIMERG0.int_ena.val &= ~BIT(timer->timer);
    }
    timer->dev->config.enable = 0;
    timerSetDivider(timer, divider);
    timerSetCountUp(timer, countUp);
    timerSetAutoReload(timer, false);
    timerAttachInterrupt(timer, NULL, false);
    timerWrite(timer, 0);
    timer->dev->config.enable = 1;
    addApbChangeCallback(timer, _on_apb_change);
    return timer;
}

void IRAM_ATTR timerEnd(hw_timer_t *timer){
    timer->dev->config.enable = 0;
    timerAttachInterrupt(timer, NULL, false);
    removeApbChangeCallback(timer, _on_apb_change);
}

void IRAM_ATTR timerAttachInterrupt(hw_timer_t *timer, void (*fn)(void), bool edge){
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
        } else {
            TIMERG0.int_ena.val &= ~BIT(timer->timer);
        }
        __timerInterruptHandlers[timer->num] = NULL;
    } else {
        __timerInterruptHandlers[timer->num] = fn;
        timer->dev->config.level_int_en = edge?0:1;//When set, an alarm will generate a level type interrupt.
        timer->dev->config.edge_int_en = edge?1:0;//When set, an alarm will generate an edge type interrupt.
        int intr_source = 0;
        if(!edge){
            if(timer->group){
                intr_source = ETS_TG1_T0_LEVEL_INTR_SOURCE + timer->timer;
            } else {
                intr_source = ETS_TG0_T0_LEVEL_INTR_SOURCE + timer->timer;
            }
        } else {
            if(timer->group){
                intr_source = ETS_TG1_T0_EDGE_INTR_SOURCE + timer->timer;
            } else {
                intr_source = ETS_TG0_T0_EDGE_INTR_SOURCE + timer->timer;
            }
        }
        if(!initialized){
            initialized = true;
            esp_intr_alloc(intr_source, (int)(ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_LOWMED|ESP_INTR_FLAG_EDGE), __timerISR, NULL, &intr_handle);
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

void IRAM_ATTR timerDetachInterrupt(hw_timer_t *timer){
    timerAttachInterrupt(timer, NULL, false);
}

uint64_t IRAM_ATTR timerReadMicros(hw_timer_t *timer){
    uint64_t timer_val = timerRead(timer);
    uint16_t div = timerGetDivider(timer);
    return timer_val * div / (getApbFrequency() / 1000000);
}

double IRAM_ATTR timerReadSeconds(hw_timer_t *timer){
    uint64_t timer_val = timerRead(timer);
    uint16_t div = timerGetDivider(timer);
    return (double)timer_val * div / getApbFrequency();
}

uint64_t IRAM_ATTR timerAlarmReadMicros(hw_timer_t *timer){
    uint64_t timer_val = timerAlarmRead(timer);
    uint16_t div = timerGetDivider(timer);
    return timer_val * div / (getApbFrequency() / 1000000);
}

double IRAM_ATTR timerAlarmReadSeconds(hw_timer_t *timer){
    uint64_t timer_val = timerAlarmRead(timer);
    uint16_t div = timerGetDivider(timer);
    return (double)timer_val * div / getApbFrequency();
}
