/* UART test
 *
 * This test is using UART0 (Serial) only for reporting test status and helping with the auto
 * baudrate detection test.
 * UART1 (Serial1) and UART2 (Serial2), where available, are used for testing.
 */

#include <unity.h>
#include "HardwareSerial.h"
#include "esp_rom_gpio.h"
#include "Wire.h"

// Default pins:
//          |  Name   | ESP32 | S2 | S3 | C3 | C6 | H2 |
// UART0 RX | SOC_RX0 |   3   | 44 | 44 | 20 | 17 | 23 |
// UART0 TX | SOC_TX0 |   1   | 43 | 43 | 21 | 16 | 24 |
// UART1 RX |   RX1   |  26   |  4 | 15 | 18 |  4 |  0 |
// UART1 TX |   TX1   |  27   |  5 | 16 | 19 |  5 |  1 |
// UART2 RX |   RX2   |   4   | -- | 19 | -- | -- | -- |
// UART2 TX |   TX2   |  25   | -- | 20 | -- | -- | -- |

/*
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

#if SOC_UART_NUM == 2
  // Used for the pin swap test
  #define NEW_RX1 6
  #define NEW_TX1 7
#endif

/* Utility global variables */

static String recv_msg = "";
static int peeked_char = -1;

/* Utility functions */

extern int8_t uart_get_RxPin(uint8_t uart_num);
extern int8_t uart_get_TxPin(uint8_t uart_num);

// This function starts all the available test UARTs
void start_serial(unsigned long baudrate = 115200) {
  #if SOC_UART_NUM >= 2
    Serial1.begin(baudrate);
    while(!Serial1) { delay(10); }
  #endif

  #if SOC_UART_NUM >= 3
    Serial2.begin(baudrate);
    while(!Serial2) { delay(10); }
  #endif
}

// This function stops all the available test UARTs
void stop_serial(bool hard_stop = false) {
  #if SOC_UART_NUM >= 2
    Serial1.end(hard_stop);
  #endif

  #if SOC_UART_NUM >= 3
    Serial2.end(hard_stop);
  #endif
}

// This function transmits a message and checks if it was received correctly
void transmit_and_check_msg(const String msg_append, bool perform_assert = true) {
  delay(100); // Wait for some settings changes to take effect
  #if SOC_UART_NUM == 2
    Serial1.print("Hello from Serial1 (UART1) >>> via loopback >>> Serial1 (UART1) " + msg_append);
    Serial1.flush();
    delay(100);
    if (perform_assert) {
      TEST_ASSERT_EQUAL_STRING(("Hello from Serial1 (UART1) >>> via loopback >>> Serial1 (UART1) " + msg_append).c_str(), recv_msg.c_str());
    }
  #elif SOC_UART_NUM == 3
    Serial1.print("Hello from Serial1 (UART1) >>> to >>> Serial2 (UART2) " + msg_append);
    Serial1.flush();
    delay(100);
    if (perform_assert) {
      TEST_ASSERT_EQUAL_STRING(("Hello from Serial1 (UART1) >>> to >>> Serial2 (UART2) " + msg_append).c_str(), recv_msg.c_str());
    }

    Serial2.print("Hello from Serial2 (UART2) >>> to >>> Serial1 (UART1) " + msg_append);
    Serial2.flush();
    delay(100);
    if (perform_assert) {
      TEST_ASSERT_EQUAL_STRING(("Hello from Serial2 (UART2) >>> to >>> Serial1 (UART1) " + msg_append).c_str(), recv_msg.c_str());
    }
  #else
    log_d("No UARTs available for transmission");
    TEST_FAIL();
  #endif
}

/* Tasks */

// This task is used to send a message after a delay to test the auto baudrate detection
void task_delayed_msg(void *pvParameters) {
  HardwareSerial *selected_serial;

  #if SOC_UART_NUM == 2
    selected_serial = &Serial;
  #elif SOC_UART_NUM == 3
    selected_serial = &Serial1;
  #endif

  delay(2000);
  selected_serial->println("Hello from Serial1 to detect baudrate");
  selected_serial->flush();
  vTaskDelete(NULL);
}

/* Unity functions */

// This function is automatically called by unity before each test is run
void setUp(void) {
  start_serial(115200);
}

// This function is automatically called by unity after each test is run
void tearDown(void) {
  stop_serial();
}

/* Callback functions */

// This is a callback function that will be activated on UART RX events
void onReceive_cb(HardwareSerial &selected_serial) {
  int uart_num = -1;
  char c;

  (void)uart_num; // Avoid compiler warning when debug level is set to none

  if (&selected_serial == &Serial) {
    uart_num = 0;
#if SOC_UART_NUM >= 2
  } else if (&selected_serial == &Serial1) {
    uart_num = 1;
#endif
#if SOC_UART_NUM >= 3
  } else if (&selected_serial == &Serial2) {
    uart_num = 2;
#endif
  }

  recv_msg = "";
  size_t available = selected_serial.available();

  if (available != 0) {
    peeked_char = selected_serial.peek();
  }

  while (available --) {
    c = (char)selected_serial.read();
    recv_msg += c;
  }

  log_d("UART %d received message: %s\n", uart_num, recv_msg.c_str());
}

/* Test functions */

// This test checks if a message can be transmitted and received correctly using the default settings
void basic_transmission_test(void) {
  log_d("Performing basic transmission test");

  transmit_and_check_msg("");

  Serial.println("Basic transmission test successful");
}

// This test checks if the baudrate can be changed and if the message can be transmitted and received correctly after the change
void change_baudrate_test(void) {
  //Test first using the updateBaudRate method and then using the begin method
  log_d("Changing baudrate to 9600");

  //Baudrate error should be within 2% of the target baudrate
  Serial1.updateBaudRate(9600);
  TEST_ASSERT_UINT_WITHIN(192, 9600, Serial1.baudRate());

  #if SOC_UART_NUM == 3
    Serial2.updateBaudRate(9600);
    TEST_ASSERT_UINT_WITHIN(192, 9600, Serial2.baudRate());
  #endif

  log_d("Sending string using 9600 baudrate");
  transmit_and_check_msg("using 9600 baudrate");

  log_d("Changing baudrate back to 115200");
  start_serial(115200);

  //Baudrate error should be within 2% of the target baudrate
  TEST_ASSERT_UINT_WITHIN(2304, 115200, Serial1.baudRate());

  #if SOC_UART_NUM == 3
    TEST_ASSERT_UINT_WITHIN(2304, 115200, Serial2.baudRate());
  #endif

  log_d("Sending string using 115200 baudrate");
  transmit_and_check_msg("using 115200 baudrate");

  Serial.println("Change baudrate test successful");
}


// This test checks if the buffers can be resized properly
void resize_buffers_test(void) {
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
void begin_when_running_test(void) {
  log_d("Trying to set up serial twice");
  start_serial(115200);
  Serial.println("Begin when running test successful");
}

// This test checks if the end function can be called when the UART is already stopped
void end_when_stopped_test(void) {
  log_d("Trying to end serial twice");

  // Calling end(true) twice should not crash
  stop_serial(true);
  stop_serial(true);

  Serial.println("End when stopped test successful");
}

// This test checks if all the UART methods work when the UART is running
void enabled_uart_calls_test(void) {
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
void disabled_uart_calls_test(void) {
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
void change_pins_test(void) {
  stop_serial();

  log_d("Disabling UART loopback");

  #if SOC_UART_NUM == 2
    esp_rom_gpio_connect_out_signal(SOC_RX0, SIG_GPIO_OUT_IDX, false, false);
  #elif SOC_UART_NUM == 3
    esp_rom_gpio_connect_out_signal(RX1, SIG_GPIO_OUT_IDX, false, false);
    esp_rom_gpio_connect_out_signal(RX2, SIG_GPIO_OUT_IDX, false, false);
  #endif

  log_d("Swapping UART pins");

  #if SOC_UART_NUM == 2
    Serial1.setPins(NEW_RX1, NEW_TX1);
    TEST_ASSERT_EQUAL(NEW_RX1, uart_get_RxPin(1));
    TEST_ASSERT_EQUAL(NEW_TX1, uart_get_TxPin(1));
  #elif SOC_UART_NUM == 3
    Serial1.setPins(RX2, TX2);
    Serial2.setPins(RX1, TX1);
    TEST_ASSERT_EQUAL(RX2, uart_get_RxPin(1));
    TEST_ASSERT_EQUAL(TX2, uart_get_TxPin(1));
    TEST_ASSERT_EQUAL(RX1, uart_get_RxPin(2));
    TEST_ASSERT_EQUAL(TX1, uart_get_TxPin(2));
  #endif

  start_serial(115200);

  log_d("Re-enabling UART loopback");

  #if SOC_UART_NUM == 2
    uart_internal_loopback(1, NEW_RX1);
  #elif SOC_UART_NUM == 3
    uart_internal_loopback(1, RX1);
    uart_internal_loopback(2, RX2);
  #endif

  transmit_and_check_msg("using new pins");

  Serial.println("Change pins test successful");

}

// This test checks if the auto baudrate detection works on ESP32 and ESP32-S2
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
void auto_baudrate_test(void) {
  log_d("Starting auto baudrate test");

  HardwareSerial *selected_serial;
  unsigned long baudrate;

  log_d("Stopping test serial. Using Serial2 for ESP32 and Serial1 for ESP32-S2.");

  #if SOC_UART_NUM == 2
    selected_serial = &Serial1;
    uart_internal_loopback(0, RX1);
  #elif SOC_UART_NUM == 3
    selected_serial = &Serial2;
  #endif

  selected_serial->end(false);

  log_d("Starting delayed task to send message");

  xTaskCreate(task_delayed_msg, "task_delayed_msg", 2048, NULL, 2, NULL);

  log_d("Starting serial with auto baudrate detection");

  selected_serial->begin(0);
  baudrate = selected_serial->baudRate();

  #if SOC_UART_NUM == 2
    Serial.end();
    Serial.begin(115200);
  #endif

  TEST_ASSERT_UINT_WITHIN(2304, 115200, baudrate);

  Serial.println("Auto baudrate test successful");
}
#endif

// This test checks if the peripheral manager can properly manage UART pins
void periman_test(void) {
  log_d("Checking if peripheral manager can properly manage UART pins");

  log_d("Setting up I2C on the same pins as UART");

  Wire.begin(RX1, TX1);

  #if SOC_UART_NUM == 3
    Wire1.begin(RX2, TX2);
  #endif

  recv_msg = "";

  log_d("Trying to send message using UART with I2C enabled");
  transmit_and_check_msg("while used by I2C", false);
  TEST_ASSERT_EQUAL_STRING("", recv_msg.c_str());

  log_d("Disabling I2C and re-enabling UART");

  Serial1.setPins(RX1, TX1);

  #if SOC_UART_NUM == 3
    Serial2.setPins(RX2, TX2);
    uart_internal_loopback(1, RX2);
    uart_internal_loopback(2, RX1);
  #elif SOC_UART_NUM == 2
    uart_internal_loopback(1, RX1);
  #endif

  log_d("Trying to send message using UART with I2C disabled");
  transmit_and_check_msg("while I2C is disabled");

  Serial.println("Peripheral manager test successful");
}

// This test checks if messages can be transmitted and received correctly after changing the CPU frequency
void change_cpu_frequency_test(void) {
  uint32_t old_freq = getCpuFrequencyMhz();
  uint32_t new_freq = getXtalFrequencyMhz();

  log_d("Changing CPU frequency from %dMHz to %dMHz", old_freq, new_freq);
  Serial.flush();
  setCpuFrequencyMhz(new_freq);

  Serial.updateBaudRate(115200);

  log_d("Trying to send message with the new CPU frequency");
  transmit_and_check_msg("with new CPU frequency");

  log_d("Changing CPU frequency back to %dMHz", old_freq);
  Serial.flush();
  setCpuFrequencyMhz(old_freq);

  Serial.updateBaudRate(115200);

  log_d("Trying to send message with the original CPU frequency");
  transmit_and_check_msg("with the original CPU frequency");

  Serial.println("Change CPU frequency test successful");
}

/* Main functions */

void setup() {
  Serial.begin(115200);
  while(!Serial) { delay(10); }
  log_d("SOC_UART_NUM = %d", SOC_UART_NUM);

  // Begin needs to be called before setting up the loopback because it creates the serial object
  start_serial(115200);

#if SOC_UART_NUM == 2
  log_d("Setup internal loop-back from and back to Serial1 (UART1) TX >> Serial1 (UART1) RX");

  Serial1.onReceive([]() {onReceive_cb(Serial1);});
  uart_internal_loopback(1, RX1);
#elif SOC_UART_NUM == 3
  log_d("Setup internal loop-back between Serial1 (UART1) <<--->> Serial2 (UART2)");

  Serial1.onReceive([]() {onReceive_cb(Serial1);});
  Serial2.onReceive([]() {onReceive_cb(Serial2);});
  uart_internal_loopback(1, RX2);
  uart_internal_loopback(2, RX1);
#endif

  log_d("Setup done. Starting tests");

  UNITY_BEGIN();
  RUN_TEST(begin_when_running_test);
  RUN_TEST(basic_transmission_test);
  RUN_TEST(resize_buffers_test);
  RUN_TEST(change_baudrate_test);
  RUN_TEST(change_cpu_frequency_test);
  RUN_TEST(disabled_uart_calls_test);
  RUN_TEST(enabled_uart_calls_test);
  #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
    RUN_TEST(auto_baudrate_test);
  #endif
  RUN_TEST(periman_test);
  RUN_TEST(change_pins_test);
  RUN_TEST(end_when_stopped_test);
  UNITY_END();
}

void loop() {}
