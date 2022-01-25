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

#include "soc/soc_caps.h"

#if SOC_TOUCH_SENSOR_NUM > 0
#include "driver/touch_sensor.h"
#include "esp32-hal-touch.h"

/*
    Internal Private Touch Data Structure and Functions
*/

static uint16_t __touchSleepCycles = 0x1000;
static uint16_t __touchMeasureCycles = 0x1000;

typedef void (*voidFuncPtr)(void);
typedef void (*voidArgFuncPtr)(void *);

typedef struct {
    voidFuncPtr fn;
    bool callWithArgs;
    void* arg;
#if SOC_TOUCH_VERSION_2     // Only for ESP32S2 and ESP32S3
    bool lastStatusIsPressed;
#endif
} TouchInterruptHandle_t;

static TouchInterruptHandle_t __touchInterruptHandlers[SOC_TOUCH_SENSOR_NUM] = {0,};

static void ARDUINO_ISR_ATTR __touchISR(void * arg)
{
#if SOC_TOUCH_VERSION_1         // ESP32 
    uint32_t pad_intr = touch_pad_get_status();
    //clear interrupt
    touch_pad_clear_status();
    // call Pad ISR User callback
    for (int i = 0; i < SOC_TOUCH_SENSOR_NUM; i++) {
        if ((pad_intr >> i) & 0x01) {
            if(__touchInterruptHandlers[i].fn){
                // keeping backward compatibility with "void cb(void)" and with new "void cb(vooid *)"
                if (__touchInterruptHandlers[i].callWithArgs) {
                    ((voidArgFuncPtr)__touchInterruptHandlers[i].fn)(__touchInterruptHandlers[i].arg);
                } else {
                    __touchInterruptHandlers[i].fn();
                }
            }
        }
    }
#endif

#if SOC_TOUCH_VERSION_2     // ESP32S2, ESP32S3
            touch_pad_intr_mask_t evt = touch_pad_read_intr_status_mask();
            uint8_t pad_num = touch_pad_get_current_meas_channel();
            if (evt & TOUCH_PAD_INTR_MASK_ACTIVE) {
                // touch has been pressed / touched
                __touchInterruptHandlers[pad_num].lastStatusIsPressed = true;
            }
            if (evt & TOUCH_PAD_INTR_MASK_INACTIVE) {
                // touch has been released / untouched
                __touchInterruptHandlers[pad_num].lastStatusIsPressed = false;
            }
            if(__touchInterruptHandlers[pad_num].fn){
                // keeping backward compatibility with "void cb(void)" and with new "void cb(vooid *)"
                if (__touchInterruptHandlers[pad_num].callWithArgs) {
                    ((voidArgFuncPtr)__touchInterruptHandlers[pad_num].fn)(__touchInterruptHandlers[pad_num].arg);
                } else {
                    __touchInterruptHandlers[pad_num].fn();
                }
            }
#endif
}



static void __touchSetCycles(uint16_t measure, uint16_t sleep)
{
    __touchSleepCycles = sleep;
    __touchMeasureCycles = measure;
#if SOC_TOUCH_VERSION_1  || SOC_TOUCH_VERSION_2         // ESP32 || ESP32S2, ESP32S3
    touch_pad_set_meas_time(sleep, measure);
#endif
}



static void __touchInit()
{
    static bool initialized = false;
    if(initialized){
        return;
    }
    initialized = true;

#if SOC_TOUCH_VERSION_1                         // ESP32
    touch_pad_init();
    // the next two lines will drive the touch reading values
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V);
    touch_pad_set_meas_time(__touchMeasureCycles, __touchSleepCycles);
    // Touch Sensor Timer initiated
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_filter_start(10);
    // Initial no Threshold and setup
    for (int i = 0; i < SOC_TOUCH_SENSOR_NUM; i++) {
        __touchInterruptHandlers[i].fn =  NULL;
        touch_pad_config(i, SOC_TOUCH_PAD_THRESHOLD_MAX);
    }
    // keep ISR activated - it can run all together (ISR + touchRead())
    touch_pad_isr_register(__touchISR, NULL);
    touch_pad_intr_enable();
#endif

#if SOC_TOUCH_VERSION_2                         // ESP32S2, ESP32S3
    touch_pad_init();
    // the next two lines will drive the touch reading values
    touch_pad_set_meas_time(TOUCH_PAD_SLEEP_CYCLE_DEFAULT, TOUCH_PAD_MEASURE_CYCLE_DEFAULT);
    touch_pad_set_voltage(TOUCH_PAD_HIGH_VOLTAGE_THRESHOLD, TOUCH_PAD_LOW_VOLTAGE_THRESHOLD, TOUCH_PAD_ATTEN_VOLTAGE_THRESHOLD);
    touch_pad_set_idle_channel_connect(TOUCH_PAD_IDLE_CH_CONNECT_DEFAULT);
    touch_pad_denoise_t denoise = {
        .grade = TOUCH_PAD_DENOISE_BIT4,
        .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
    };
    touch_pad_denoise_set_config(&denoise);
    touch_pad_denoise_enable();
    // Touch Sensor Timer initiated
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_fsm_start();

    // Initial no Threshold and setup - TOUCH0 is internal denoise channel
    for (int i = 0; i < SOC_TOUCH_SENSOR_NUM; i++) {
        __touchInterruptHandlers[i].fn =  NULL;
        touch_pad_config(i);
    }
    // keep ISR activated - it can run all together (ISR + touchRead())
    touch_pad_isr_register(__touchISR, NULL, TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE);
    touch_pad_intr_enable(TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE);
#endif
}

static uint32_t __touchRead(uint8_t pin)
{
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad < 0){
        return 0;
    }
    __touchInit();

#if SOC_TOUCH_VERSION_1                 // ESP32
    uint16_t    touch_value;
#endif

#if SOC_TOUCH_VERSION_2                 // ESP32S2 ESP32S3
    uint32_t    touch_value;
#endif

#if SOC_TOUCH_VERSION_1  || SOC_TOUCH_VERSION_2         // ESP32 || ESP32S2, ESP32S3
    touch_pad_read_raw_data(pad, &touch_value);
#endif

    return (uint32_t) touch_value;
}

static void __touchConfigInterrupt(uint8_t pin, void (*userFunc)(void), void *Args, uint32_t threshold, bool callWithArgs)
{
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad < 0){
        return;
    }

    if (userFunc == NULL) {
        // dettach ISR User Call
        __touchInterruptHandlers[pad].fn = NULL;
        threshold = SOC_TOUCH_PAD_THRESHOLD_MAX;   // deactivate the ISR with SOC_TOUCH_PAD_THRESHOLD_MAX
    } else {
        // attach ISR User Call
        __touchInit();
        __touchInterruptHandlers[pad].fn = userFunc;
        __touchInterruptHandlers[pad].callWithArgs = callWithArgs;
        __touchInterruptHandlers[pad].arg = Args;
    }

#if SOC_TOUCH_VERSION_1                         // ESP32
    touch_pad_config(pad, (uint16_t) threshold);
#endif

#if SOC_TOUCH_VERSION_2                         // ESP32S2, ESP32S3
    touch_pad_set_thresh(pad, threshold);
#endif
}

// it keeps backwards compatibility
static void __touchAttachInterrupt(uint8_t pin, void (*userFunc)(void), uint32_t threshold)
{
    __touchConfigInterrupt(pin, userFunc, NULL, threshold, false);
}

// new additional version of the API with User Args
static void __touchAttachArgsInterrupt(uint8_t pin, void (*userFunc)(void), void *args, uint32_t threshold)
{
    __touchConfigInterrupt(pin, userFunc, args, threshold, true);
}

// new additional API to dettach touch ISR
static void __touchDettachInterrupt(uint8_t pin)
{
    __touchConfigInterrupt(pin, NULL, NULL, 0, false);  // userFunc as NULL acts as dettaching 
}
/*
    External Public Touch API Functions
*/

#if SOC_TOUCH_VERSION_1     // Only for ESP32 SoC
void touchInterruptSetThresholdDirection(bool mustbeLower) {
    if (mustbeLower) {
        touch_pad_set_trigger_mode(TOUCH_TRIGGER_BELOW);
    } else {
        touch_pad_set_trigger_mode(TOUCH_TRIGGER_ABOVE);
    }
}
#endif

#if SOC_TOUCH_VERSION_2     // Only for ESP32S2 and ESP32S3
// returns true if touch pad has been and continues pressed and false otherwise 
bool touchInterruptGetLastStatus(uint8_t pin) {
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad < 0){
        return false;
    }

    return __touchInterruptHandlers[pad].lastStatusIsPressed;
}
#endif



extern uint32_t touchRead(uint8_t) __attribute__ ((weak, alias("__touchRead")));
extern void touchAttachInterrupt(uint8_t, voidFuncPtr, uint32_t) __attribute__ ((weak, alias("__touchAttachInterrupt")));
extern void touchAttachInterruptArg(uint8_t, voidArgFuncPtr, void *, uint32_t) __attribute__ ((weak, alias("__touchAttachArgsInterrupt")));
extern void touchDetachInterrupt(uint8_t) __attribute__ ((weak, alias("__touchDettachInterrupt")));
extern void touchSetCycles(uint16_t, uint16_t) __attribute__ ((weak, alias("__touchSetCycles")));

#endif      // #if SOC_TOUCH_SENSOR_NUM > 0
