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
#include "pins_arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_system.h"

#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/rom/ets_sys.h"
#include "esp32/rom/gpio.h"
#include "esp_intr_alloc.h"
#include "soc/rtc_io_reg.h"
#define GPIO_FUNC 2
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/ets_sys.h"
#include "esp32s2/rom/gpio.h"
#include "esp_intr_alloc.h"
#include "soc/periph_defs.h"
#include "soc/rtc_io_reg.h"
#define GPIO_FUNC 1
#else 
#define USE_ESP_IDF_GPIO 1
#endif
#else // ESP32 Before IDF 4.0
#include "rom/ets_sys.h"
#include "rom/gpio.h"
#include "esp_intr.h"
#endif

#if CONFIG_IDF_TARGET_ESP32
const int8_t esp32_adc2gpio[20] = {36, 37, 38, 39, 32, 33, 34, 35, -1, -1, 4, 0, 2, 15, 13, 12, 14, 27, 25, 26};
#elif CONFIG_IDF_TARGET_ESP32S2
const int8_t esp32_adc2gpio[20] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
#endif

const DRAM_ATTR esp32_gpioMux_t esp32_gpioMux[SOC_GPIO_PIN_COUNT]={
#if CONFIG_IDF_TARGET_ESP32
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
    {0x1c, 9, 4, 8},
    {0x20, 8, 5, 9},
    {0x14, 4, 6, -1},
    {0x18, 5, 7, -1},
    {0x04, 0, 0, -1},
    {0x08, 1, 1, -1},
    {0x0c, 2, 2, -1},
    {0x10, 3, 3, -1}
#elif CONFIG_IDF_TARGET_ESP32S2
    {0x04,  0, -1, -1},
    {0x08,  1,  0,  1},
    {0x0c,  2,  1,  2},
    {0x10,  3,  2,  3},
    {0x14,  4,  3,  4},
    {0x18,  5,  4,  5},
    {0x1c,  6,  5,  6},
    {0x20,  7,  6,  7},
    {0x24,  8,  7,  8},
    {0x28,  9,  8,  9},//FSPI_HD
    {0x2c, 10,  9, 10},//FSPI_CS0 / FSPI_D4
    {0x30, 11, 10, 11},//FSPI_D / FSPI_D5
    {0x34, 12, 11, 12},//FSPI_CLK / FSPI_D6
    {0x38, 13, 12, 13},//FSPI_Q / FSPI_D7
    {0x3c, 14, 13, 14},//FSPI_WP / FSPI_DQS
    {0x40, 15, 14, -1},//32K+ / RTS0
    {0x44, 16, 15, -1},//32K- / CTS0
    {0x48, 17, 16, -1},//DAC1 / TXD1
    {0x4c, 18, 17, -1},//DAC2 / RXD1
    {0x50, 19, 18, -1},//USB D- / RTS1
    {0x54, 20, 19, -1},//USB D+ / CTS1
    {0x58, 21, -1, -1},//SDA?
    {   0, -1, -1, -1},//UNAVAILABLE
    {   0, -1, -1, -1},//UNAVAILABLE
    {   0, -1, -1, -1},//UNAVAILABLE
    {   0, -1, -1, -1},//UNAVAILABLE
    {0x6c, -1, -1, -1},//RESERVED SPI_CS1
    {0x70, -1, -1, -1},//RESERVED SPI_HD
    {0x74, -1, -1, -1},//RESERVED SPI_WP
    {0x78, -1, -1, -1},//RESERVED SPI_CS0
    {0x7c, -1, -1, -1},//RESERVED SPI_CLK
    {0x80, -1, -1, -1},//RESERVED SPI_Q
    {0x84, -1, -1, -1},//RESERVED SPI_D
    {0x88, -1, -1, -1},//FSPI_HD
    {0x8c, -1, -1, -1},//FSPI_CS0
    {0x90, -1, -1, -1},//FSPI_D
    {0x94, -1, -1, -1},//FSPI_CLK
    {0x98, -1, -1, -1},//FSPI_Q
    {0x9c, -1, -1, -1},//FSPI_WP
    {0xa0, -1, -1, -1},//MTCK
    {0xa4, -1, -1, -1},//MTDO
    {0xa8, -1, -1, -1},//MTDI
    {0xac, -1, -1, -1},//MTMS
    {0xb0, -1, -1, -1},//TXD0
    {0xb4, -1, -1, -1},//RXD0
    {0xb8, -1, -1, -1},//SCL?
    {0xbc, -1, -1, -1},//INPUT ONLY
    {0, -1, -1, -1}
#endif
};

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void*);
typedef struct {
    voidFuncPtr fn;
    void* arg;
    bool functional;
} InterruptHandle_t;
static InterruptHandle_t __pinInterruptHandlers[SOC_GPIO_PIN_COUNT] = {0,};

#include "driver/rtc_io.h"

extern void ARDUINO_ISR_ATTR __pinMode(uint8_t pin, uint8_t mode)
{
#if USE_ESP_IDF_GPIO
	if (!GPIO_IS_VALID_GPIO(pin)) {
		return;
	}
	gpio_config_t conf = {
		    .pin_bit_mask = (1ULL<<pin),			/*!< GPIO pin: set with bit mask, each bit maps to a GPIO */
		    .mode = GPIO_MODE_DISABLE,              /*!< GPIO mode: set input/output mode                     */
		    .pull_up_en = GPIO_PULLUP_DISABLE,      /*!< GPIO pull-up                                         */
		    .pull_down_en = GPIO_PULLDOWN_DISABLE,  /*!< GPIO pull-down                                       */
		    .intr_type = GPIO_INTR_DISABLE      	/*!< GPIO interrupt type                                  */
	};
	if (mode < 0x20) {//io
		conf.mode = mode & (INPUT | OUTPUT);
		if (mode & OPEN_DRAIN) {
			conf.mode |= GPIO_MODE_DEF_OD;
		}
		if (mode & PULLUP) {
			conf.pull_up_en = GPIO_PULLUP_ENABLE;
		}
		if (mode & PULLDOWN) {
			conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
		}
	}
	gpio_config(&conf);

	if(mode == SPECIAL){
#if CONFIG_IDF_TARGET_ESP32
		PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], (uint32_t)(((pin)==RX||(pin)==TX)?0:1));
#elif CONFIG_IDF_TARGET_ESP32S2
		PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], (uint32_t)(((pin)==RX||(pin)==TX)?0:2));
#endif
	} else if(mode == ANALOG){
#if !CONFIG_IDF_TARGET_ESP32C3
		//adc_gpio_init(ADC_UNIT_1, ADC_CHANNEL_0);
#endif
	} else if(mode >= 0x20 && mode < ANALOG) {//function
		PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], mode >> 5);
	}
#else
    if(!digitalPinIsValid(pin)) {
        return;
    }

    int8_t rtc_io = esp32_gpioMux[pin].rtc;
    uint32_t rtc_reg = (rtc_io != -1)?rtc_io_desc[rtc_io].reg:0;
    if(mode == ANALOG) {
        if(!rtc_reg) {
            return;//not rtc pin
        }
#if CONFIG_IDF_TARGET_ESP32S2
        SENS.sar_io_mux_conf.iomux_clk_gate_en = 1;
#endif
        SET_PERI_REG_MASK(rtc_io_desc[rtc_io].reg, (rtc_io_desc[rtc_io].mux));
        SET_PERI_REG_BITS(rtc_io_desc[rtc_io].reg, RTC_IO_TOUCH_PAD1_FUN_SEL_V, 0, rtc_io_desc[rtc_io].func);

        RTCIO.pin[rtc_io].pad_driver = 0;//OD = 1
        RTCIO.enable_w1tc.w1tc = (1U << rtc_io);
        CLEAR_PERI_REG_MASK(rtc_io_desc[rtc_io].reg, rtc_io_desc[rtc_io].ie);

        if (rtc_io_desc[rtc_io].pullup) {
            CLEAR_PERI_REG_MASK(rtc_io_desc[rtc_io].reg, rtc_io_desc[rtc_io].pullup);
        }
        if (rtc_io_desc[rtc_io].pulldown) {
            CLEAR_PERI_REG_MASK(rtc_io_desc[rtc_io].reg, rtc_io_desc[rtc_io].pulldown);
        }
        ESP_REG(DR_REG_IO_MUX_BASE + esp32_gpioMux[pin].reg) = ((uint32_t)GPIO_FUNC << MCU_SEL_S) | ((uint32_t)2 << FUN_DRV_S) | FUN_IE;
        return;
    }

    //RTC pins PULL settings
    if(rtc_reg) {
        ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_io_desc[rtc_io].mux);
        if(mode & PULLUP) {
            ESP_REG(rtc_reg) = (ESP_REG(rtc_reg) | rtc_io_desc[rtc_io].pullup) & ~(rtc_io_desc[rtc_io].pulldown);
        } else if(mode & PULLDOWN) {
            ESP_REG(rtc_reg) = (ESP_REG(rtc_reg) | rtc_io_desc[rtc_io].pulldown) & ~(rtc_io_desc[rtc_io].pullup);
        } else {
            ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_io_desc[rtc_io].pullup | rtc_io_desc[rtc_io].pulldown);
        }
    }

    uint32_t pinFunction = 0, pinControl = 0;

    if(mode & INPUT) {
        if(pin < 32) {
            GPIO.enable_w1tc = ((uint32_t)1 << pin);
        } else {
            GPIO.enable1_w1tc.val = ((uint32_t)1 << (pin - 32));
        }
    } else if(mode & OUTPUT) {
        if(pin >= NUM_OUPUT_PINS){
            return;
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
        pinFunction |= ((uint32_t)PIN_FUNC_GPIO << MCU_SEL_S);
    } else if(mode == SPECIAL) {
#if CONFIG_IDF_TARGET_ESP32
        pinFunction |= ((uint32_t)(((pin)==RX||(pin)==TX)?0:1) << MCU_SEL_S);
#elif CONFIG_IDF_TARGET_ESP32S2
        pinFunction |= ((uint32_t)(((pin)==RX||(pin)==TX)?0:2) << MCU_SEL_S);
#endif
    } else {
        pinFunction |= ((uint32_t)(mode >> 5) << MCU_SEL_S);
    }

    ESP_REG(DR_REG_IO_MUX_BASE + esp32_gpioMux[pin].reg) = pinFunction;

    if(mode & OPEN_DRAIN) {
        pinControl = (1 << GPIO_PIN0_PAD_DRIVER_S);
    }

    GPIO.pin[pin].val = pinControl;
#endif
}

extern void ARDUINO_ISR_ATTR __digitalWrite(uint8_t pin, uint8_t val)
{
#if USE_ESP_IDF_GPIO
	gpio_set_level((gpio_num_t)pin, val);
#elif CONFIG_IDF_TARGET_ESP32C3
    if (val) {
    	GPIO.out_w1ts.out_w1ts = (1 << pin);
    } else {
    	GPIO.out_w1tc.out_w1tc = (1 << pin);
    }
#else
    if(val) {
        if(pin < 32) {
            GPIO.out_w1ts = ((uint32_t)1 << pin);
        } else if(pin < NUM_OUPUT_PINS) {
            GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
        }
    } else {
        if(pin < 32) {
            GPIO.out_w1tc = ((uint32_t)1 << pin);
        } else if(pin < NUM_OUPUT_PINS) {
            GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
        }
    }
#endif
}

extern int ARDUINO_ISR_ATTR __digitalRead(uint8_t pin)
{
#if USE_ESP_IDF_GPIO
	return gpio_get_level((gpio_num_t)pin);
#elif CONFIG_IDF_TARGET_ESP32C3
	return (GPIO.in.data >> pin) & 0x1;
#else
    if(pin < 32) {
        return (GPIO.in >> pin) & 0x1;
    } else if(pin < GPIO_PIN_COUNT) {
        return (GPIO.in1.val >> (pin - 32)) & 0x1;
    }
    return 0;
#endif
}

#if USE_ESP_IDF_GPIO
static void ARDUINO_ISR_ATTR __onPinInterrupt(void * arg) {
	InterruptHandle_t * isr = (InterruptHandle_t*)arg;
    if(isr->fn) {
        if(isr->arg){
            ((voidFuncPtrArg)isr->fn)(isr->arg);
        } else {
        	isr->fn();
        }
    }
}
#else
static intr_handle_t gpio_intr_handle = NULL;

static void ARDUINO_ISR_ATTR __onPinInterrupt()
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
#endif

extern void cleanupFunctional(void* arg);

extern void __attachInterruptFunctionalArg(uint8_t pin, voidFuncPtrArg userFunc, void * arg, int intr_type, bool functional)
{
    static bool interrupt_initialized = false;

    if(!interrupt_initialized) {
#if USE_ESP_IDF_GPIO
    	esp_err_t err = gpio_install_isr_service((int)ARDUINO_ISR_FLAG);
    	interrupt_initialized = (err == ESP_OK) || (err == ESP_ERR_INVALID_STATE);
#else
        interrupt_initialized = true;
        esp_intr_alloc(ETS_GPIO_INTR_SOURCE, (int)ARDUINO_ISR_FLAG, __onPinInterrupt, NULL, &gpio_intr_handle);
#endif
    }
    if(!interrupt_initialized) {
    	log_e("GPIO ISR Service Failed To Start");
    	return;
    }

    // if new attach without detach remove old info
    if (__pinInterruptHandlers[pin].functional && __pinInterruptHandlers[pin].arg)
    {
    	cleanupFunctional(__pinInterruptHandlers[pin].arg);
    }
    __pinInterruptHandlers[pin].fn = (voidFuncPtr)userFunc;
    __pinInterruptHandlers[pin].arg = arg;
    __pinInterruptHandlers[pin].functional = functional;

#if USE_ESP_IDF_GPIO
    gpio_set_intr_type((gpio_num_t)pin, (gpio_int_type_t)(intr_type & 0x7));
    if(intr_type & 0x8){
    	gpio_wakeup_enable((gpio_num_t)pin, (gpio_int_type_t)(intr_type & 0x7));
    }
    gpio_isr_handler_add((gpio_num_t)pin, __onPinInterrupt, &__pinInterruptHandlers[pin]);
    gpio_intr_enable((gpio_num_t)pin);
#else
    esp_intr_disable(gpio_intr_handle);
#if CONFIG_IDF_TARGET_ESP32
    if(esp_intr_get_cpu(gpio_intr_handle)) { //APP_CPU
#endif
        GPIO.pin[pin].int_ena = 1;
#if CONFIG_IDF_TARGET_ESP32
    } else { //PRO_CPU
        GPIO.pin[pin].int_ena = 4;
    }
#endif
    GPIO.pin[pin].int_type = intr_type;
    esp_intr_enable(gpio_intr_handle);
#endif
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
#if USE_ESP_IDF_GPIO
	gpio_intr_disable((gpio_num_t)pin);
	gpio_isr_handler_remove((gpio_num_t)pin);
	gpio_wakeup_disable((gpio_num_t)pin);
#else
    esp_intr_disable(gpio_intr_handle);
#endif
    if (__pinInterruptHandlers[pin].functional && __pinInterruptHandlers[pin].arg)
    {
    	cleanupFunctional(__pinInterruptHandlers[pin].arg);
    }
    __pinInterruptHandlers[pin].fn = NULL;
    __pinInterruptHandlers[pin].arg = NULL;
    __pinInterruptHandlers[pin].functional = false;

#if USE_ESP_IDF_GPIO
    gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
#else
    GPIO.pin[pin].int_ena = 0;
    GPIO.pin[pin].int_type = 0;
    esp_intr_enable(gpio_intr_handle);
#endif
}


extern void pinMode(uint8_t pin, uint8_t mode) __attribute__ ((weak, alias("__pinMode")));
extern void digitalWrite(uint8_t pin, uint8_t val) __attribute__ ((weak, alias("__digitalWrite")));
extern int digitalRead(uint8_t pin) __attribute__ ((weak, alias("__digitalRead")));
extern void attachInterrupt(uint8_t pin, voidFuncPtr handler, int mode) __attribute__ ((weak, alias("__attachInterrupt")));
extern void attachInterruptArg(uint8_t pin, voidFuncPtrArg handler, void * arg, int mode) __attribute__ ((weak, alias("__attachInterruptArg")));
extern void detachInterrupt(uint8_t pin) __attribute__ ((weak, alias("__detachInterrupt")));

