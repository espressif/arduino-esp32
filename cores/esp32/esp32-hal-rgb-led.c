#include "esp32-hal-rgb-led.h"


void neopixelWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val){
  rmt_data_t led_data[24];

  bool shallAlwaysInitRMT = true;  // in case it is used for any other GPIO
#ifdef RGB_BUILTIN
  static bool initialized = false;
  if(pin == RGB_BUILTIN){
    pin = RGB_BUILTIN - SOC_GPIO_PIN_COUNT;
    if(!initialized) {
      initialized = true;
    } else {
      // RGB_BUILTIN has been already initialized before, save this time now
      shallAlwaysInitRMT = false;
    }
  }
#endif

  if (shallAlwaysInitRMT) {
    // 10MHz RMT Frequency -> tick = 100ns
    // force initialization to make sure it has the right RMT Frequency
    if (!rmtInit(pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)){
      log_e("RGB LED driver initialization failed!");
      return;
    }
  }
  int color[] = {green_val, red_val, blue_val};  // Color coding is in order GREEN, RED, BLUE
  int i = 0;
  for(int col=0; col<3; col++ ){
    for(int bit=0; bit<8; bit++){
      if((color[col] & (1<<(7-bit)))){
        // HIGH bit
        led_data[i].level0 = 1; // T1H
        led_data[i].duration0 = 8; // 0.8us
        led_data[i].level1 = 0; // T1L
        led_data[i].duration1 = 4; // 0.4us
      }else{
        // LOW bit
        led_data[i].level0 = 1; // T0H
        led_data[i].duration0 = 4; // 0.4us
        led_data[i].level1 = 0; // T0L
        led_data[i].duration1 = 8; // 0.8us
      }
      i++;
    }
  }
  rmtWrite(pin, led_data, 24, false, false);
}
