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

#include "esp32-hal-gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "esp_attr.h"
#include "esp_intr.h"
#include "rom/gpio.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_struct.h"
#include "soc/rtc_io_reg.h"

#define ETS_GPIO_INUM       4

typedef struct {
    uint32_t mux;       /*!< Register to modify various pin settings */
    uint32_t pud;       /*!< Register to modify to enable or disable pullups or pulldowns */
    uint32_t pu;        /*!< Bit to set or clear in the above register to enable or disable the pullup, respectively */
    uint32_t pd;        /*!< Bit to set or clear in the above register to enable or disable the pulldown, respectively */
} esp32_gpioMux_t;

const DRAM_ATTR esp32_gpioMux_t esp32_gpioMux[GPIO_PIN_COUNT]={
    {DR_REG_IO_MUX_BASE + 0x44, RTC_IO_TOUCH_PAD1_REG, RTC_IO_TOUCH_PAD1_RUE_M, RTC_IO_TOUCH_PAD1_RDE_M},
    {DR_REG_IO_MUX_BASE + 0x88, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x40, RTC_IO_TOUCH_PAD2_REG, RTC_IO_TOUCH_PAD2_RUE_M, RTC_IO_TOUCH_PAD2_RDE_M},
    {DR_REG_IO_MUX_BASE + 0x84, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x48, RTC_IO_TOUCH_PAD0_REG, RTC_IO_TOUCH_PAD0_RUE_M, RTC_IO_TOUCH_PAD0_RDE_M},
    {DR_REG_IO_MUX_BASE + 0x6c, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x60, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x64, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x68, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x54, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x58, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x5c, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x34, RTC_IO_TOUCH_PAD5_REG, RTC_IO_TOUCH_PAD5_RUE_M, RTC_IO_TOUCH_PAD5_RDE_M},
    {DR_REG_IO_MUX_BASE + 0x38, RTC_IO_TOUCH_PAD4_REG, RTC_IO_TOUCH_PAD4_RUE_M, RTC_IO_TOUCH_PAD4_RDE_M},
    {DR_REG_IO_MUX_BASE + 0x30, RTC_IO_TOUCH_PAD6_REG, RTC_IO_TOUCH_PAD6_RUE_M, RTC_IO_TOUCH_PAD6_RDE_M},
    {DR_REG_IO_MUX_BASE + 0x3c, RTC_IO_TOUCH_PAD3_REG, RTC_IO_TOUCH_PAD3_RUE_M, RTC_IO_TOUCH_PAD3_RDE_M},
    {DR_REG_IO_MUX_BASE + 0x4c, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x50, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x70, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x74, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x78, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x7c, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x80, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x8c, 0, 0, 0},
    {0, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x24, RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_RUE_M, RTC_IO_PDAC1_RDE_M},
    {DR_REG_IO_MUX_BASE + 0x28, RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_RUE_M, RTC_IO_PDAC2_RDE_M},
    {DR_REG_IO_MUX_BASE + 0x2c, RTC_IO_TOUCH_PAD7_REG, RTC_IO_TOUCH_PAD7_RUE_M, RTC_IO_TOUCH_PAD7_RDE_M},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x1c, RTC_IO_XTAL_32K_PAD_REG, RTC_IO_X32P_RUE_M, RTC_IO_X32P_RDE_M},
    {DR_REG_IO_MUX_BASE + 0x20, RTC_IO_XTAL_32K_PAD_REG, RTC_IO_X32N_RUE_M, RTC_IO_X32N_RDE_M},
    {DR_REG_IO_MUX_BASE + 0x14, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x18, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x04, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x08, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x0c, 0, 0, 0},
    {DR_REG_IO_MUX_BASE + 0x10, 0, 0, 0}
};

typedef void (*voidFuncPtr)(void);
static voidFuncPtr __pinInterruptHandlers[GPIO_PIN_COUNT] = {0,};

extern void IRAM_ATTR __pinMode(uint8_t pin, uint8_t mode)
{

    if(pin > 39 || !esp32_gpioMux[pin].mux) {
        return;
    }

    uint32_t pinFunction = 0, pinControl = 0;
    const esp32_gpioMux_t * mux = &esp32_gpioMux[pin];

    if(mode & INPUT) {
        if(pin < 32) {
            GPIO.enable_w1tc = ((uint32_t)1 << pin);
        } else {
            GPIO.enable1_w1tc.val = ((uint32_t)1 << (pin - 32));
        }

        if(mode & PULLUP) {
            pinFunction |= FUN_PU;
        } else if(mode & PULLDOWN) {
            pinFunction |= FUN_PD;
        }
    } else if(mode & OUTPUT) {
        if(pin < 32) {
            GPIO.enable_w1ts = ((uint32_t)1 << pin);
        } else {
            GPIO.enable1_w1ts.val = ((uint32_t)1 << (pin - 32));
        }
    }

    pinFunction |= ((uint32_t)2 << FUN_DRV_S);//what are the drivers?
    pinFunction |= FUN_IE;//input enable but required for output as well?

    if(mode & (INPUT | OUTPUT)) {
        pinFunction |= ((uint32_t)2 << MCU_SEL_S);
    } else if(mode == SPECIAL) {
        pinFunction |= ((uint32_t)(((pin)==1||(pin)==3)?0:1) << MCU_SEL_S);
    } else {
        pinFunction |= ((uint32_t)(mode >> 5) << MCU_SEL_S);
    }

    ESP_REG(mux->mux) = pinFunction;

    if(mux->pud){
        if((mode & INPUT) && (mode & (PULLUP|PULLDOWN))) {
            if(mode & PULLUP) {
                ESP_REG(mux->pud) = (ESP_REG(mux->pud) | mux->pu) & ~(mux->pd);
            } else {
                ESP_REG(mux->pud) = (ESP_REG(mux->pud) | mux->pd) & ~(mux->pu);
            }
        } else {
            ESP_REG(mux->pud) = ESP_REG(mux->pud) & ~(mux->pu | mux->pd);
        }
    }

    if(mode & OPEN_DRAIN) {
        pinControl = (1 << GPIO_PIN0_PAD_DRIVER_S);
    }

    GPIO.pin[pin].val = pinControl;
}

extern void IRAM_ATTR __digitalWrite(uint8_t pin, uint8_t val)
{
    if(val) {
        if(pin < 32) {
            GPIO.out_w1ts = ((uint32_t)1 << pin);
        } else if(pin < 35) {
            GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
        }
    } else {
        if(pin < 32) {
            GPIO.out_w1tc = ((uint32_t)1 << pin);
        } else if(pin < 35) {
            GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
        }
    }
}

extern int IRAM_ATTR __digitalRead(uint8_t pin)
{
    if(pin < 32) {
        return (GPIO.in >> pin) & 0x1;
    } else if(pin < 40) {
        return (GPIO.in1.val >> (pin - 32)) & 0x1;
    }
    return 0;
}


static void IRAM_ATTR __onPinInterrupt(void *arg)
{
    uint32_t gpio_intr_status_l=0;
    uint32_t gpio_intr_status_h=0;

    gpio_intr_status_l = GPIO.status;
    gpio_intr_status_h = GPIO.status1.val;
    GPIO.status_w1tc = gpio_intr_status_l;//Clear intr for gpio0-gpio31
    GPIO.status1_w1tc.val = gpio_intr_status_h;//Clear intr for gpio32-39

    uint8_t pin=0;
    if(gpio_intr_status_l) {
        do {
            if(gpio_intr_status_l & ((uint32_t)1 << pin)) {
                if(__pinInterruptHandlers[pin]) {
                    __pinInterruptHandlers[pin]();
                }
            }
        } while(++pin<32);
    }
    if(gpio_intr_status_h) {
        pin=32;
        do {
            if(gpio_intr_status_h & ((uint32_t)1 << (pin - 32))) {
                if(__pinInterruptHandlers[pin]) {
                    __pinInterruptHandlers[pin]();
                }
            }
        } while(++pin<GPIO_PIN_COUNT);
    }
}

extern void __attachInterrupt(uint8_t pin, voidFuncPtr userFunc, int intr_type)
{
    static bool interrupt_initialized = false;
    static int core_id = 0;
    
    if(!interrupt_initialized) {
        interrupt_initialized = true;
        core_id = xPortGetCoreID();
        ESP_INTR_DISABLE(ETS_GPIO_INUM);
        intr_matrix_set(core_id, ETS_GPIO_INTR_SOURCE, ETS_GPIO_INUM);
        xt_set_interrupt_handler(ETS_GPIO_INUM, &__onPinInterrupt, NULL);
        ESP_INTR_ENABLE(ETS_GPIO_INUM);
    }
    __pinInterruptHandlers[pin] = userFunc;
    ESP_INTR_DISABLE(ETS_GPIO_INUM);
    if(core_id) { //APP_CPU
        GPIO.pin[pin].int_ena = 1;
    } else { //PRO_CPU
        GPIO.pin[pin].int_ena = 4;
    }
    GPIO.pin[pin].int_type = intr_type;
    ESP_INTR_ENABLE(ETS_GPIO_INUM);
}

extern void __detachInterrupt(uint8_t pin)
{
    __pinInterruptHandlers[pin] = NULL;
    ESP_INTR_DISABLE(ETS_GPIO_INUM);
    GPIO.pin[pin].int_ena = 0;
    GPIO.pin[pin].int_type = 0;
    ESP_INTR_ENABLE(ETS_GPIO_INUM);
}


extern void pinMode(uint8_t pin, uint8_t mode) __attribute__ ((weak, alias("__pinMode")));
extern void digitalWrite(uint8_t pin, uint8_t val) __attribute__ ((weak, alias("__digitalWrite")));
extern int digitalRead(uint8_t pin) __attribute__ ((weak, alias("__digitalRead")));
extern void attachInterrupt(uint8_t pin, voidFuncPtr handler, int mode) __attribute__ ((weak, alias("__attachInterrupt")));
extern void detachInterrupt(uint8_t pin) __attribute__ ((weak, alias("__detachInterrupt")));

