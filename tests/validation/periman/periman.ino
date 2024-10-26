/* Peripheral Manager test

   This test is using Serial to check if the peripheral manager is able to
   attach and detach peripherals correctly on shared pins.
   Make sure that the peripheral names contain only letters, numbers and underscores.

   This test skips the following peripherals:
   - USB: USB is not able to be detached
   - SDMMC: SDMMC requires a card to be mounted before the pins are attached
   - ETH: ETH requires a ethernet port to be connected before the pins are attached
*/

#if SOC_I2S_SUPPORTED
#include "ESP_I2S.h"
#endif

#if SOC_I2C_SUPPORTED
#include "Wire.h"
#endif

#if SOC_GPSPI_SUPPORTED
#include "SPI.h"
#endif

/* Definitions */

#define UART1_RX_DEFAULT 4
#define UART1_TX_DEFAULT 5

#define ADC1_DEFAULT A4

#if CONFIG_IDF_TARGET_ESP32
#define ADC2_DEFAULT A5
#else
#define ADC2_DEFAULT A3
#endif

#if CONFIG_IDF_TARGET_ESP32
#define TOUCH1_DEFAULT T0
#define TOUCH2_DEFAULT T2
#else
#define TOUCH1_DEFAULT T4
#define TOUCH2_DEFAULT T5
#endif

/* Global variables */

bool test_executed = false;
String current_test;
int8_t uart1_rx_pin;
int8_t uart1_tx_pin;

/* Callback functions */

void onReceive_cb(void) {
  // This is a callback function that will be activated on UART RX events
  size_t available = Serial1.available();
  while (available--) {
    Serial.print((char)Serial1.read());
  }
}

// This function is called by before each test is run
void setup_test(String test_name, int8_t rx_pin = UART1_RX_DEFAULT, int8_t tx_pin = UART1_TX_DEFAULT) {
  log_i("Setting up %s test", test_name.c_str());

  current_test = test_name;
  uart1_rx_pin = rx_pin;
  uart1_tx_pin = tx_pin;
  test_executed = false;

  pinMode(uart1_rx_pin, INPUT_PULLUP);
  pinMode(uart1_tx_pin, OUTPUT);
  Serial1.setPins(uart1_rx_pin, uart1_tx_pin);
  uart_internal_loopback(1, true);
  delay(100);
  log_i("Running %s test", test_name.c_str());
}

// This function is called after each test is run
void teardown_test(void) {
  log_i("Tearing down %s test", current_test.c_str());
  if (test_executed) {
    pinMode(uart1_rx_pin, INPUT_PULLUP);
    pinMode(uart1_tx_pin, OUTPUT);
    Serial1.print(current_test);
    Serial1.println(" test: This should not be printed");
    Serial1.flush();

    Serial1.setPins(uart1_rx_pin, uart1_tx_pin);
    uart_internal_loopback(1, true);
    delay(100);
  }

  Serial1.print(current_test);
  Serial1.println(" test: This should be printed");
  Serial1.flush();

  log_i("Finished %s test", current_test.c_str());
  Serial.println("=========================================================================================");
  delay(2000);
}

/* Test functions */
/* These functions must call "setup_test" and "teardown_test" and set "test_executed" to true
   if the test is executed
*/

void gpio_test(void) {
  setup_test("GPIO");
  test_executed = true;
  pinMode(uart1_rx_pin, INPUT);
  pinMode(uart1_tx_pin, OUTPUT);
  digitalRead(uart1_rx_pin);
  digitalWrite(uart1_tx_pin, HIGH);
  teardown_test();
}

void sigmadelta_test(void) {
  setup_test("SigmaDelta");
#if SOC_SDM_SUPPORTED
  test_executed = true;
  if (!sigmaDeltaAttach(uart1_rx_pin, 312500)) {
    Serial.println("SigmaDelta init failed");
  }
  if (!sigmaDeltaAttach(uart1_tx_pin, 312500)) {
    Serial.println("SigmaDelta init failed");
  }
#endif
  teardown_test();
}

void adc_oneshot_test(void) {
#if !SOC_ADC_SUPPORTED
  setup_test("ADC_Oneshot - NO ADC Support");
#else
  setup_test("ADC_Oneshot", ADC1_DEFAULT, ADC2_DEFAULT);
  test_executed = true;
  analogReadResolution(12);
  pinMode(ADC1_DEFAULT, INPUT);
  pinMode(ADC2_DEFAULT, INPUT);
  analogRead(ADC1_DEFAULT);
  analogRead(ADC2_DEFAULT);
#endif
  teardown_test();
}

#if SOC_ADC_SUPPORTED
volatile bool adc_coversion_done = false;
void ARDUINO_ISR_ATTR adcComplete() {
  adc_coversion_done = true;
}
#endif

void adc_continuous_test(void) {
#if !SOC_ADC_SUPPORTED || CONFIG_IDF_TARGET_ESP32P4
  setup_test("ADC_Continuous - NO ADC Support");
#else
  setup_test("ADC_Continuous", ADC1_DEFAULT, ADC2_DEFAULT);
  test_executed = true;
  uint8_t adc_pins[] = {ADC1_DEFAULT, ADC2_DEFAULT};
  uint8_t adc_pins_count = 2;
  adc_continuous_data_t *result = NULL;

  analogContinuousSetWidth(12);
  analogContinuousSetAtten(ADC_11db);

  analogContinuous(adc_pins, adc_pins_count, 6, 20000, &adcComplete);
  analogContinuousStart();

  Serial.println();
  while (adc_coversion_done == false) {
    Serial.print(".");
    delay(500);
  }

  if (!analogContinuousRead(&result, 0)) {
    Serial.println("ADC continuous read failed");
  }

  analogContinuousStop();
#endif
  teardown_test();
}

void dac_test(void) {
#if !SOC_DAC_SUPPORTED
  setup_test("DAC");
#else
  setup_test("DAC", DAC1, DAC2);
  test_executed = true;
  dacWrite(DAC1, 255);
  dacWrite(DAC2, 255);
#endif
  teardown_test();
}

void ledc_test(void) {
  setup_test("LEDC");
#if SOC_LEDC_SUPPORTED
  test_executed = true;
  if (!ledcAttach(uart1_rx_pin, 5000, 12)) {
    Serial.println("LEDC init failed");
  }
  if (!ledcAttach(uart1_tx_pin, 5000, 12)) {
    Serial.println("LEDC init failed");
  }
#endif
  teardown_test();
}

void rmt_test(void) {
  setup_test("RMT");
#if SOC_RMT_SUPPORTED
  test_executed = true;
  if (!rmtInit(uart1_rx_pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)) {
    Serial.println("RMT init failed");
  }
  if (!rmtInit(uart1_tx_pin, RMT_RX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)) {
    Serial.println("RMT init failed");
  }
#endif
  teardown_test();
}

void i2s_test(void) {
  setup_test("I2S");
#if SOC_I2S_SUPPORTED
  test_executed = true;
  I2SClass i2s;

  i2s.setPins(uart1_rx_pin, uart1_tx_pin, -1);

  i2s.setTimeout(1000);
  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
  }
#endif
  teardown_test();
  delay(500); Serial.println("Exting I2S driver..."); delay(500);
}

void i2c_test(void) {
  setup_test("I2C");
#if SOC_I2C_SUPPORTED
  test_executed = true;
  if (!Wire.begin(uart1_rx_pin, uart1_tx_pin)) {
    Serial.println("I2C init failed");
  }
#endif
  teardown_test();
}

void spi_test(void) {
  setup_test("SPI");
#if SOC_GPSPI_SUPPORTED
  test_executed = true;
  SPI.begin(uart1_rx_pin, uart1_tx_pin, -1, -1);
#endif
  teardown_test();
}

void touch_test(void) {
#if !SOC_TOUCH_SENSOR_SUPPORTED
  setup_test("Touch");
#else
  setup_test("Touch", TOUCH1_DEFAULT, TOUCH2_DEFAULT);
  test_executed = true;
  touchRead(TOUCH1_DEFAULT);
  touchRead(TOUCH2_DEFAULT);
#endif
  teardown_test();
}

/* Main functions */

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial1.setPins(UART1_RX_DEFAULT, UART1_TX_DEFAULT);
  Serial1.begin(115200);
  while (!Serial1) {
    delay(10);
  }
  Serial1.onReceive(onReceive_cb);
  uart_internal_loopback(1, true);

  Serial.println("starting gpio_test()");
  gpio_test();
  Serial.println("starting sigmadelta_test()");
  sigmadelta_test();
  Serial.println("starting ledc_test()");
  ledc_test();
  Serial.println("starting rmt_test()");
  rmt_test();
  Serial.println("\t ==>starting i2s_test()");
  i2s_test();  // apos esta linha temos um [E][esp32-hal-periman.c:122] perimanSetPinBus(): Invalid pin: 255
  Serial.println("\t ==>Back to setup()");
  Serial.println("\t ==>starting spi_test()");
  spi_test();
  Serial.println("\t ==>starting i2c_test()");
  i2c_test();
  Serial.println("\t ==>ended i2c_test()");
  Serial.println("starting adc_oneshot_test()");
  adc_oneshot_test();
  Serial.println("starting adc_continuous_test()");
  adc_continuous_test();
  Serial.println("starting dac_test()");
  dac_test();
  Serial.println("starting touch_test()");
  touch_test();
  Serial.println("starting gpio_test()");

  // Print to Serial1 to avoid buffering issues
  Serial1.println("Peripheral Manager test done");
}

void loop() {}
