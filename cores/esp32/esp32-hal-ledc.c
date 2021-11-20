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

#include "esp32-hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp32-hal-matrix.h"
#include "soc/ledc_reg.h"
#include "soc/ledc_struct.h"
#include "driver/periph_ctrl.h"

#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "soc/dport_reg.h"
#include "esp32/rom/ets_sys.h"
#define LAST_CHAN (15)
#elif CONFIG_IDF_TARGET_ESP32S2
#include "soc/dport_reg.h"
#include "esp32s2/rom/ets_sys.h"
#define LAST_CHAN (7)
#define LEDC_DIV_NUM_HSTIMER0_V LEDC_CLK_DIV_LSTIMER0_V
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/ets_sys.h"
#define LAST_CHAN (7)
#define LEDC_DIV_NUM_HSTIMER0_V LEDC_CLK_DIV_LSTIMER0_V
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/ets_sys.h"
#endif

#if CONFIG_DISABLE_HAL_LOCKS
#define LEDC_MUTEX_LOCK()
#define LEDC_MUTEX_UNLOCK()
#else
#define LEDC_MUTEX_LOCK()    do {} while (xSemaphoreTake(_ledc_sys_lock, portMAX_DELAY) != pdPASS)
#define LEDC_MUTEX_UNLOCK()  xSemaphoreGive(_ledc_sys_lock)
xSemaphoreHandle _ledc_sys_lock = NULL;
#endif

/*
 * LEDC Chan to Group/Channel/Timer Mapping
** ledc: 0  => Group: 0, Channel: 0, Timer: 0
** ledc: 1  => Group: 0, Channel: 1, Timer: 0
** ledc: 2  => Group: 0, Channel: 2, Timer: 1
** ledc: 3  => Group: 0, Channel: 3, Timer: 1
** ledc: 4  => Group: 0, Channel: 4, Timer: 2
** ledc: 5  => Group: 0, Channel: 5, Timer: 2
** ledc: 6  => Group: 0, Channel: 6, Timer: 3
** ledc: 7  => Group: 0, Channel: 7, Timer: 3
** ledc: 8  => Group: 1, Channel: 0, Timer: 0
** ledc: 9  => Group: 1, Channel: 1, Timer: 0
** ledc: 10 => Group: 1, Channel: 2, Timer: 1
** ledc: 11 => Group: 1, Channel: 3, Timer: 1
** ledc: 12 => Group: 1, Channel: 4, Timer: 2
** ledc: 13 => Group: 1, Channel: 5, Timer: 2
** ledc: 14 => Group: 1, Channel: 6, Timer: 3
** ledc: 15 => Group: 1, Channel: 7, Timer: 3
*/
#define LEDC_CHAN(g,c) LEDC.channel_group[(g)].channel[(c)]
#define LEDC_TIMER(g,t) LEDC.timer_group[(g)].timer[(t)]

static void _on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb){
    if(ev_type == APB_AFTER_CHANGE && old_apb != new_apb){
        uint16_t iarg = *(uint16_t*)arg;
        uint8_t chan = 0;
        old_apb /= 1000000;
        new_apb /= 1000000;
        while(iarg){ // run though all active channels, adjusting timing configurations
            if(iarg & 1) {// this channel is active
                uint8_t group=(chan/8), timer=((chan/2)%4);
                if(LEDC_TIMER(group, timer).conf.tick_sel){
                    LEDC_MUTEX_LOCK();
                    uint32_t old_div = LEDC_TIMER(group, timer).conf.clock_divider;
                    uint32_t div_num = (new_apb * old_div) / old_apb;
                    if(div_num > LEDC_DIV_NUM_HSTIMER0_V){
                        div_num = ((REF_CLK_FREQ /1000000) * old_div) / old_apb;
                        if(div_num > LEDC_DIV_NUM_HSTIMER0_V) {
                            div_num = LEDC_DIV_NUM_HSTIMER0_V;//lowest clock possible
                        }
                        LEDC_TIMER(group, timer).conf.tick_sel = 0;
                    } else if(div_num < 256) {
                        div_num = 256;//highest clock possible
                    }
                    LEDC_TIMER(group, timer).conf.clock_divider = div_num;
                    LEDC_MUTEX_UNLOCK();
                }
                else {
                    log_d("using REF_CLK chan=%d",chan);
                }
            }
            iarg = iarg >> 1;
            chan++;
        }
    }
}

//uint32_t frequency = (80MHz or 1MHz)/((div_num / 256.0)*(1 << bit_num));
static void _ledcSetupTimer(uint8_t chan, uint32_t div_num, uint8_t bit_num, bool apb_clk)
{
    uint8_t group=(chan/8), timer=((chan/2)%4);
    static bool tHasStarted = false;
    static uint16_t _activeChannels = 0;
#if CONFIG_IDF_TARGET_ESP32S2
// ESP32-S2 TRM v1.0 on Page 789 -> BIT LEDC_TICK_SEL_TIMERx is 0 for LEDC_PWM_CLK and 1 for REF_TICK
    apb_clk = 0;        
#endif
    if(!tHasStarted) {
        tHasStarted = true;
        periph_module_enable(PERIPH_LEDC_MODULE);
        LEDC.conf.apb_clk_sel = 1;//LS use apb clock
        addApbChangeCallback((void*)&_activeChannels, _on_apb_change);

#if !CONFIG_DISABLE_HAL_LOCKS
        _ledc_sys_lock = xSemaphoreCreateMutex();
#endif
    }
    LEDC_MUTEX_LOCK();
    LEDC_TIMER(group, timer).conf.clock_divider = div_num;//18 bit (10.8) This register is used to configure parameter for divider in timer the least significant eight bits represent the decimal part.
    LEDC_TIMER(group, timer).conf.duty_resolution = bit_num;//5 bit This register controls the range of the counter in timer. the counter range is [0 2**bit_num] the max bit width for counter is 20.
    LEDC_TIMER(group, timer).conf.tick_sel = apb_clk;//apb clock
#if CONFIG_IDF_TARGET_ESP32
    if(group) {
#endif
        LEDC_TIMER(group, timer).conf.low_speed_update = 1;//This bit is only useful for low speed timer channels, reserved for high speed timers
#if CONFIG_IDF_TARGET_ESP32
    }
#endif
    LEDC_TIMER(group, timer).conf.pause = 0;
    LEDC_TIMER(group, timer).conf.rst = 1;//This bit is used to reset timer the counter will be 0 after reset.
    LEDC_TIMER(group, timer).conf.rst = 0;
    LEDC_MUTEX_UNLOCK();
    _activeChannels |= (1 << chan); // mark as active for APB callback
}

//max div_num 0x3FFFF (262143)
//max bit_num 0x1F (31)
static double _ledcSetupTimerFreq(uint8_t chan, double freq, uint8_t bit_num)
{
    uint64_t clk_freq = getApbFrequency();
    clk_freq <<= 8;//div_num is 8 bit decimal
    uint32_t div_num = (clk_freq >> bit_num) / freq;
    bool apb_clk = true;
    if(div_num > LEDC_DIV_NUM_HSTIMER0_V) {
        clk_freq /= 80;
        div_num = (clk_freq >> bit_num) / freq;
        if(div_num > LEDC_DIV_NUM_HSTIMER0_V) {
            div_num = LEDC_DIV_NUM_HSTIMER0_V;//lowest clock possible
        }
        apb_clk = false;
    } else if(div_num < 256) {
        div_num = 256;//highest clock possible
    }
    _ledcSetupTimer(chan, div_num, bit_num, apb_clk);
    //log_i("Fin: %f, Fclk: %uMhz, bits: %u, DIV: %u, Fout: %f",
    //        freq, apb_clk?80:1, bit_num, div_num, (clk_freq >> bit_num) / (double)div_num);
    return (clk_freq >> bit_num) / (double)div_num;
}

static double _ledcTimerRead(uint8_t chan)
{
    uint32_t div_num;
    uint8_t bit_num;
    bool apb_clk;
    uint8_t group=(chan/8), timer=((chan/2)%4);
    LEDC_MUTEX_LOCK();
    div_num = LEDC_TIMER(group, timer).conf.clock_divider;//18 bit (10.8) This register is used to configure parameter for divider in timer the least significant eight bits represent the decimal part.
    bit_num = LEDC_TIMER(group, timer).conf.duty_resolution;//5 bit This register controls the range of the counter in timer. the counter range is [0 2**bit_num] the max bit width for counter is 20.
    apb_clk = LEDC_TIMER(group, timer).conf.tick_sel;//apb clock
    LEDC_MUTEX_UNLOCK();
    uint64_t clk_freq = 1000000;
    if(apb_clk) {
        clk_freq = getApbFrequency();
    }
    clk_freq <<= 8;//div_num is 8 bit decimal
    return (clk_freq >> bit_num) / (double)div_num;
}

static void _ledcSetupChannel(uint8_t chan, uint8_t idle_level)
{
    uint8_t group=(chan/8), channel=(chan%8), timer=((chan/2)%4);
    LEDC_MUTEX_LOCK();
    LEDC_CHAN(group, channel).conf0.timer_sel = timer;//2 bit Selects the timer to attach 0-3
    LEDC_CHAN(group, channel).conf0.idle_lv = idle_level;//1 bit This bit is used to control the output value when channel is off.
    LEDC_CHAN(group, channel).hpoint.hpoint = 0;//20 bit The output value changes to high when timer selected by channel has reached hpoint
    LEDC_CHAN(group, channel).conf1.duty_inc = 1;//1 bit This register is used to increase the duty of output signal or decrease the duty of output signal for high speed channel
    LEDC_CHAN(group, channel).conf1.duty_num = 1;//10 bit This register is used to control the number of increased or decreased times for channel
    LEDC_CHAN(group, channel).conf1.duty_cycle = 1;//10 bit This register is used to increase or decrease the duty every duty_cycle cycles for channel
    LEDC_CHAN(group, channel).conf1.duty_scale = 0;//10 bit This register controls the increase or decrease step scale for channel.
    LEDC_CHAN(group, channel).duty.duty = 0;
    LEDC_CHAN(group, channel).conf0.sig_out_en = 0;//This is the output enable control bit for channel
    LEDC_CHAN(group, channel).conf1.duty_start = 0;//When duty_num duty_cycle and duty_scale has been configured. these register won't take effect until set duty_start. this bit is automatically cleared by hardware.
#if CONFIG_IDF_TARGET_ESP32
    if(group) {
#endif
        LEDC_CHAN(group, channel).conf0.low_speed_update = 1;
#if CONFIG_IDF_TARGET_ESP32
    } else {
        LEDC_CHAN(group, channel).conf0.clk_en = 0;
    }
#endif
    LEDC_MUTEX_UNLOCK();
}

double ledcSetup(uint8_t chan, double freq, uint8_t bit_num)
{
    if(chan > LAST_CHAN) {
        return 0;
    }
    double res_freq = _ledcSetupTimerFreq(chan, freq, bit_num);
    _ledcSetupChannel(chan, LOW);
    return res_freq;
}

void ledcWrite(uint8_t chan, uint32_t duty)
{
    if(chan > LAST_CHAN) {
        return;
    }
    uint8_t group=(chan/8), channel=(chan%8);
    LEDC_MUTEX_LOCK();
    LEDC_CHAN(group, channel).duty.duty = duty << 4;//25 bit (21.4)
    if(duty) {
        LEDC_CHAN(group, channel).conf0.sig_out_en = 1;//This is the output enable control bit for channel
        LEDC_CHAN(group, channel).conf1.duty_start = 1;//When duty_num duty_cycle and duty_scale has been configured. these register won't take effect until set duty_start. this bit is automatically cleared by hardware.
#if CONFIG_IDF_TARGET_ESP32
        if(group) {
#endif
            LEDC_CHAN(group, channel).conf0.low_speed_update = 1;
#if CONFIG_IDF_TARGET_ESP32
        } else {
            LEDC_CHAN(group, channel).conf0.clk_en = 1;
        }
#endif
    } else {
        LEDC_CHAN(group, channel).conf0.sig_out_en = 0;//This is the output enable control bit for channel
        LEDC_CHAN(group, channel).conf1.duty_start = 0;//When duty_num duty_cycle and duty_scale has been configured. these register won't take effect until set duty_start. this bit is automatically cleared by hardware.
#if CONFIG_IDF_TARGET_ESP32
        if(group) {
#endif
            LEDC_CHAN(group, channel).conf0.low_speed_update = 1;
#if CONFIG_IDF_TARGET_ESP32
        } else {
            LEDC_CHAN(group, channel).conf0.clk_en = 0;
        }
#endif
    }
    LEDC_MUTEX_UNLOCK();
}

uint32_t ledcRead(uint8_t chan)
{
    if(chan > LAST_CHAN) {
        return 0;
    }
    return LEDC.channel_group[chan/8].channel[chan%8].duty.duty >> 4;
}

double ledcReadFreq(uint8_t chan)
{
    if(!ledcRead(chan)){
        return 0;
    }
    return _ledcTimerRead(chan);
}

double ledcWriteTone(uint8_t chan, double freq)
{
    if(chan > LAST_CHAN) {
        return 0;
    }
    if(!freq) {
        ledcWrite(chan, 0);
        return 0;
    }
    double res_freq = _ledcSetupTimerFreq(chan, freq, 10);
    ledcWrite(chan, 0x1FF);
    return res_freq;
}

double ledcWriteNote(uint8_t chan, note_t note, uint8_t octave){
    const uint16_t noteFrequencyBase[12] = {
    //   C        C#       D        Eb       E        F       F#        G       G#        A       Bb        B
        4186,    4435,    4699,    4978,    5274,    5588,    5920,    6272,    6645,    7040,    7459,    7902
    };

    if(octave > 8 || note >= NOTE_MAX){
        return 0;
    }
    double noteFreq =  (double)noteFrequencyBase[note] / (double)(1 << (8-octave));
    return ledcWriteTone(chan, noteFreq);
}

void ledcAttachPin(uint8_t pin, uint8_t chan)
{
    if(chan > LAST_CHAN) {
        return;
    }
    pinMode(pin, OUTPUT);
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32C3
    pinMatrixOutAttach(pin, LEDC_LS_SIG_OUT0_IDX + chan, false, false);
#else
    pinMatrixOutAttach(pin, ((chan/8)?LEDC_LS_SIG_OUT0_IDX:LEDC_HS_SIG_OUT0_IDX) + (chan%8), false, false);
#endif
}

void ledcDetachPin(uint8_t pin)
{
    pinMatrixOutDetach(pin, false, false);
}

double ledcChangeFrequency(uint8_t chan, double freq, uint8_t bit_num)
{
    if (chan > 15) {
        return 0;
    }
    double res_freq = _ledcSetupTimerFreq(chan, freq, bit_num);
    return res_freq;
}
