/* HW Timer test */
#include <unity.h>

#define TIMER_DIVIDER  16
#define TIMER_SCALE    (APB_CLK_FREQ / TIMER_DIVIDER)

hw_timer_t * timer = NULL;
static volatile bool alarm_flag;

/* These functions are intended to be called before and after each test. */
void setUp(void) {
  timer = timerBegin(0, TIMER_DIVIDER, true);
  timerStop(timer);
  timerRestart(timer);
}

void tearDown(void){
  timerEnd(timer);
}



void ARDUINO_ISR_ATTR onTimer(){
  alarm_flag = true;
}


void timer_interrupt_test(void){
  
  alarm_flag = false;
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, (1.2 * TIMER_SCALE), true);
  timerAlarmEnable(timer);
  timerStart(timer);
  
  delay(2000);

  TEST_ASSERT_EQUAL(true, alarm_flag);

  timerStop(timer);
  timerRestart(timer);
  alarm_flag = false;
  timerAlarmDisable(timer);
  timerStart(timer);

  delay(2000);
  TEST_ASSERT_EQUAL(false, alarm_flag);
}

void timer_divider_test(void){

  uint64_t time_val;
  uint64_t comp_time_val;

  timerStart(timer);

  delay(1000);
  time_val = timerRead(timer);

  // compare divider  16 and 8, value should be double
  timerStop(timer);
  timerSetDivider(timer,8);
  timerRestart(timer);
  timerStart(timer);

  delay(1000);        
  comp_time_val = timerRead(timer);
    
  TEST_ASSERT_INT_WITHIN(5000, 5000000, time_val);
  TEST_ASSERT_INT_WITHIN(10000, 10000000, comp_time_val);

  // divider is 256, value should be 2^4
  timerStop(timer);
  timerSetDivider(timer,256);
  timerRestart(timer);
  timerStart(timer);
  delay(1000);       
  comp_time_val = timerRead(timer);
  TEST_ASSERT_INT_WITHIN(5000, 5000000, time_val);
  TEST_ASSERT_INT_WITHIN(3126, 312500, comp_time_val);
}

void timer_read_test(void){

  uint64_t set_timer_val = 0xFF;
  uint64_t get_timer_val = 0;

  timerWrite(timer,set_timer_val);
  get_timer_val = timerRead(timer);

  TEST_ASSERT_EQUAL(set_timer_val, get_timer_val);
}

void setup(){
  
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  UNITY_BEGIN();
  RUN_TEST(timer_read_test);
  RUN_TEST(timer_interrupt_test);
  RUN_TEST(timer_divider_test);
  UNITY_END();
}

void loop(){
}
