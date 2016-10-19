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
#include "rom/ets_sys.h"
#include "esp32-hal-matrix.h"
#include "soc/dport_reg.h"
#include "soc/ledc_reg.h"
#include "soc/ledc_struct.h"

xSemaphoreHandle _ledc_sys_lock;


#define LEDC_MUTEX_LOCK()    do {} while (xSemaphoreTake(_ledc_sys_lock, portMAX_DELAY) != pdPASS)
#define LEDC_MUTEX_UNLOCK()  xSemaphoreGive(_ledc_sys_lock)

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

//uint32_t frequency = (80MHz or 1MHz)/((div_num / 256.0)*(1 << bit_num));
void ledcSetupTimer(uint8_t chan, uint32_t div_num, uint8_t bit_num, bool apb_clk)
{
    ledc_dev_t * ledc_dev = (volatile ledc_dev_t *)(DR_REG_LEDC_BASE);
    uint8_t group=(chan/8), timer=((chan/2)%4);
    static bool tHasStarted = false;
    if(!tHasStarted) {
        tHasStarted = true;
        SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_LEDC_CLK_EN);
        CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_LEDC_RST);
        ledc_dev->conf.apb_clk_sel = 1;//LS use apb clock
        _ledc_sys_lock = xSemaphoreCreateMutex();
    }
    LEDC_MUTEX_LOCK();
    ledc_dev->timer_group[group].timer[timer].conf.div_num = div_num;//18 bit (10.8) This register is used to configure parameter for divider in timer the least significant eight bits represent the decimal part.
    ledc_dev->timer_group[group].timer[timer].conf.bit_num = bit_num;//5 bit This register controls the range of the counter in timer. the counter range is [0 2**bit_num] the max bit width for counter is 20.
    ledc_dev->timer_group[group].timer[timer].conf.tick_sel = apb_clk;//apb clock
    if(group) {
        ledc_dev->timer_group[group].timer[timer].conf.low_speed_update = 1;//This bit is only useful for low speed timer channels, reserved for high speed timers
    }
    ledc_dev->timer_group[group].timer[timer].conf.pause = 0;
    ledc_dev->timer_group[group].timer[timer].conf.rst = 1;//This bit is used to reset timer the counter will be 0 after reset.
    ledc_dev->timer_group[group].timer[timer].conf.rst = 0;
    LEDC_MUTEX_UNLOCK();
}

uint32_t ledcSetupTimerFreq(uint8_t chan, uint32_t freq, uint8_t bit_num)
{
    uint64_t clk_freq = APB_CLK_FREQ;
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
    ledcSetupTimer(chan, div_num, bit_num, apb_clk);
    return (clk_freq >> bit_num) / div_num;
}

void ledcSetupChannel(uint8_t chan, uint8_t idle_level)
{
    uint8_t group=(chan/8), channel=(chan%8), timer=((chan/2)%4);
    ledc_dev_t * ledc_dev = (volatile ledc_dev_t *)(DR_REG_LEDC_BASE);
    LEDC_MUTEX_LOCK();
    ledc_dev->channel_group[group].channel[channel].conf0.timer_sel = timer;//2 bit Selects the timer to attach 0-3
    ledc_dev->channel_group[group].channel[channel].conf0.idle_lv = idle_level;//1 bit This bit is used to control the output value when channel is off.
    ledc_dev->channel_group[group].channel[channel].hpoint.hpoint = 0;//20 bit The output value changes to high when timer selected by channel has reached hpoint
    ledc_dev->channel_group[group].channel[channel].conf1.duty_inc = 1;//1 bit This register is used to increase the duty of output signal or decrease the duty of output signal for high speed channel
    ledc_dev->channel_group[group].channel[channel].conf1.duty_num = 1;//10 bit This register is used to control the number of increased or decreased times for channel
    ledc_dev->channel_group[group].channel[channel].conf1.duty_cycle = 1;//10 bit This register is used to increase or decrease the duty every duty_cycle cycles for channel
    ledc_dev->channel_group[group].channel[channel].conf1.duty_scale = 0;//10 bit This register controls the increase or decrease step scale for channel.
    ledc_dev->channel_group[group].channel[channel].duty.duty = 0;
    ledc_dev->channel_group[group].channel[channel].conf0.sig_out_en = 0;//This is the output enable control bit for channel
    ledc_dev->channel_group[group].channel[channel].conf1.duty_start = 0;//When duty_num duty_cycle and duty_scale has been configured. these register won't take effect until set duty_start. this bit is automatically cleared by hardware.
    if(group) {
        ledc_dev->channel_group[group].channel[channel].conf0.val &= ~BIT(4);
    } else {
        ledc_dev->channel_group[group].channel[channel].conf0.clk_en = 0;
    }
    LEDC_MUTEX_UNLOCK();
}

uint32_t ledcSetup(uint8_t chan, uint32_t freq, uint8_t bit_num)
{
    if(chan > 15) {
        return 0;
    }
    uint32_t res_freq = ledcSetupTimerFreq(chan, freq, bit_num);
    ledcSetupChannel(chan, LOW);
    return res_freq;
}

void ledcWrite(uint8_t chan, uint32_t duty)
{
    if(chan > 15) {
        return;
    }
    uint8_t group=(chan/8), channel=(chan%8);
    ledc_dev_t * ledc_dev = (volatile ledc_dev_t *)(DR_REG_LEDC_BASE);
    LEDC_MUTEX_LOCK();
    ledc_dev->channel_group[group].channel[channel].duty.duty = duty << 4;//25 bit (21.4)
    if(duty) {
        ledc_dev->channel_group[group].channel[channel].conf0.sig_out_en = 1;//This is the output enable control bit for channel
        ledc_dev->channel_group[group].channel[channel].conf1.duty_start = 1;//When duty_num duty_cycle and duty_scale has been configured. these register won't take effect until set duty_start. this bit is automatically cleared by hardware.
        if(group) {
            ledc_dev->channel_group[group].channel[channel].conf0.val |= BIT(4);
        } else {
            ledc_dev->channel_group[group].channel[channel].conf0.clk_en = 1;
        }
    } else {
        ledc_dev->channel_group[group].channel[channel].conf0.sig_out_en = 0;//This is the output enable control bit for channel
        ledc_dev->channel_group[group].channel[channel].conf1.duty_start = 0;//When duty_num duty_cycle and duty_scale has been configured. these register won't take effect until set duty_start. this bit is automatically cleared by hardware.
        if(group) {
            ledc_dev->channel_group[group].channel[channel].conf0.val &= ~BIT(4);
        } else {
            ledc_dev->channel_group[group].channel[channel].conf0.clk_en = 0;
        }
    }
    LEDC_MUTEX_UNLOCK();
}

uint32_t ledcRead(uint8_t chan)
{
    if(chan > 15) {
        return 0;
    }
    ledc_dev_t * ledc_dev = (volatile ledc_dev_t *)(DR_REG_LEDC_BASE);
    return ledc_dev->channel_group[chan/8].channel[chan%8].duty.duty >> 4;
}

void ledcAttachPin(uint8_t pin, uint8_t chan)
{
    if(chan > 15) {
        return;
    }
    pinMode(pin, OUTPUT);
    pinMatrixOutAttach(pin, ((chan/8)?LEDC_LS_SIG_OUT0_IDX:LEDC_HS_SIG_OUT0_IDX) + (chan%8), false, false);
}

void ledcDetachPin(uint8_t pin)
{
    pinMatrixOutDetach(pin, false, false);
}
