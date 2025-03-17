/* UART test
 *
 * This test is using UART0 (Serial) only for reporting test status and helping with the auto
 * baudrate detection test.
 * The other serials are used for testing.
 */

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

#include <vector>
#include <unity.h>
#include "HardwareSerial.h"
#include "esp_rom_gpio.h"
#include "Wire.h"

/* Utility defines */

#define TEST_UART_NUM (uart_test_configs.size())

/* Utility classes */

class UARTTestConfig {
public:
  int uart_num;
  HardwareSerial &serial;
  int peeked_char;
  int8_t default_rx_pin;
  int8_t default_tx_pin;
  String recv_msg;

  UARTTestConfig(int num, HardwareSerial &serial_ref, int8_t rx_pin, int8_t tx_pin)
    : uart_num(num), serial(serial_ref), peeked_char(-1), default_rx_pin(rx_pin), default_tx_pin(tx_pin), recv_msg("") {}

  void begin(unsigned long baudrate) {
    // pinMode will force enabling the internal pullup resistor (IDF 5.3.2 Change)
    pinMode(default_rx_pin, INPUT_PULLUP);
    serial.begin(baudrate, SERIAL_8N1, default_rx_pin, default_tx_pin);
    while (!serial) {
      delay(10);
    }
  }

  void end() {
    serial.end();
  }

  void reset_buffers() {
    recv_msg = "";
    peeked_char = -1;
  }

  void transmit_and_check_msg(const String &msg_append, bool perform_assert = true) {
    reset_buffers();
    delay(100);
    serial.print("Hello from Serial" + String(uart_num) + " " + msg_append);
    serial.flush();
    delay(100);
    if (perform_assert) {
      TEST_ASSERT_EQUAL_STRING(("Hello from Serial" + String(uart_num) + " " + msg_append).c_str(), recv_msg.c_str());
      log_d("UART%d received message: %s\n", uart_num, recv_msg.c_str());
    }
  }

  void onReceive() {
    char c;
    size_t available = serial.available();
    if (peeked_char == -1) {
      peeked_char = serial.peek();
    }
    while (available--) {
      c = (char)serial.read();
      recv_msg += c;
    }
  }
};

/* Utility global variables */

[[maybe_unused]]
static const int NEW_RX1 = 9;
[[maybe_unused]]
static const int NEW_TX1 = 10;
std::vector<UARTTestConfig *> uart_test_configs;

/* Utility functions */

extern "C" int8_t uart_get_RxPin(uint8_t uart_num);
extern "C" int8_t uart_get_TxPin(uint8_t uart_num);

/* Tasks */

// This task is used to send a message after a delay to test the auto baudrate detection
void task_delayed_msg(void *pvParameters) {
  HardwareSerial &selected_serial = uart_test_configs.size() == 1 ? Serial : Serial1;
  delay(2000);
  selected_serial.println("Hello to detect baudrate");
  selected_serial.flush();
  vTaskDelete(NULL);
}

/* Unity functions */

// This function is automatically called by unity before each test is run
void setUp(void) {
  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    //log_d("Setup internal loop-back from and back to UART%d TX >> UART%d RX", config.uart_num, config.uart_num);
    config.begin(115200);
    config.serial.onReceive([&config]() {
      config.onReceive();
    });
    uart_internal_loopback(config.uart_num, uart_get_RxPin(config.uart_num));
  }
}

// This function is automatically called by unity after each test is run
void tearDown(void) {
  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    config.end();
  }
}

/* Test functions */

// This test checks if a message can be transmitted and received correctly using the default settings
void basic_transmission_test(void) {
  log_d("Performing basic transmission test");

  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    config.transmit_and_check_msg("");
  }

  Serial.println("Basic transmission test successful");
}

// This test checks if the baudrate can be changed and if the message can be transmitted and received correctly after the change
void change_baudrate_test(void) {
  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    log_d("Changing baudrate of UART%d to 9600", config.uart_num);

    //Baudrate error should be within 2% of the target baudrate
    config.serial.updateBaudRate(9600);
    TEST_ASSERT_UINT_WITHIN(192, 9600, config.serial.baudRate());

    log_d("Sending string on UART%d using 9600 baudrate", config.uart_num);
    config.transmit_and_check_msg("using 9600 baudrate");

    config.serial.begin(115200);
    TEST_ASSERT_UINT_WITHIN(2304, 115200, config.serial.baudRate());

    log_d("Sending string on UART%d using 115200 baudrate", config.uart_num);
    config.transmit_and_check_msg("using 115200 baudrate");
  }

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

  Serial1.end();

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
  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    // Calling twice should not crash
    config.begin(115200);
    config.begin(115200);
  }
  Serial.println("Begin when running test successful");
}

// This test checks if the end function can be called when the UART is already stopped
void end_when_stopped_test(void) {
  log_d("Trying to end serial twice");

  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    // Calling twice should not crash
    config.end();
    config.end();
  }

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
  TEST_ASSERT_GREATER_OR_EQUAL(0, uart_test_configs[0]->peeked_char);

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

  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    config.end();
  }

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

  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    esp_rom_gpio_connect_out_signal(config.default_rx_pin, SIG_GPIO_OUT_IDX, false, false);
  }

  log_d("Swapping UART pins and testing transmission");

  if (TEST_UART_NUM == 1) {
    UARTTestConfig &config = *uart_test_configs[0];
    // pinMode will force enabling the internal pullup resistor (IDF 5.3.2 Change)
    pinMode(NEW_RX1, INPUT_PULLUP);
    config.serial.setPins(NEW_RX1, NEW_TX1);
    TEST_ASSERT_EQUAL(NEW_RX1, uart_get_RxPin(config.uart_num));
    TEST_ASSERT_EQUAL(NEW_TX1, uart_get_TxPin(config.uart_num));

    uart_internal_loopback(config.uart_num, NEW_RX1);
    config.transmit_and_check_msg("using new pins");
  } else {
    for (int i = 0; i < TEST_UART_NUM; i++) {
      UARTTestConfig &config = *uart_test_configs[i];
      UARTTestConfig &next_uart = *uart_test_configs[(i + 1) % TEST_UART_NUM];
      config.serial.setPins(next_uart.default_rx_pin, next_uart.default_tx_pin);
      TEST_ASSERT_EQUAL(uart_get_RxPin(config.uart_num), next_uart.default_rx_pin);
      TEST_ASSERT_EQUAL(uart_get_TxPin(config.uart_num), next_uart.default_tx_pin);

      uart_internal_loopback(config.uart_num, next_uart.default_rx_pin);
      config.transmit_and_check_msg("using new pins");
    }
  }

  Serial.println("Change pins test successful");
}

// This test checks if the auto baudrate detection works on ESP32 and ESP32-S2
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
void auto_baudrate_test(void) {
  log_d("Starting auto baudrate test");

  HardwareSerial *selected_serial;
  unsigned long baudrate;

  log_d("Stopping test serial. Using Serial2 for ESP32 and Serial1 for ESP32-S2.");

  if (TEST_UART_NUM == 1) {
    selected_serial = &Serial1;
    // UART1 pins were swapped because of ESP32-P4
    uart_internal_loopback(0, /*RX1*/ TX1);
  } else {
#ifdef RX2
    selected_serial = &Serial2;
    uart_internal_loopback(1, RX2);
#endif
  }

  //selected_serial->end(false);

  log_d("Starting delayed task to send message");

  xTaskCreate(task_delayed_msg, "task_delayed_msg", 2048, NULL, 2, NULL);

  log_d("Starting serial with auto baudrate detection");

  selected_serial->begin(0);
  baudrate = selected_serial->baudRate();

  if (TEST_UART_NUM == 1) {
    Serial.end();
    Serial.begin(115200);
  }

  TEST_ASSERT_UINT_WITHIN(2304, 115200, baudrate);

  Serial.println("Auto baudrate test successful");
}
#endif

// This test checks if the peripheral manager can properly manage UART pins
void periman_test(void) {
  log_d("Checking if peripheral manager can properly manage UART pins");

  log_d("Setting up I2C on the same pins as UART");

  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    Wire.begin(config.default_rx_pin, config.default_tx_pin);
    config.recv_msg = "";

    log_d("Trying to send message using UART%d with I2C enabled", config.uart_num);
    config.transmit_and_check_msg("while used by I2C", false);
    TEST_ASSERT_EQUAL_STRING("", config.recv_msg.c_str());

    log_d("Disabling I2C and re-enabling UART%d", config.uart_num);

    config.serial.setPins(config.default_rx_pin, config.default_tx_pin);
    uart_internal_loopback(config.uart_num, config.default_rx_pin);

    log_d("Trying to send message using UART%d with I2C disabled", config.uart_num);
    config.transmit_and_check_msg("while I2C is disabled");
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

  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    log_d("Trying to send message with the new CPU frequency on UART%d", config.uart_num);
    config.transmit_and_check_msg("with new CPU frequency");
  }

  log_d("Changing CPU frequency back to %dMHz", old_freq);
  Serial.flush();
  setCpuFrequencyMhz(old_freq);

  Serial.updateBaudRate(115200);

  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    log_d("Trying to send message with the original CPU frequency on UART%d", config.uart_num);
    config.transmit_and_check_msg("with the original CPU frequency");
  }

  Serial.println("Change CPU frequency test successful");
}

/* Main functions */

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  uart_test_configs = {
#if SOC_UART_HP_NUM >= 2 && defined(RX1) && defined(TX1)
    // inverting RX1<->TX1 because ESP32-P4 has a problem with loopback on RX1 :: GPIO11 <-- UART_TX SGINAL
    new UARTTestConfig(1, Serial1, TX1, RX1),
#endif
#if SOC_UART_HP_NUM >= 3 && defined(RX2) && defined(TX2)
    new UARTTestConfig(2, Serial2, RX2, TX2),
#endif
#if SOC_UART_HP_NUM >= 4 && defined(RX3) && defined(TX3)
    new UARTTestConfig(3, Serial3, RX3, TX3),
#endif
#if SOC_UART_HP_NUM >= 5 && defined(RX4) && defined(TX4)
    new UARTTestConfig(4, Serial4, RX4, TX4)
#endif
  };

  if (TEST_UART_NUM == 0) {
    log_e("This test requires at least one UART besides UART0 configured");
    abort();
  }

  log_d("TEST_UART_NUM = %d", TEST_UART_NUM);

  for (auto *ref : uart_test_configs) {
    UARTTestConfig &config = *ref;
    config.begin(115200);
    log_d("Setup internal loop-back from and back to UART%d TX >> UART%d RX", config.uart_num, config.uart_num);
    config.serial.onReceive([&config]() {
      config.onReceive();
    });
    uart_internal_loopback(config.uart_num, uart_get_RxPin(config.uart_num));
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
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
  RUN_TEST(auto_baudrate_test);
#endif
  RUN_TEST(periman_test);
  RUN_TEST(change_pins_test);
  RUN_TEST(end_when_stopped_test);
  UNITY_END();
}

void loop() {}
