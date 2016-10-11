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

#define ETS_GPIO_INUM       4

const uint8_t esp32_gpioToFn[40] = {
    0x44,//0
    0x88,//1
    0x40,//2
    0x84,//3
    0x48,//4
    0x6c,//5
    0x60,//6
    0x64,//7
    0x68,//8
    0x54,//9
    0x58,//10
    0x5c,//11
    0x34,//12
    0x38,//13
    0x30,//14
    0x3c,//15
    0x4c,//16
    0x50,//17
    0x70,//18
    0x74,//19
    0x78,//20
    0x7c,//21
    0x80,//22
    0x8c,//23
    0xFF,//N/A
    0x24,//25
    0x28,//26
    0x2c,//27
    0xFF,//N/A
    0xFF,//N/A
    0xFF,//N/A
    0xFF,//N/A
    0x1c,//32
    0x20,//33
    0x14,//34
    0x18,//35
    0x04,//36
    0x08,//37
    0x0c,//38
    0x10 //39
};

typedef void (*voidFuncPtr)(void);
static voidFuncPtr __pinInterruptHandlers[GPIO_PIN_COUNT] = {0,};

extern void IRAM_ATTR __pinMode(uint8_t pin, uint8_t mode)
{
    uint32_t pinFunction = 0, pinControl = 0;

    if(pin > 39 || esp32_gpioToFn[pin] == 0xFF) {
        return;
    }

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

    if(mode & OPEN_DRAIN) {
        pinControl = (1 << GPIO_PIN0_PAD_DRIVER_S);
    }

    ESP_REG(DR_REG_IO_MUX_BASE + esp32_gpioToFn[pin]) = pinFunction;
    GPIO.pin[pin].val = pinControl;
}

extern void IRAM_ATTR __digitalWrite(uint8_t pin, uint8_t val)
{
    if(pin > 39) {
        return;
    }
    if(val) {
        if(pin < 32) {
            GPIO.out_w1ts = ((uint32_t)1 << pin);
        } else {
            GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
        }
    } else {
        if(pin < 32) {
            GPIO.out_w1tc = ((uint32_t)1 << pin);
        } else {
            GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
        }
    }
}

extern int IRAM_ATTR __digitalRead(uint8_t pin)
{
    if(pin > 39) {
        return 0;
    }
    if(pin < 32) {
        return (GPIO.in >> pin) & 0x1;
    } else {
        return (GPIO.in1.val >> (pin - 32)) & 0x1;
    }
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

