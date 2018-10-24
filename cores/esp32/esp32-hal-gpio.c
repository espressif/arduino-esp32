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

const int8_t esp32_adc2gpio[20] = {36, 37, 38, 39, 32, 33, 34, 35, -1, -1, 4, 0, 2, 15, 13, 12, 14, 27, 25, 26};

const DRAM_ATTR esp32_gpioMux_t esp32_gpioMux[GPIO_PIN_COUNT]={
    {0x44, 11, 11, 1},
    {0x88, -1, -1, -1},
    {0x40, 12, 12, 2},
    {0x84, -1, -1, -1},
    {0x48, 10, 10, 0},
    {0x6c, -1, -1, -1},
    {0x60, -1, -1, -1},
    {0x64, -1, -1, -1},
    {0x68, -1, -1, -1},
    {0x54, -1, -1, -1},
    {0x58, -1, -1, -1},
    {0x5c, -1, -1, -1},
    {0x34, 15, 15, 5},
    {0x38, 14, 14, 4},
    {0x30, 16, 16, 6},
    {0x3c, 13, 13, 3},
    {0x4c, -1, -1, -1},
    {0x50, -1, -1, -1},
    {0x70, -1, -1, -1},
    {0x74, -1, -1, -1},
    {0x78, -1, -1, -1},
    {0x7c, -1, -1, -1},
    {0x80, -1, -1, -1},
    {0x8c, -1, -1, -1},
    {0, -1, -1, -1},
    {0x24, 6, 18, -1}, //DAC1
    {0x28, 7, 19, -1}, //DAC2
    {0x2c, 17, 17, 7},
    {0, -1, -1, -1},
    {0, -1, -1, -1},
    {0, -1, -1, -1},
    {0, -1, -1, -1},
    {0x1c, 9, 4, 9},
    {0x20, 8, 5, 8},
    {0x14, 4, 6, -1},
    {0x18, 5, 7, -1},
    {0x04, 0, 0, -1},
    {0x08, 1, 1, -1},
    {0x0c, 2, 2, -1},
    {0x10, 3, 3, -1}
};

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void*);
typedef struct {
    voidFuncPtr fn;
    void* arg;
    bool functional;
} InterruptHandle_t;
static InterruptHandle_t __pinInterruptHandlers[GPIO_PIN_COUNT] = {0,};

#include "driver/rtc_io.h"

extern void IRAM_ATTR __pinMode(uint8_t pin, uint8_t mode)
{

    if(!digitalPinIsValid(pin)) {
        return;
    }

    uint32_t rtc_reg = rtc_gpio_desc[pin].reg;
    if(mode == ANALOG) {
        if(!rtc_reg) {
            return;//not rtc pin
        }
        //lock rtc
        uint32_t reg_val = ESP_REG(rtc_reg);
        if(reg_val & rtc_gpio_desc[pin].mux){
            return;//already in adc mode
        }
        reg_val &= ~(
                (RTC_IO_TOUCH_PAD1_FUN_SEL_V << rtc_gpio_desc[pin].func)
                |rtc_gpio_desc[pin].ie
                |rtc_gpio_desc[pin].pullup
                |rtc_gpio_desc[pin].pulldown);
        ESP_REG(RTC_GPIO_ENABLE_W1TC_REG) = (1 << (rtc_gpio_desc[pin].rtc_num + RTC_GPIO_ENABLE_W1TC_S));
        ESP_REG(rtc_reg) = reg_val | rtc_gpio_desc[pin].mux;
        //unlock rtc
        ESP_REG(DR_REG_IO_MUX_BASE + esp32_gpioMux[pin].reg) = ((uint32_t)2 << MCU_SEL_S) | ((uint32_t)2 << FUN_DRV_S) | FUN_IE;
        return;
    }

    //RTC pins PULL settings
    if(rtc_reg) {
        //lock rtc
        ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].mux);
        if(mode & PULLUP) {
            ESP_REG(rtc_reg) = (ESP_REG(rtc_reg) | rtc_gpio_desc[pin].pullup) & ~(rtc_gpio_desc[pin].pulldown);
        } else if(mode & PULLDOWN) {
            ESP_REG(rtc_reg) = (ESP_REG(rtc_reg) | rtc_gpio_desc[pin].pulldown) & ~(rtc_gpio_desc[pin].pullup);
        } else {
            ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].pullup | rtc_gpio_desc[pin].pulldown);
        }
        //unlock rtc
    }

    uint32_t pinFunction = 0, pinControl = 0;

    //lock gpio
    if(mode & INPUT) {
        if(pin < 32) {
            GPIO.enable_w1tc = ((uint32_t)1 << pin);
        } else {
            GPIO.enable1_w1tc.val = ((uint32_t)1 << (pin - 32));
        }
    } else if(mode & OUTPUT) {
        if(pin > 33){
            //unlock gpio
            return;//pins above 33 can be only inputs
        } else if(pin < 32) {
            GPIO.enable_w1ts = ((uint32_t)1 << pin);
        } else {
            GPIO.enable1_w1ts.val = ((uint32_t)1 << (pin - 32));
        }
    }

    if(mode & PULLUP) {
        pinFunction |= FUN_PU;
    } else if(mode & PULLDOWN) {
        pinFunction |= FUN_PD;
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

    ESP_REG(DR_REG_IO_MUX_BASE + esp32_gpioMux[pin].reg) = pinFunction;

    if(mode & OPEN_DRAIN) {
        pinControl = (1 << GPIO_PIN0_PAD_DRIVER_S);
    }

    GPIO.pin[pin].val = pinControl;
    //unlock gpio
}

extern void IRAM_ATTR __digitalWrite(uint8_t pin, uint8_t val)
{
    if(val) {
        if(pin < 32) {
            GPIO.out_w1ts = ((uint32_t)1 << pin);
        } else if(pin < 34) {
            GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
        }
    } else {
        if(pin < 32) {
            GPIO.out_w1tc = ((uint32_t)1 << pin);
        } else if(pin < 34) {
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

static intr_handle_t gpio_intr_handle = NULL;

static void IRAM_ATTR __onPinInterrupt()
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
                if(__pinInterruptHandlers[pin].fn) {
                    if(__pinInterruptHandlers[pin].arg){
                        ((voidFuncPtrArg)__pinInterruptHandlers[pin].fn)(__pinInterruptHandlers[pin].arg);
                    } else {
                        __pinInterruptHandlers[pin].fn();
                    }
                }
            }
        } while(++pin<32);
    }
    if(gpio_intr_status_h) {
        pin=32;
        do {
            if(gpio_intr_status_h & ((uint32_t)1 << (pin - 32))) {
                if(__pinInterruptHandlers[pin].fn) {
                    if(__pinInterruptHandlers[pin].arg){
                        ((voidFuncPtrArg)__pinInterruptHandlers[pin].fn)(__pinInterruptHandlers[pin].arg);
                    } else {
                        __pinInterruptHandlers[pin].fn();
                    }
                }
            }
        } while(++pin<GPIO_PIN_COUNT);
    }
}

extern void cleanupFunctional(void* arg);

extern void __attachInterruptFunctionalArg(uint8_t pin, voidFuncPtrArg userFunc, void * arg, int intr_type, bool functional)
{
    static bool interrupt_initialized = false;

    if(!interrupt_initialized) {
        interrupt_initialized = true;
        esp_intr_alloc(ETS_GPIO_INTR_SOURCE, (int)ESP_INTR_FLAG_IRAM, __onPinInterrupt, NULL, &gpio_intr_handle);
    }

    // if new attach without detach remove old info
    if (__pinInterruptHandlers[pin].functional && __pinInterruptHandlers[pin].arg)
    {
    	cleanupFunctional(__pinInterruptHandlers[pin].arg);
    }
    __pinInterruptHandlers[pin].fn = (voidFuncPtr)userFunc;
    __pinInterruptHandlers[pin].arg = arg;
    __pinInterruptHandlers[pin].functional = functional;

    esp_intr_disable(gpio_intr_handle);
    if(esp_intr_get_cpu(gpio_intr_handle)) { //APP_CPU
        GPIO.pin[pin].int_ena = 1;
    } else { //PRO_CPU
        GPIO.pin[pin].int_ena = 4;
    }
    GPIO.pin[pin].int_type = intr_type;
    esp_intr_enable(gpio_intr_handle);
}

extern void __attachInterruptArg(uint8_t pin, voidFuncPtrArg userFunc, void * arg, int intr_type)
{
	__attachInterruptFunctionalArg(pin, userFunc, arg, intr_type, false);
}

extern void __attachInterrupt(uint8_t pin, voidFuncPtr userFunc, int intr_type) {
    __attachInterruptFunctionalArg(pin, (voidFuncPtrArg)userFunc, NULL, intr_type, false);
}

extern void __detachInterrupt(uint8_t pin)
{
    esp_intr_disable(gpio_intr_handle);
    if (__pinInterruptHandlers[pin].functional && __pinInterruptHandlers[pin].arg)
    {
    	cleanupFunctional(__pinInterruptHandlers[pin].arg);
    }
    __pinInterruptHandlers[pin].fn = NULL;
    __pinInterruptHandlers[pin].arg = NULL;
    __pinInterruptHandlers[pin].arg = false;

    GPIO.pin[pin].int_ena = 0;
    GPIO.pin[pin].int_type = 0;
    esp_intr_enable(gpio_intr_handle);
}


extern void pinMode(uint8_t pin, uint8_t mode) __attribute__ ((weak, alias("__pinMode")));
extern void digitalWrite(uint8_t pin, uint8_t val) __attribute__ ((weak, alias("__digitalWrite")));
extern int digitalRead(uint8_t pin) __attribute__ ((weak, alias("__digitalRead")));
extern void attachInterrupt(uint8_t pin, voidFuncPtr handler, int mode) __attribute__ ((weak, alias("__attachInterrupt")));
extern void attachInterruptArg(uint8_t pin, voidFuncPtrArg handler, void * arg, int mode) __attribute__ ((weak, alias("__attachInterruptArg")));
extern void detachInterrupt(uint8_t pin) __attribute__ ((weak, alias("__detachInterrupt")));

