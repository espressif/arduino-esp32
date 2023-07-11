/* UART test */
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
 * For 1 UART:
 *
 *          terminal
 *         |       ^
 *         v UART0 |
 *         RX<--->TX
 *
 * For 2 UARTS:
 *
 *          terminal
 *         |       ^
 *         v UART0 |
 *         RX    TX
 *          ^
 *          |
 *         TX    RX
 *           UART1
 *
 * For 3 UARTS:
 *
 *          terminal
 *         |       ^
 *         v UART0 |
 *         RX /   TX
 *           /
 *          /
 *         /
 *     TX /________  RX
 *  UART2 RX      TX UART1
 *
 */

/* setUp / tearDown functions are intended to be called before / after each test. */
void setUp(void) {
}

void tearDown(void){
}

void uart_test_01(void){
  log_d("Send string from UART0 to terminal");
  Serial.println("Hello from Serial (UART0) >>>  to >>> terminal");
  Serial.flush();
  delay(2500);
  #if SOC_UART_NUM == 2
    log_d("Send string from UART1 to UART0");
    Serial1.println("Hello from Serial1 (UART1) >>>  to >>> Serial (UART0)");
    Serial1.flush();
  #endif
  #if SOC_UART_NUM == 3
    log_d("Send string from UART1 to UART2");
    Serial1.println("Hello from Serial1 (UART1) >>>  to >>> Serial2 (UART2)");
    Serial1.flush();
    delay(2500);
    log_d("Send string from UART2 to UART0");
    Serial2.println("Hello from Serial2 (UART2) >>>  to >>> Serial0 (UART0)");
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

void onReceiveError(hardwareSerial_error_t err){
  // This is a callback function that will be activated on UART RX events
  log_d("Function triggered with code onReceiveError [ERR#%d:%s] \n", err, uartErrorStrings[err]);
}

void onReceiveFunction0() {
  // This is a callback function that will be activated on UART RX events
  size_t available = Serial.available();
  log_d("Function triggered with %d B", available);
  Serial.printf("onReceive Callback0:: There are %d bytes available: {", available);
  while (available --) {
    Serial.print((char)Serial.read());
  }
  Serial.println("}");
  Serial.flush();
}

#if SOC_UART_NUM > 1
void onReceiveFunction1() {
  // This is a callback function that will be activated on UART RX events
  size_t available = Serial1.available();
  log_d("Function triggered with %d B", available);
  Serial1.printf("onReceive Callback1:: There are %d bytes available: {", available);
  while (available --) {
    Serial1.print((char)Serial1.read());
  }
  Serial1.println("}");
  Serial1.flush();
}
#endif

#if SOC_UART_NUM > 2
void onReceiveFunction2() {
  // This is a callback function that will be activated on UART RX events
  size_t available = Serial2.available();
  log_d("Function triggered with %d B", available);
  Serial2.printf("onReceive Callback2:: There are %d bytes available: {", available);
  while (available --) {
    Serial2.print((char)Serial2.read());
  }
  Serial2.println("}");
  Serial2.flush();
}
#endif

void setup(){
  Serial.begin(115200);
  while(!Serial){ delay(1); }
  log_d("SOC_UART_NUM=%d", SOC_UART_NUM);

  #if SOC_UART_NUM > 1
    Serial1.begin(115200);
    while(!Serial1){ delay(1); }
  #endif

  #if SOC_UART_NUM > 2
    Serial2.begin(115200);
    while(!Serial2){ delay(1); }
  #endif

  Serial.onReceive(onReceiveFunction0);
  Serial.onReceiveError(onReceiveError);

#if SOC_UART_NUM == 1
  // Setup loopback on UART0 (U0_TX -> U0_RX)
  // UART0 TX -> UART1 RX
  uart_internal_loopback(0, RX1);
#endif

#if SOC_UART_NUM == 2
  Serial1.onReceive(onReceiveFunction1);
  Serial1.onReceiveError(onReceiveError);
  log_d("Setup internal loop-back from Serial1 (UART1) >> Serial (UART0)");
  // Setup internal loop-back from Serial1 (UART1) >> Serial (UART0)
  // UART0 TX -> UART1 RX
  //uart_internal_loopback(0, RX1);

  //UART1 TX -> UART0 RX
  uart_internal_loopback(1, SOC_RX0);
#endif

#if SOC_UART_NUM == 3
  log_d("Setup internal loop-back from Serial1 (UART1) >> Serial2 (UART2) >> Serial (UART0)");
  // Setup internal loop-back from Serial1 (UART1) >> Serial2 (UART2) >> Serial (UART0)
  Serial1.onReceive(onReceiveFunction1);
  Serial2.onReceive(onReceiveFunction2);

   Serial1.onReceiveError(onReceiveError);
   Serial2.onReceiveError(onReceiveError);
  // UART0 TX -> UART1 RX
  //uart_internal_loopback(0, RX1);

  // UART1 TX -> UART2 RX
  //uart_internal_loopback(1, RX2);
  esp_rom_gpio_connect_out_signal(RX2, U1TXD_OUT_IDX, false, false);
  esp_rom_gpio_connect_in_signal(RX2, U2RXD_IN_IDX, false);

  // UART2 TX -> UART0 RX
  //uart_internal_loopback(2, SOC_RX0);  // this does not work! :(
  //esp_rom_gpio_connect_out_signal(SOC_RX0, U2TXD_OUT_IDX, false, false); // this does not work! :(
  //esp_rom_gpio_connect_in_signal(SOC_RX0, U0RXD_IN_IDX, false);

  //esp_rom_gpio_connect_out_signal(TX2, U2TXD_OUT_IDX, false, false); // this does not work! :(
  //esp_rom_gpio_connect_in_signal(TX2, U0RXD_IN_IDX, false);
  // ???
#endif

  UNITY_BEGIN();
  RUN_TEST(uart_test_01);
  //RUN_TEST(uart_test_02);
  //RUN_TEST(uart_test_03);
  UNITY_END();
}

void loop(){
}