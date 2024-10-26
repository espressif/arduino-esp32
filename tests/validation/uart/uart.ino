/* UART test

   This test is using UART0 (Serial) only for reporting test status and helping with the auto
   baudrate detection test.
   UART1 (Serial1) is used for testing.
*/

#include <unity.h>
#include "HardwareSerial.h"
#include "esp_rom_gpio.h"
#include "Wire.h"

// Default pins:
//          |  Name   | ESP32 | S2 | S3 | C3 | C6 | H2 | P4 |
// UART0 RX | SOC_RX0 |   3   | 44 | 44 | 20 | 17 | 23 | 38 |
// UART0 TX | SOC_TX0 |   1   | 43 | 43 | 21 | 16 | 24 | 37 |
// UART1 RX |   RX1   |  26   |  4 | 15 | 18 |  4 |  0 | 11 |
// UART1 TX |   TX1   |  27   |  5 | 16 | 19 |  5 |  1 | 10 |

/*
   Using 2 UARTS:

             terminal
            |       ^
            v UART0 |
            RX  ^  TX
                |
          report status
                |
           TX <---> RX
              UART1
*/

// Used for the pin swap test
#define NEW_RX1 10
#define NEW_TX1 11

/* Utility global variables */
static String recv_msg = "";
static int peeked_char = -1;

/* Utility functions */

extern int8_t uart_get_RxPin(uint8_t uart_num);
extern int8_t uart_get_TxPin(uint8_t uart_num);

// This function starts all the available test UARTs
void start_serial1(unsigned long baudrate = 115200) {
  Serial1.begin(baudrate);
  while (!Serial1) {
    delay(10);
  }
}

// This function stops all the available test UARTs
void stop_serial() {
  Serial1.end();
}

// This function transmits a message and checks if it was received correctly
void transmit_and_check_msg(const String msg_append, bool perform_assert = true) {
  delay(100);  // Wait for some settings changes to take effect
  Serial1.print("Hello from Serial1 (UART1) >>> via loopback >>> Serial1 (UART1) " + msg_append);
  Serial1.flush();
  delay(100);
  if (perform_assert) {
    TEST_ASSERT_EQUAL_STRING(("Hello from Serial1 (UART1) >>> via loopback >>> Serial1 (UART1) " + msg_append).c_str(), recv_msg.c_str());
  }
}

/* Unity functions */

// This function is automatically called by unity before each test is run
void setUp(void) {
  start_serial1(115200);
  log_d("Setup internal loop-back from and back to Serial1 (UART1) TX >> Serial1 (UART1) RX");
  Serial1.onReceive([]() {
    onReceive_cb(Serial1, 1);
  });
  uart_internal_loopback(1, true);
}

// This function is automatically called by unity after each test is run
void tearDown(void) {
  stop_serial();
}

/* Callback functions */

// This is a callback function that will be activated on UART RX events
void onReceive_cb(HardwareSerial &HWSerial, uint8_t uartNum) {
  char c;

  recv_msg = "";
  size_t available = HWSerial.available();
  if (available != 0) {
    peeked_char = HWSerial.peek();
  }

  while (available--) {
    c = (char)HWSerial.read();
    recv_msg += c;
  }

  log_d("UART%d received message: %s\n", uartNum, recv_msg.c_str());
}

/* Test functions */

// This test checks if a message can be transmitted and received correctly using the default settings
void basic_transmission_test() {
  log_d("Performing basic transmission test");
  transmit_and_check_msg("");
  Serial.println("Basic transmission test successful");
}

// This test checks if the baudrate can be changed and if the message can be transmitted and received correctly after the change
void change_baudrate_test() {
  //Test first using the updateBaudRate method and then using the begin method
  log_d("Changing baudrate to 9600");

  //Baudrate error should be within 2% of the target baudrate
  Serial1.updateBaudRate(9600);
  TEST_ASSERT_UINT_WITHIN(192, 9600, Serial1.baudRate());

  log_d("Sending string using 9600 baudrate");
  transmit_and_check_msg("using 9600 baudrate");

  log_d("Changing baudrate back to 115200");
  start_serial1(115200);

  //Baudrate error should be within 2% of the target baudrate
  TEST_ASSERT_UINT_WITHIN(2304, 115200, Serial1.baudRate());

  log_d("Sending string using 115200 baudrate");
  transmit_and_check_msg("using 115200 baudrate");

  Serial.println("Change baudrate test successful");
}

// This test checks if the buffers can be resized properly
void resize_buffers_test() {
  size_t ret;

  log_d("Trying to resize RX buffer while running.");
  ret = Serial1.setRxBufferSize(256);
  TEST_ASSERT_EQUAL(0, ret);

  log_d("Trying to resize TX buffer while running.");
  ret = Serial1.setTxBufferSize(256);
  TEST_ASSERT_EQUAL(0, ret);

  stop_serial();

  log_d("Trying to resize RX buffer while stopped.");
  ret = Serial1.setRxBufferSize(256);
  TEST_ASSERT_EQUAL(256, ret);

  log_d("Trying to resize TX buffer while stopped.");
  ret = Serial1.setTxBufferSize(256);
  TEST_ASSERT_EQUAL(256, ret);

  Serial.println("Buffer resize test successful");
}

// This test checks if the begin function can be called when the UART is already running
void begin_when_running_test() {
  log_d("Trying to set up serial twice");
  start_serial1(115200);
  start_serial1(115200);
  // starting other Serial ports as part of the test
#if SOC_UART_HP_NUM > 2
  Serial2.begin(115200, SERIAL_8N1, RX1, TX1);
  Serial2.begin(115200);
#endif
#if SOC_UART_HP_NUM > 3
  Serial3.begin(115200, SERIAL_8N1, RX1, TX1);
  Serial3.begin(115200);
#endif
#if SOC_UART_HP_NUM > 4
  Serial4.begin(115200, SERIAL_8N1, RX1, TX1);
  Serial4.begin(115200);
#endif
  Serial.println("Begin when running test successful");
}

// This test checks if the end function can be called when the UART is already stopped
void end_when_stopped_test() {
  log_d("Trying to end serial twice");

  // Calling end(true) twice should not crash
  stop_serial();
  stop_serial();

  Serial.println("End when stopped test successful");
}

// This test checks if all the UART methods work when the UART is running
void enabled_uart_calls_test() {
  bool boolean_ret;
  long int integer_ret;
  uint8_t test_buf[1];

  log_d("Checking if Serial 1 can set the RX timeout while running");
  boolean_ret = Serial1.setRxTimeout(1);
  TEST_ASSERT_EQUAL(true, boolean_ret);

  log_d("Checking if Serial 1 can set the RX FIFO full interrupt threshold while running");
  boolean_ret = Serial1.setRxFIFOFull(120);
  TEST_ASSERT_EQUAL(true, boolean_ret);

  log_d("Checking if Serial 1 is writable while running");
  boolean_ret = Serial1.availableForWrite();
  TEST_ASSERT_EQUAL(true, boolean_ret);

  log_d("Checking if Serial 1 is peekable while running");
  TEST_ASSERT_GREATER_OR_EQUAL(0, peeked_char);

  log_d("Checking if Serial 1 can read bytes while running");
  integer_ret = Serial1.readBytes(test_buf, 1);
  TEST_ASSERT_GREATER_OR_EQUAL(0, integer_ret);

  log_d("Checking if Serial 1 can set the flow control while running");
  boolean_ret = Serial1.setHwFlowCtrlMode(UART_HW_FLOWCTRL_DISABLE, 64);
  TEST_ASSERT_EQUAL(true, boolean_ret);

  log_d("Checking if Serial 1 can set the mode while running");
  boolean_ret = Serial1.setMode(UART_MODE_UART);
  TEST_ASSERT_EQUAL(true, boolean_ret);

  // Tests without return values. Just check for crashes.

  log_d("Checking if Serial 1 event queue can be reset while running");
  Serial1.eventQueueReset();

  log_d("Checking if Serial 1 debug output can be enabled while running");
  Serial1.setDebugOutput(true);
  Serial1.setDebugOutput(false);

  log_d("Checking if Serial 1 RX can be inverted while running");
  Serial1.setRxInvert(true);
  Serial1.setRxInvert(false);

  Serial.println("Enabled UART calls test successful");
}

// This test checks if all the UART methods work when the UART is stopped
void disabled_uart_calls_test() {
  bool boolean_ret;
  int integer_ret;
  uint8_t test_buf[1];

  stop_serial();

  log_d("Checking if Serial 1 can set the RX timeout when stopped");
  boolean_ret = Serial1.setRxTimeout(1);
  TEST_ASSERT_EQUAL(false, boolean_ret);

  log_d("Checking if Serial 1 can set the RX FIFO full interrupt threshold when stopped");
  boolean_ret = Serial1.setRxFIFOFull(128);
  TEST_ASSERT_EQUAL(false, boolean_ret);

  log_d("Checking if Serial 1 is available when stopped");
  boolean_ret = Serial1.available();
  TEST_ASSERT_EQUAL(false, boolean_ret);

  log_d("Checking if Serial 1 is writable when stopped");
  boolean_ret = Serial1.availableForWrite();
  TEST_ASSERT_EQUAL(false, boolean_ret);

  log_d("Checking if Serial 1 is peekable when stopped");
  integer_ret = Serial1.peek();
  TEST_ASSERT_EQUAL(-1, integer_ret);

  log_d("Checking if Serial 1 is readable when stopped");
  integer_ret = Serial1.read();
  TEST_ASSERT_EQUAL(-1, integer_ret);

  log_d("Checking if Serial 1 can read bytes when stopped");
  integer_ret = Serial1.readBytes(test_buf, 1);
  TEST_ASSERT_EQUAL(0, integer_ret);

  log_d("Checking if Serial 1 can retrieve the baudrate when stopped");
  integer_ret = Serial1.baudRate();
  TEST_ASSERT_EQUAL(0, integer_ret);

  log_d("Checking if Serial 1 can set the flow control when stopped");
  boolean_ret = Serial1.setHwFlowCtrlMode(UART_HW_FLOWCTRL_DISABLE, 64);
  TEST_ASSERT_EQUAL(false, boolean_ret);

  log_d("Checking if Serial 1 can set the mode when stopped");
  boolean_ret = Serial1.setMode(UART_MODE_UART);
  TEST_ASSERT_EQUAL(false, boolean_ret);

  log_d("Checking if Serial 1 set the baudrate when stopped");
  Serial1.updateBaudRate(9600);
  integer_ret = Serial1.baudRate();
  TEST_ASSERT_EQUAL(0, integer_ret);

  // Tests without return values. Just check for crashes.

  log_d("Checking if Serial 1 event queue can be reset when stopped");
  Serial1.eventQueueReset();

  log_d("Checking if Serial 1 can be flushed when stopped");
  Serial1.flush();

  log_d("Checking if Serial 1 debug output can be enabled when stopped");
  Serial1.setDebugOutput(true);
  Serial1.setDebugOutput(false);

  log_d("Checking if Serial 1 RX can be inverted when stopped");
  Serial1.setRxInvert(true);
  Serial1.setRxInvert(false);

  Serial.println("Disabled UART calls test successful");
}

// This test checks if the pins can be changed and if the message can be transmitted and received correctly after the change
void change_pins_test() {
  //stop_serial();

  log_d("Disabling UART loopback");
  uart_internal_loopback(0, false);

  log_d("Swapping UART pins");

  Serial1.setPins(NEW_RX1, NEW_TX1);
  TEST_ASSERT_EQUAL(NEW_RX1, uart_get_RxPin(1));
  TEST_ASSERT_EQUAL(NEW_TX1, uart_get_TxPin(1));

  start_serial1(115200);

  log_d("Re-enabling UART loopback");

  uart_internal_loopback(1, true);

  transmit_and_check_msg("using new pins");

  Serial.println("Change pins test successful");
}

// This test checks if the peripheral manager can properly manage UART pins
void periman_test() {
  log_d("Checking if peripheral manager can properly manage UART pins");

  log_d("Setting up I2C on the same pins as UART");

  Wire.begin(RX1, TX1);

  recv_msg = "";

  log_d("Trying to send message using UART with I2C enabled");
  transmit_and_check_msg("while used by I2C", false);
  TEST_ASSERT_EQUAL_STRING("", recv_msg.c_str());

  log_d("Disabling I2C and re-enabling UART");

  Serial1.setPins(RX1, TX1);
  uart_internal_loopback(1, true);

  log_d("Trying to send message using UART with I2C disabled");
  transmit_and_check_msg("while I2C is disabled");

  Serial.println("Peripheral manager test successful");
}

// This test checks if messages can be transmitted and received correctly after changing the CPU frequency
void change_cpu_frequency_test() {
  uint32_t old_freq = getCpuFrequencyMhz();
  uint32_t new_freq = getXtalFrequencyMhz();

  log_d("Changing CPU frequency from %dMHz to %dMHz", old_freq, new_freq);
  Serial.flush();
  setCpuFrequencyMhz(new_freq);

  Serial1.updateBaudRate(115200);

  log_d("Trying to send message with the new CPU frequency");
  transmit_and_check_msg("with new CPU frequency");

  log_d("Changing CPU frequency back to %dMHz", old_freq);
  Serial.flush();
  setCpuFrequencyMhz(old_freq);

  Serial1.updateBaudRate(115200);

  log_d("Trying to send message with the original CPU frequency");
  transmit_and_check_msg("with the original CPU frequency");

  Serial.println("Change CPU frequency test successful");
}

/* Main functions */

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  UNITY_BEGIN();
  RUN_TEST(begin_when_running_test);
  RUN_TEST(basic_transmission_test);
  RUN_TEST(resize_buffers_test);
  RUN_TEST(change_baudrate_test);
  RUN_TEST(change_cpu_frequency_test);
  RUN_TEST(disabled_uart_calls_test);
  RUN_TEST(enabled_uart_calls_test);
  RUN_TEST(periman_test);
  RUN_TEST(change_pins_test);
  RUN_TEST(end_when_stopped_test);
  UNITY_END();
}

void loop() {}
