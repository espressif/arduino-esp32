/*
  This example smothly blinks GPIO_2 using different frequencies changed after Deep Sleep Time
  The PWM and control of blink frequency is done by ULP exclusively
  This is an example about how to program the ULP using Arduino
  It also demonstrates use of RTM MEMORY to persist data and states
*/

#include <Arduino.h>
#include "esp32/ulp.h"
#include "driver/rtc_io.h"

// RTC Memory used for ULP internal variable and Sketch interfacing
#define RTC_dutyMeter 0
#define RTC_dir       4
#define RTC_fadeDelay 12
// *fadeCycleDelay is used to pass values to ULP and change its behaviour
uint32_t *fadeCycleDelay = &RTC_SLOW_MEM[RTC_fadeDelay];
#define ULP_START_OFFSET 32

// For ESP32 Arduino, it is usually at offeset 512, defined in sdkconfig
RTC_DATA_ATTR uint32_t ULP_Started = 0; // 0 or 1

//Time-to-Sleep
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5           /* Time ESP32 will go to sleep (in microseconds); multiplied by above conversion to achieve seconds*/


void ulp_setup() {
  if (ULP_Started) {
    return;
  }
  *fadeCycleDelay = 5; // 5..200 works fine for a full Fade In + Out cycle
  ULP_Started = 1;

  // GPIO2 initialization (set to output and initial value is 0)
  const gpio_num_t MeterPWMPin = GPIO_NUM_2;
  rtc_gpio_init(MeterPWMPin);
  rtc_gpio_set_direction(MeterPWMPin, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_set_level(MeterPWMPin, 0);

  // if LED is connected to GPIO2 (specify by +RTC_GPIO_OUT_DATA_S : ESP32 is 14, S2/S3 is 10)
  const uint32_t MeterPWMBit = rtc_io_number_get(MeterPWMPin) + RTC_GPIO_OUT_DATA_S;

  enum labels {
    INIFINITE_LOOP,
    RUN_PWM,
    NEXT_PWM_CYCLE,
    PWM_ON,
    PWM_OFF,
    END_PWM_CYCLE,
    POSITIVE_DIR,
    DEC_DUTY,
    INC_DUTY,
  };
  
  // Define ULP program
  const ulp_insn_t ulp_prog[] = {
    // Initial Value setup
    I_MOVI(R0, 0),                // R0 = 0
    I_ST(R0, R0, RTC_dutyMeter),  // RTC_SLOW_MEM[RTC_dutyMeter] = 0
    I_MOVI(R1, 1),                // R1 = 1
    I_ST(R1, R0, RTC_dir),        // RTC_SLOW_MEM[RTC_dir] = 1

    M_LABEL(INIFINITE_LOOP),      // while(1) {

    // run certain PWM Duty for about (RTC_fadeDelay x 100) microseconds
    I_MOVI(R3, 0),                //   R3 = 0
    I_LD(R3, R3, RTC_fadeDelay),  //   R3 = RTC_SLOW_MEM[RTC_fadeDelay]
    M_LABEL(RUN_PWM),             //   do {  // repeat RTC_fadeDelay times:

    // execute about 10KHz PWM on GPIO2 using as duty cycle = RTC_SLOW_MEM[RTC_dutyMeter]
    I_MOVI(R0, 0),                //     R0 = 0
    I_LD(R0, R0, RTC_dutyMeter),  //     R0 = RTC_SLOW_MEM[RTC_dutyMeter]
    M_BL(NEXT_PWM_CYCLE, 1),      //     if (R0 > 0) turn on LED
    I_WR_REG(RTC_GPIO_OUT_W1TS_REG, MeterPWMBit, MeterPWMBit, 1), // W1TS set bit to clear GPIO - GPIO2 on
    M_LABEL(PWM_ON),              //     while (R0 > 0) // repeat RTC_dutyMeter times:
    M_BL(NEXT_PWM_CYCLE, 1),      //     {
    //I_DELAY(8),                 //       // 8 is about 1 microsecond based on 8MHz
    I_SUBI(R0, R0, 1),            //       R0 = R0 - 1
    M_BX(PWM_ON),                 //     }
    M_LABEL(NEXT_PWM_CYCLE),      //     // toggle GPIO_2
    I_MOVI(R0, 0),                //     R0 = 0
    I_LD(R0, R0, RTC_dutyMeter),  //     R0 = RTC_SLOW_MEM[RTC_dutyMeter]
    I_MOVI(R1, 100),              //     R1 = 100
    I_SUBR(R0, R1, R0),           //     R0 = 100 - dutyMeter
    M_BL(END_PWM_CYCLE, 1),       //     if (R0 > 0) turn off LED
    I_WR_REG(RTC_GPIO_OUT_W1TC_REG, MeterPWMBit, MeterPWMBit, 1), // W1TC set bit to clear GPIO - GPIO2 off
    M_LABEL(PWM_OFF),             //     while (R0 > 0)  // repeat (100 - RTC_dutyMeter) times:
    M_BL(END_PWM_CYCLE, 1),       //     {
    //I_DELAY(8),                 //       // 8 is about 1us: ULP fetch+execution time
    I_SUBI(R0, R0, 1),            //       R0 = R0 - 1
    M_BX(PWM_OFF),                //     }
    M_LABEL(END_PWM_CYCLE),       //

    I_SUBI(R3, R3, 1),            //     R3 = R3 - 1  // RTC_fadeDelay
    I_MOVR(R0, R3),               //     R0 = R3      // only R0 can be used to compare and branch
    M_BGE(RUN_PWM, 1),            //   } while (R3 > 0)  // ESP32 repeatinf RTC_fadeDelay times

    // increase/decrease DutyMeter to apply Fade In/Out loop
    I_MOVI(R1, 0),                //   R1 = 0
    I_LD(R1, R1, RTC_dutyMeter),  //   R1 = RTC_SLOW_MEM[RTC_dutyMeter]
    I_MOVI(R0, 0),                //   R0 = 0
    I_LD(R0, R0, RTC_dir),        //   R0 = RTC_SLOW_MEM[RTC_dir]

    M_BGE(POSITIVE_DIR, 1),                  //   if(dir == 0) { // decrease duty by 2
    // Dir is 0, means decrease Duty by 2
    I_MOVR(R0, R1),               //     R0 = Duty
    M_BGE(DEC_DUTY, 1),                  //     if (duty == 0) { // change direction and increase duty
    I_MOVI(R3, 0),                //       R3 = 0
    I_MOVI(R2, 1),                //       R2 = 1
    I_ST(R2, R3, RTC_dir),        //       RTC_SLOW_MEM[RTC_dir] = 1   // increasing direction
    M_BX(INC_DUTY),                      //       goto "increase Duty"
    M_LABEL(DEC_DUTY),                   //     } "decrease Duty":
    I_SUBI(R0, R0, 2),            //     Duty -= 2
    I_MOVI(R2, 0),                //     R2 = 0
    I_ST(R0, R2, RTC_dutyMeter),  //     RTC_SLOW_MEM[RTC_dutyMeter] += 2
    M_BX(INIFINITE_LOOP),         // }

    M_LABEL(POSITIVE_DIR),        //   else { // dir == 1 // increase duty by 2
    // Dir is 1, means increase Duty by 2
    I_MOVR(R0, R1),               //     R0 = Duty
    M_BL(INC_DUTY, 100),          //     if (duty == 100) { // change direction and decrease duty
    I_MOVI(R2, 0),                //       R2 = 0
    I_ST(R2, R2, RTC_dir),        //       RTC_SLOW_MEM[RTC_dir] = 0  // decreasing direction
    M_BX(DEC_DUTY),               //       goto "decrease Duty"
    M_LABEL(INC_DUTY),            //     } "increase Duty":
    I_ADDI(R0, R0, 2),            //     Duty += 2
    I_MOVI(R2, 0),                //     R2 = 0
    I_ST(R0, R2, RTC_dutyMeter),  //     RTC_SLOW_MEM[RTC_dutyMeter] -= 2
                                  //   } // if (dir == 0)
    M_BX(INIFINITE_LOOP),         // } // while(1)
  };
  // Run ULP program
  size_t size = sizeof(ulp_prog) / sizeof(ulp_insn_t);
  ulp_process_macros_and_load(ULP_START_OFFSET, ulp_prog, &size);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  ulp_run(ULP_START_OFFSET);
}


void setup() {
  Serial.begin(115200);
  while (!Serial) {}  // wait for Serial to start

  ulp_setup(); // it really only runs on the first ESP32 boot
  Serial.printf("\nStarted smooth blink with delay %d\n", *fadeCycleDelay);

  // *fadeCycleDelay resides in RTC_SLOW_MEM and persists along deep sleep waking up
  // it is used as a delay time parameter for smooth blinking, in the ULP processing code
  if (*fadeCycleDelay < 195) {
    *fadeCycleDelay += 10;
  } else {
    *fadeCycleDelay = 5; // 5..200 works fine for a full Fade In + Out cycle
  }
  Serial.println("Entering in Deep Sleep");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR /*/ 4*/); // time set with variable above
  esp_deep_sleep_start();
  // From this point on, no code is executed in DEEP SLEEP mode
}


void loop() {
  // It never reaches this code because it enters in Deep Sleep mode at the end of setup()
}

