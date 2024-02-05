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

#if SOC_TOUCH_SENSOR_SUPPORTED
#include "driver/touch_sensor.h"
#include "esp32-hal-touch.h"
#include "esp32-hal-periman.h"

/*
    Internal Private Touch Data Structure and Functions
*/

#if SOC_TOUCH_VERSION_1         // ESP32 
static uint16_t __touchSleepCycles = 0x1000;
static uint16_t __touchMeasureCycles = 0x1000;
#elif SOC_TOUCH_VERSION_2       // ESP32S2, ESP32S3
static uint16_t __touchSleepCycles = TOUCH_PAD_SLEEP_CYCLE_DEFAULT;
static uint16_t __touchMeasureCycles = TOUCH_PAD_MEASURE_CYCLE_DEFAULT;
#endif

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

static uint8_t used_pads = 0;
static bool initialized = false;
static bool channels_initialized[SOC_TOUCH_SENSOR_NUM] = { false };

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
#elif SOC_TOUCH_VERSION_2     // ESP32S2, ESP32S3
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
#if SOC_TOUCH_VERSION_1                         // ESP32
    touch_pad_set_measurement_clock_cycles(measure);
#elif SOC_TOUCH_VERSION_2                         // ESP32S2, ESP32S3
    touch_pad_set_charge_discharge_times(measure);
#endif
    touch_pad_set_measurement_interval(sleep);
}

static bool touchDetachBus(void * pin){
    int8_t pad = digitalPinToTouchChannel((int)(pin-1));
    channels_initialized[pad] = false;
    used_pads--;
    if (used_pads == 0) {
        if (touch_pad_deinit() != ESP_OK) //deinit touch module, as no pads are used
        {
            log_e("Touch module deinit failed!");
            return false;
        }
        initialized = false;
    }
    return true;
}

static void __touchInit()
{
    if(initialized){
        return;
    }
 
   esp_err_t err = ESP_OK;

#if SOC_TOUCH_VERSION_1                         // ESP32
    err = touch_pad_init();
    if (err != ESP_OK) {
        goto err;
    }
    // the next two lines will drive the touch reading values -- both will return ESP_OK
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V); 
    touch_pad_set_measurement_clock_cycles(__touchMeasureCycles);
    touch_pad_set_measurement_interval(__touchSleepCycles);
    // Touch Sensor Timer initiated
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);   // returns ESP_OK
    err = touch_pad_filter_start(10);
    if (err != ESP_OK) {
        goto err;
    }
    // keep ISR activated - it can run all together (ISR + touchRead())
    err = touch_pad_isr_register(__touchISR, NULL);
    if (err != ESP_OK) {
        goto err;
    }
    touch_pad_intr_enable();  // returns ESP_OK
#elif SOC_TOUCH_VERSION_2                         // ESP32S2, ESP32S3
    err = touch_pad_init();
    if (err != ESP_OK) {
        goto err;
    }
    // the next lines will drive the touch reading values -- all os them return ESP_OK
    touch_pad_set_charge_discharge_times(__touchMeasureCycles);
    touch_pad_set_measurement_interval(__touchSleepCycles);
    touch_pad_set_voltage(TOUCH_PAD_HIGH_VOLTAGE_THRESHOLD, TOUCH_PAD_LOW_VOLTAGE_THRESHOLD, TOUCH_PAD_ATTEN_VOLTAGE_THRESHOLD);
    touch_pad_set_idle_channel_connect(TOUCH_PAD_IDLE_CH_CONNECT_DEFAULT);
    touch_pad_denoise_t denoise = {
        .grade = TOUCH_PAD_DENOISE_BIT4,
        .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
    };
    touch_pad_denoise_set_config(&denoise);
    touch_pad_denoise_enable();
    // Touch Sensor Timer initiated
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);  // returns ESP_OK
    touch_pad_fsm_start();                         // returns ESP_OK
    //ISR setup moved to __touchChannelInit
#endif

    initialized = true;
    return;
err:
    log_e(" Touch sensor initialization error.");
    initialized = false;
    return;
}

static void __touchChannelInit(int pad)
{
    if(channels_initialized[pad]){
        return;
    }

#if SOC_TOUCH_VERSION_1                         // ESP32
    // Initial no Threshold and setup
    __touchInterruptHandlers[pad].fn =  NULL;
    touch_pad_config(pad, SOC_TOUCH_PAD_THRESHOLD_MAX);  // returns ESP_OK
#elif SOC_TOUCH_VERSION_2                       // ESP32S2, ESP32S3
    // Initial no Threshold and setup
    __touchInterruptHandlers[pad].fn =  NULL;
    touch_pad_config(pad);                       // returns ESP_OK
    // keep ISR activated - it can run all together (ISR + touchRead())
    esp_err_t err = touch_pad_isr_register(__touchISR, NULL, TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE);
    if (err != ESP_OK) {
        log_e(" Touch sensor initialization error.");
        return;
    }
    touch_pad_intr_enable(TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE); // returns ESP_OK
#endif

    channels_initialized[pad] = true;
    used_pads++;
    delay(20);  //delay needed before reading from touch channel after config
}

static touch_value_t __touchRead(uint8_t pin)
{
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad < 0){
        log_e(" No touch pad on selected pin!");
        return 0;
    }

    if(perimanGetPinBus(pin, ESP32_BUS_TYPE_TOUCH) == NULL){
        perimanSetBusDeinit(ESP32_BUS_TYPE_TOUCH, touchDetachBus);
        if(!perimanClearPinBus(pin)){
            return 0;
        }
        __touchInit();
        __touchChannelInit(pad);

        if(!perimanSetPinBus(pin, ESP32_BUS_TYPE_TOUCH, (void *)(pin+1), -1, pad)){
            touchDetachBus((void *)(pin+1));
            return 0;
        }
    }

    touch_value_t touch_value;
    touch_pad_read_raw_data(pad, &touch_value);

    return touch_value;
}

static void __touchConfigInterrupt(uint8_t pin, void (*userFunc)(void), void *Args, touch_value_t threshold, bool callWithArgs)
{
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad < 0){
        log_e(" No touch pad on selected pin!");
        return;
    }

    if (userFunc == NULL) {
        // dettach ISR User Call
        __touchInterruptHandlers[pad].fn = NULL;
        threshold = SOC_TOUCH_PAD_THRESHOLD_MAX;   // deactivate the ISR with SOC_TOUCH_PAD_THRESHOLD_MAX
    } else {
        // attach ISR User Call
        __touchInit();
        __touchChannelInit(pad);
        __touchInterruptHandlers[pad].fn = userFunc;
        __touchInterruptHandlers[pad].callWithArgs = callWithArgs;
        __touchInterruptHandlers[pad].arg = Args;
    }

    touch_pad_set_thresh(pad, threshold);
}

// it keeps backwards compatibility
static void __touchAttachInterrupt(uint8_t pin, void (*userFunc)(void), touch_value_t threshold)
{
    __touchConfigInterrupt(pin, userFunc, NULL, threshold, false);
}

// new additional version of the API with User Args
static void __touchAttachArgsInterrupt(uint8_t pin, void (*userFunc)(void), void *args, touch_value_t threshold)
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

#if SOC_TOUCH_VERSION_1        // Only for ESP32 SoC
void touchInterruptSetThresholdDirection(bool mustbeLower) {
    if (mustbeLower) {
        touch_pad_set_trigger_mode(TOUCH_TRIGGER_BELOW);
    } else {
        touch_pad_set_trigger_mode(TOUCH_TRIGGER_ABOVE);
    }
}
#elif SOC_TOUCH_VERSION_2     // Only for ESP32S2 and ESP32S3
// returns true if touch pad has been and continues pressed and false otherwise 
bool touchInterruptGetLastStatus(uint8_t pin) {
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad < 0){
        return false;
    }

    return __touchInterruptHandlers[pad].lastStatusIsPressed;
}
#endif

void touchSleepWakeUpEnable(uint8_t pin, touch_value_t threshold)
{
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad < 0){
        log_e(" No touch pad on selected pin!");
        return;
    }

    if(perimanGetPinBus(pin, ESP32_BUS_TYPE_TOUCH) == NULL){
        perimanSetBusDeinit(ESP32_BUS_TYPE_TOUCH, touchDetachBus);
        __touchInit();
        __touchChannelInit(pad);
        if(!perimanSetPinBus(pin, ESP32_BUS_TYPE_TOUCH, (void *)(pin+1), -1, pad)){
            log_e("Failed to set bus to Peripheral manager");
            touchDetachBus((void *)(pin+1));
            return;
        }
    }
    #if SOC_TOUCH_VERSION_1        // Only for ESP32 SoC
    touch_pad_set_thresh(pad, threshold);
   
    #elif SOC_TOUCH_VERSION_2
    touch_pad_sleep_channel_enable(pad, true);
    touch_pad_sleep_set_threshold(pad, threshold);

    #endif 
    esp_sleep_enable_touchpad_wakeup();
}

extern touch_value_t touchRead(uint8_t) __attribute__ ((weak, alias("__touchRead")));
extern void touchAttachInterrupt(uint8_t, voidFuncPtr, touch_value_t) __attribute__ ((weak, alias("__touchAttachInterrupt")));
extern void touchAttachInterruptArg(uint8_t, voidArgFuncPtr, void *, touch_value_t) __attribute__ ((weak, alias("__touchAttachArgsInterrupt")));
extern void touchDetachInterrupt(uint8_t) __attribute__ ((weak, alias("__touchDettachInterrupt")));
extern void touchSetCycles(uint16_t, uint16_t) __attribute__ ((weak, alias("__touchSetCycles")));

#endif /* SOC_TOUCH_SENSOR_SUPPORTED */
