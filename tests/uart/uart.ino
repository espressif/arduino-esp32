/* UART test
 * This test is using UART0 (Serial) only for reporting test status and is not part of the test.
 * UART1 (Serial1) and UART2 (Serial2) where available are used for testing with loopback.
 */
#include <unity.h>
#include "HardwareSerial.h"
#include "esp_rom_gpio.h"

// Default pins:
//          |  name   | ESP32 | C3 | S2 | S3 |
// UART0 TX | SOC_TX0 |   1   | 21 | 43 | 43 |
// UART0 RX | SOC_RX0 |   3   | 20 | 44 | 44 |
// UART1 TX | SOC_TX1 |  10   | 19 | 17 | 16 |
// UART1 RX | SOC_RX1 |   9   | 18 | 18 | 15 |
// UART2 TX | SOC_TX2 |  17   | -- | -- | 20 |
// UART2 RX | SOC_RX3 |  16   | -- | -- | 19 |

/*
 * General notes and plan for the test:
 * do not enable CDC on boot!
 * make sure all the tests print something for the pytest to check the state
 * Add tests for API - test all functions
 * Try weird usage - send twice end, or begin - make sure it doesn't break.
 * begin with some baudrate, send something, then change the bad rate to make sure it is changing
 * test changing size of the buffers (RX, TX)
 * on UART pins try use them for something else (make sure they don't conflict with flash etc) (for example LEDC, PWM, I2C) now using UART should not break. after unasigning this the UART shoudl return back to functioning state
 * compile for various cpu frequency
 * After Serial.end() all other functions should do nothing and return "fail"
 * Test all UARTs (some SoCs have 3, some only 2)
 *
 * Steps:
 * 1. check idf testing folder - just take a look, get inspired
 * 2. use function uart_internal_loopback from cores/esp32/esp32-hal-uart.c
 * 3. Use examples libraries/ESP32.examples/Serial
 * 4. With existing examples extend to test various speeds
 *
 * Done:
 * move definition of constants "SOC_TX0" from HardwareSerial.cpp to the .h
 *
 * For 2 UARTS:
 *
 *           terminal
 *          |       ^
 *          v UART0 |
 *          RX  ^  TX
 *              |
 *        report status
 *              |
 *         TX <---> RX
 *            UART1
 *
 * For 3 UARTS:
 *
 *     =====terminal======
 *      ^  |       ^    ^
 *      |  v UART0 |    |
 *      |  RX    TX     |
 *      |               |
 *      ^ report status ^
 *      |               |
 *      | TX ---> RX    |
 *  UART2 RX <--- TX UART1
 *
 */

/* setUp / tearDown functions are intended to be called before / after each test. */
void setUp(void) {
}

void tearDown(void){
}

void uart_test_01(void){
  #if SOC_UART_NUM == 2
    log_d("Send string in loopback from and to UART1");
    Serial1.print("Hello from Serial1 (UART1) >>>  via loopback >>> Serial1 (UART1)");
    Serial1.flush();
  #endif
  #if SOC_UART_NUM == 3
    log_d("Send string from UART1 to UART2");
    Serial1.print("Hello from Serial1 (UART1) >>>  to >>> Serial2 (UART2)");
    Serial1.flush();
    delay(100);
    log_d("Send string from UART2 to UART0");
    Serial2.print("Hello from Serial2 (UART2) >>>  to >>> Serial1 (UART1)");
    Serial2.flush();
  #endif
}

void uart_test_02(void){
  //TODO
}

void uart_test_03(void){
  //TODO
}


const char *uartErrorStrings[] = {
  "UART_NO_ERROR",
  "UART_BREAK_ERROR",
  "UART_BUFFER_FULL_ERROR",
  "UART_FIFO_OVF_ERROR",
  "UART_FRAME_ERROR",
  "UART_PARITY_ERROR"
};

void onReceiveError1(hardwareSerial_error_t err){
  // This is a callback function that will be activated on UART RX events
  log_d("Function triggered with code onReceiveError [ERR#%d:%s] \n", err, uartErrorStrings[err]);
}

void onReceiveError2(hardwareSerial_error_t err){
  // This is a callback function that will be activated on UART RX events
  log_d("Function triggered with code onReceiveError [ERR#%d:%s] \n", err, uartErrorStrings[err]);
}

// This is a callback function that will be activated on UART RX events
void onReceiveFunction(HardwareSerial &mySerial){
  int8_t uart_num = -1;
  if (&mySerial == &Serial) {
    uart_num = 0;
#if SOC_UART_NUM > 1
  } else if (&mySerial == &Serial1) {
    uart_num = 1;
#endif
#if SOC_UART_NUM > 2
  } else if (&mySerial == &Serial2) {
    uart_num = 2;
#endif
  }

  size_t available = mySerial.available();
  Serial.printf("UART %d: onReceiveFunction: There are %d bytes available:\n", uart_num, available);
  char c;
  int i = 0;
  while (available --) {
    c = (char)mySerial.read();
    Serial.printf("[%d] '%c' = 0x%1x = %d\n", i++, c, c, c);
  }
  Serial.println("}");
  Serial.flush();
}

void setup(){
  Serial.begin(115200);
  while(!Serial){ delay(10); }
  log_d("SOC_UART_NUM=%d", SOC_UART_NUM);

  #if SOC_UART_NUM > 1
    log_d("Setup Serial 1");
    Serial1.begin(115200);
    while(!Serial1){ Serial.print("."); delay(100); }
    Serial.println("");
  #endif

  #if SOC_UART_NUM > 2
    log_d("Setup Serial 2");
    Serial2.begin(115200);
    while(!Serial2){ delay(10); }
  #endif

#if SOC_UART_NUM == 2
  log_d("Setup internal loop-back from and back to Serial1 (UART1) TX >> Serial1 (UART1) RX");
  Serial1.onReceive([]() {onReceiveFunction(Serial1);});
  Serial1.onReceiveError(onReceiveError1);

  //UART1 TX -> UART0 RX
  uart_internal_loopback(1, SOC_RX0);
#endif

#if SOC_UART_NUM == 3
  log_d("Setup internal loop-back between Serial1 (UART1) <<--->> Serial2 (UART2)");
  Serial1.onReceive([]() {onReceiveFunction(Serial1);});
  Serial2.onReceive([]() {onReceiveFunction(Serial2);});

  Serial1.onReceiveError(onReceiveError1);
  Serial2.onReceiveError(onReceiveError2);
  uart_internal_loopback(1, RX2);
  uart_internal_loopback(2, RX1);
#endif

  log_d("Setup done, start tests");

  UNITY_BEGIN();
  RUN_TEST(uart_test_01);
  //RUN_TEST(uart_test_02);
  //RUN_TEST(uart_test_03);
  UNITY_END();
}

void loop(){
}