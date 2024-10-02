// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @brief This example demonstrates simple Zigbee light switch.
 *
 * The example demonstrates how to use Zigbee library to control a light bulb.
 * The light bulb is a Zigbee end device, which is controlled by a Zigbee coordinator (Switch).
 * Button switch and Zigbee runs in separate tasks.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include "ZigbeeCore.h"
#include "ep/ZigbeeSwitch.h"

#define SWITCH_ENDPOINT_NUMBER 5

/* Switch configuration */
#define GPIO_INPUT_IO_TOGGLE_SWITCH 9
#define PAIR_SIZE(TYPE_STR_PAIR)    (sizeof(TYPE_STR_PAIR) / sizeof(TYPE_STR_PAIR[0]))

typedef enum {
  SWITCH_ON_CONTROL,
  SWITCH_OFF_CONTROL,
  SWITCH_ONOFF_TOGGLE_CONTROL,
  SWITCH_LEVEL_UP_CONTROL,
  SWITCH_LEVEL_DOWN_CONTROL,
  SWITCH_LEVEL_CYCLE_CONTROL,
  SWITCH_COLOR_CONTROL,
} SwitchFunction;

typedef struct {
  uint8_t pin;
  SwitchFunction func;
} SwitchData;

typedef enum {
  SWITCH_IDLE,
  SWITCH_PRESS_ARMED,
  SWITCH_PRESS_DETECTED,
  SWITCH_PRESSED,
  SWITCH_RELEASE_DETECTED,
} SwitchState;

static SwitchData buttonFunctionPair[] = {{GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL}};

ZigbeeSwitch zbSwitch = ZigbeeSwitch(SWITCH_ENDPOINT_NUMBER);

/********************* Zigbee functions **************************/
static void onZbButton(SwitchData *button_func_pair) {
  if (button_func_pair->func == SWITCH_ONOFF_TOGGLE_CONTROL) {
    // Send toggle command to the light
    zbSwitch.lightToggle();
  }
}

/********************* GPIO functions **************************/
static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR onGpioInterrupt(void *arg) {
  xQueueSendFromISR(gpio_evt_queue, (SwitchData *)arg, NULL);
}

static void enableGpioInterrupt(bool enabled) {
  for (int i = 0; i < PAIR_SIZE(buttonFunctionPair); ++i) {
    if (enabled) {
      enableInterrupt((buttonFunctionPair[i]).pin);
    } else {
      disableInterrupt((buttonFunctionPair[i]).pin);
    }
  }
}

/********************* Arduino functions **************************/
void setup() {

  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  //Optional: set Zigbee device name and model
  zbSwitch.setManufacturerAndModel("Espressif", "ZigbeeSwitch");

  //Optional to allow multiple light to bind to the switch
  zbSwitch.allowMultipleBinding(true);

  //Add endpoint to Zigbee Core
  log_d("Adding ZigbeeSwitch endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbSwitch);

  //Open network for 180 seconds after boot
  Zigbee.setRebootOpenNetwork(180);

  // Init button switch
  for (int i = 0; i < PAIR_SIZE(buttonFunctionPair); i++) {
    pinMode(buttonFunctionPair[i].pin, INPUT_PULLUP);
    /* create a queue to handle gpio event from isr */
    gpio_evt_queue = xQueueCreate(10, sizeof(SwitchData));
    if (gpio_evt_queue == 0) {
      log_e("Queue was not created and must not be used");
      while (1);
    }
    attachInterruptArg(buttonFunctionPair[i].pin, onGpioInterrupt, (void *)(buttonFunctionPair + i), FALLING);
  }

  // When all EPs are registered, start Zigbee with ZIGBEE_COORDINATOR mode
  log_d("Calling Zigbee.begin()");
  Zigbee.begin(ZIGBEE_COORDINATOR);

  Serial.println("Waiting for Light to bound to the switch");
  //Wait for switch to bound to a light:
  while (!zbSwitch.isBound()) {
    Serial.printf(".");
    delay(500);
  }

  // Optional: read manufacturer and model name from the bound light
  std::list<zb_device_params_t *> boundLights = zbSwitch.getBoundDevices();
  //List all bound lights
  for (const auto &device : boundLights) {
    Serial.printf("Device on endpoint %d, short address: 0x%x\n", device->endpoint, device->short_addr);
    Serial.printf(
      "IEEE Address: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n", device->ieee_addr[0], device->ieee_addr[1], device->ieee_addr[2], device->ieee_addr[3],
      device->ieee_addr[4], device->ieee_addr[5], device->ieee_addr[6], device->ieee_addr[7]
    );
    Serial.printf("Light manufacturer: %s", zbSwitch.readManufacturer(device->endpoint, device->short_addr));
    Serial.printf("Light model: %s", zbSwitch.readModel(device->endpoint, device->short_addr));
  }

  Serial.println();
}

void loop() {
  // Handle button switch in loop()
  uint8_t pin = 0;
  SwitchData buttonSwitch;
  static SwitchState buttonState = SWITCH_IDLE;
  bool eventFlag = false;

  /* check if there is any queue received, if yes read out the buttonSwitch */
  if (xQueueReceive(gpio_evt_queue, &buttonSwitch, portMAX_DELAY)) {
    pin = buttonSwitch.pin;
    enableGpioInterrupt(false);
    eventFlag = true;
  }
  while (eventFlag) {
    bool value = digitalRead(pin);
    switch (buttonState) {
      case SWITCH_IDLE:           buttonState = (value == LOW) ? SWITCH_PRESS_DETECTED : SWITCH_IDLE; break;
      case SWITCH_PRESS_DETECTED: buttonState = (value == LOW) ? SWITCH_PRESS_DETECTED : SWITCH_RELEASE_DETECTED; break;
      case SWITCH_RELEASE_DETECTED:
        buttonState = SWITCH_IDLE;
        /* callback to button_handler */
        (*onZbButton)(&buttonSwitch);
        break;
      default: break;
    }
    if (buttonState == SWITCH_IDLE) {
      enableGpioInterrupt(true);
      eventFlag = false;
      break;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  // print the bound lights every 10 seconds
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 10000) {
    lastPrint = millis();
    zbSwitch.printBoundDevices();
  }
}
