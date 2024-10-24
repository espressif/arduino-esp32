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
//          |  Name   | ESP32 | S2 | S3 | C3 | C6 | H2 | P4 |
// UART0 RX | SOC_RX0 |   3   | 44 | 44 | 20 | 17 | 23 | 38 |
// UART0 TX | SOC_TX0 |   1   | 43 | 43 | 21 | 16 | 24 | 37 |
// UART1 RX |   RX1   |  26   |  4 | 15 | 18 |  4 |  0 | 11 |
// UART1 TX |   TX1   |  27   |  5 | 16 | 19 |  5 |  1 | 10 |
// UART2 RX |   RX2   |   4   | -- | 19 | -- | -- | -- | -- |
// UART2 TX |   TX2   |  25   | -- | 20 | -- | -- | -- | -- |

/*
 * For each UART:
 *
 *           terminal
 *          |       ^
 *          v UART0 |
 *          RX  ^  TX
 *              |
 *        report status
 *              |
 *         TX <---> RX
 *            UARTx
 */

typedef struct test_uart_t
{
  uint8_t rx_pin;
  uint8_t tx_pin;
  HardwareSerial &serial;
} test_uart_t;

const test_uart_t test_uarts[SOC_UART_NUM] = {
  {
    .rx_pin = SOC_RX0,
    .tx_pin = SOC_TX0,
    .serial = Serial,
  },
  {
    .rx_pin = RX1,
    .tx_pin = TX1,
    .serial = Serial1,
  },
#if SOC_UART_NUM >= 3
  {
#ifdef RX2
    .rx_pin = RX2,
    .tx_pin = TX2,
#else
    .rx_pin = RX1,
    .tx_pin = TX1,
#endif
    .serial = Serial2,
  },
#endif  // SOC_UART_NUM >= 3
#if SOC_UART_NUM >= 4
  {
#ifdef RX3
    .rx_pin = RX3,
    .tx_pin = TX3,
#else
    .rx_pin = RX1,
    .tx_pin = TX1,
#endif
    .serial = Serial3,
  },
#endif  // SOC_UART_NUM >= 4
#if SOC_UART_NUM >= 5
  {
#ifdef RX4
    .rx_pin = RX4,
    .tx_pin = TX4,
#else
    .rx_pin = RX1,
    .tx_pin = TX1,
#endif
    .serial = Serial4,
  },
#endif  // SOC_UART_NUM >= 5
};

/* Utility global variables */

uint8_t i;
static String recv_msg = "";
static int peeked_char[SOC_UART_NUM];

/* Utility functions */

extern int8_t uart_get_RxPin(uint8_t uart_num);
extern int8_t uart_get_TxPin(uint8_t uart_num);

// This function starts all the available test UARTs
void start_serial(unsigned long baudrate = 115200) {
  for (i = 1; i < SOC_UART_NUM; i++) {
    test_uarts[i].serial.begin(baudrate, SERIAL_8N1, test_uarts[i].rx_pin, test_uarts[i].tx_pin);
    while (!test_uarts[i].serial) {
      delay(10);
    }
  }
}

// This function stops all the available test UARTs
void stop_serial() {
  for (i = 1; i < SOC_UART_NUM; i++) {
    test_uarts[i].serial.end();
  }
}

// This function transmits a message and checks if it was received correctly
void transmit_and_check_msg(const String msg_append, bool perform_assert = true) {
  delay(100);  // Wait for some settings changes to take effect
  for (i = 1; i < SOC_UART_NUM; i++) {
    test_uarts[i].serial.print("Hello from Serial" + String(i) + " (UART" + String(i) + ") >>> via loopback >>> Serial" + String(i) + " (UART" + String(i) + ") " + msg_append);
    test_uarts[i].serial.flush();
    delay(100);
    if (perform_assert) {
      TEST_ASSERT_EQUAL_STRING(("Hello from Serial" + String(i) + " (UART" + String(i) + ") >>> via loopback >>> Serial" + String(i) + " (UART" + String(i) + ") " + msg_append).c_str(), recv_msg.c_str());
    }
  }
}

/* Tasks */

/*
// This task is used to send a message after a delay to test the auto baudrate detection
void task_delayed_msg(void *pvParameters) {
  HardwareSerial *selected_serial;

#if SOC_UART_NUM >= 3
  selected_serial = &Serial1;
#else
  selected_serial = &Serial;
#endif

  delay(2000);
  selected_serial->println("Hello from Serial to detect baudrate");
  selected_serial->flush();
  vTaskDelete(NULL);
}
*/

/* Unity functions */

// This function is automatically called by unity before each test is run
void setUp(void) {
  start_serial(115200);

  for (i = 1; i < SOC_UART_NUM; i++) {
    log_d("Setup internal loop-back from and back to Serial%d (UART%d) TX >> Serial%d (UART%d) RX", i, i, i, i);

    test_uarts[i].serial.onReceive([]() {
      onReceive_cb(i);
    });
    uart_internal_loopback(i, true);
  }
}

// This function is automatically called by unity after each test is run
void tearDown(void) {
  stop_serial();
}

/* Callback functions */

// This is a callback function that will be activated on UART RX events
void onReceive_cb(int uart_num) {
  HardwareSerial &selected_serial = test_uarts[uart_num].serial;
  char c;

  recv_msg = "";
  size_t available = selected_serial.available();

  if (available != 0) {
    peeked_char[uart_num] = selected_serial.peek();
  }

  while (available--) {
    c = (char)selected_serial.read();
    recv_msg += c;
  }

  log_d("UART%d received message: %s\n", uart_num, recv_msg.c_str());
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

  for (i = 1; i < SOC_UART_NUM; i++) {
    test_uarts[i].serial.updateBaudRate(9600);
    //Baudrate error should be within 2% of the target baudrate
    TEST_ASSERT_UINT_WITHIN(192, 9600, test_uarts[i].serial.baudRate());
  }

  log_d("Sending string using 9600 baudrate");
  transmit_and_check_msg("using 9600 baudrate");

  log_d("Changing baudrate back to 115200");
  start_serial(115200);

  for (i = 1; i < SOC_UART_NUM; i++) {
    //Baudrate error should be within 2% of the target baudrate
    TEST_ASSERT_UINT_WITHIN(2304, 115200, test_uarts[i].serial.baudRate());
  }

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

  // Calling end() twice should not crash
  stop_serial();
  stop_serial();

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
  TEST_ASSERT_GREATER_OR_EQUAL(0, peeked_char[1]);

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
  log_d("Disabling UART loopback");
  //stop_serial();

  for (i = 1; i < SOC_UART_NUM; i++) {
    uart_internal_loopback(i, false);
  }

  log_d("Swapping UART pins");

#if SOC_UART_NUM == 2
  Serial1.setPins(TX1, RX1);  // Invert TX and RX pins
  TEST_ASSERT_EQUAL(TX1, uart_get_RxPin(1));  // TX1 is now RX pin
  TEST_ASSERT_EQUAL(RX1, uart_get_TxPin(1));  // RX1 is now TX pin
#elif SOC_UART_NUM >= 3
  for (i = 1; i < SOC_UART_NUM; i++) {
    // Swapping pins with the next available UART
    int uart_replacement = (i + 1) % SOC_UART_NUM;
    uart_replacement = uart_replacement == 0 ? 1 : uart_replacement;
    test_uarts[i].serial.setPins(test_uarts[uart_replacement].rx_pin, test_uarts[uart_replacement].tx_pin);
    TEST_ASSERT_EQUAL(test_uarts[uart_replacement].tx_pin, uart_get_TxPin(i));
    TEST_ASSERT_EQUAL(test_uarts[uart_replacement].rx_pin, uart_get_RxPin(i));
  }
#endif

  start_serial(115200);

  log_d("Re-enabling UART loopback");

  for (i = 1; i < SOC_UART_NUM; i++) {
    uart_internal_loopback(i, true);
  }

  transmit_and_check_msg("using new pins");

  Serial.println("Change pins test successful");
}

/*

// The new loopback API does not allow cross connecting UARTs. This test is disabled for now.

// This test checks if the auto baudrate detection works on ESP32 and ESP32-S2
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
void auto_baudrate_test(void) {
  log_d("Starting auto baudrate test");

  HardwareSerial *selected_serial;
  unsigned long baudrate;

  log_d("Stopping test serial. Using Serial2 for ESP32 and Serial1 for ESP32-S2.");

#if SOC_UART_HP_NUM == 2
  selected_serial = &Serial1;
  uart_internal_loopback(0, true);  // it was suppose to cross connect TX0 to RX1
#elif SOC_UART_NUM >= 3
  selected_serial = &Serial2;
#endif

  //selected_serial->end(false);

  log_d("Starting delayed task to send message");

  xTaskCreate(task_delayed_msg, "task_delayed_msg", 2048, NULL, 2, NULL);

  log_d("Starting serial with auto baudrate detection");

  selected_serial->begin(0);
  baudrate = selected_serial->baudRate();

#if SOC_UART_HP_NUM == 2
  Serial.end();
  Serial.begin(115200);
#endif

  TEST_ASSERT_UINT_WITHIN(2304, 115200, baudrate);

  Serial.println("Auto baudrate test successful");
}
#endif
*/

// This test checks if the peripheral manager can properly manage UART pins
void periman_test(void) {
  log_d("Checking if peripheral manager can properly manage UART pins");

  for (i = 1; i < SOC_UART_NUM; i++) {
    log_d("Setting up I2C on the same pins as UART%d", i);
    Wire.begin(test_uarts[i].rx_pin, test_uarts[i].tx_pin);

    recv_msg = "";

    log_d("Trying to send message using UART%d with I2C enabled", i);
    test_uarts[i].serial.print("Hello from Serial" + String(i) + " (UART" + String(i) + ") >>> via loopback >>> Serial" + String(i) + " (UART" + String(i) + ") while used by I2C");
    test_uarts[i].serial.flush();
    delay(100);
    TEST_ASSERT_EQUAL_STRING("", recv_msg.c_str());

    log_d("Disabling I2C and re-enabling UART");

    Serial1.setPins(test_uarts[i].rx_pin, test_uarts[i].tx_pin);
    uart_internal_loopback(i, true);

    log_d("Trying to send message using UART with I2C disabled");
    transmit_and_check_msg("while I2C is disabled");
  }

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
  while (!Serial) {
    delay(10);
  }
  log_d("SOC_UART_HP_NUM = %d", SOC_UART_HP_NUM);

  for (i = 0; i < SOC_UART_NUM; i++) {
    peeked_char[i] = -1;
  }

  // Begin needs to be called before setting up the loopback because it creates the serial object
  start_serial(115200);

  for (i = 1; i < SOC_UART_NUM; i++) {
    log_d("Setup internal loop-back from and back to Serial%d (UART%d) TX >> Serial%d (UART%d) RX", i, i, i, i);
    test_uarts[i].serial.onReceive([]() {
      onReceive_cb(i);
    });
    uart_internal_loopback(i, true);
  }

  log_d("Setup done. Starting tests");

  UNITY_BEGIN();
  RUN_TEST(begin_when_running_test);
  RUN_TEST(basic_transmission_test);
  RUN_TEST(resize_buffers_test);
  RUN_TEST(change_baudrate_test);
  RUN_TEST(change_cpu_frequency_test);
  RUN_TEST(disabled_uart_calls_test);
  RUN_TEST(enabled_uart_calls_test);
/*
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
  RUN_TEST(auto_baudrate_test);
#endif
*/
  RUN_TEST(periman_test);
  RUN_TEST(change_pins_test);
  RUN_TEST(end_when_stopped_test);
  UNITY_END();
}

void loop() {
  vTaskDelete(NULL);
}
