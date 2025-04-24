/*

   This Sketch demonstrates the effect of changing RX Timeout parameter into HardwareSerial Class
   Serial.setRxTimeout(byte) is used to change it.
   By default, UART ISR will wait for an RX Timeout equivalent to 2 UART symbols to understand that a flow.
   of UART data has ended. For example, if just one byte is received, UART will send about 10 to
   11 bits depending of the configuration (parity, number of stopbits). The timeout is measured in
   number of UART symbols, with 10 or 11 bits, in the current baudrate.
   For 9600 baud, 1 bit takes 1/9600 of a second, equivalent to 104 microseconds, therefore, for 10 bits,
   it takes about 1ms. A timeout of 2 UART symbols, with about 20 bits, would take about 2.1 milliseconds
   for the ESP32 UART to trigger an IRQ telling the UART driver that the transmission has ended.
   Just at this point, the data will be made available to Arduino HardwareSerial API (read(), available(), etc).

   The way we demonstrate the effect of this parameter is by measuring the time the Sketch takes
   to read data using Arduino HardwareSerial API.

   The higher RX Timeout is, the longer it will take to make the data available, when a flow of data ends.
   UART driver works copying data from UART FIFO to Arduino internal buffer.
   The driver will copy data from FIFO when RX Timeout is detected or when FIFO is full.
   ESP32 FIFO has 128 bytes and by default, the driver will copy the data when FIFO reaches 120 bytes.
   If UART receives less than 120 bytes, it will wait RX Timeout to understand that the bus is IDLE and
   then copy the data from the FIFO to the Arduino internal buffer, making it available to the Arduino API.

   There is an important detail about how HardwareSerial works using ESP32 and ESP32-S2:
   If the baud rate is lower than 250,000, it will select REF_TICK as clock source in order to avoid that
   the baud rate may change when the CPU Frequency is changed. Default UART clock source is APB, which changes
   when CPU clock source is also changed. But when it selects REF_TICK as UART clock source, RX Timeout is limited to 1.
   Therefore, in order to change the ESP32/ESP32-S2 RX Timeout it is necessary to fix the UART Clock Source to APB.

   In the case of the other SoC, such as ESP32-S3, C3, C6, H2 and P4, there is no such RX Timeout limitation.
   Those will set the UART Source Clock as XTAL, which allows the baud rate to be high and it is steady, not
   changing with the CPU Frequency.
*/

#include <Arduino.h>

// There are two ways to make this sketch work:
// By physically connecting the pins 4 and 5 and then create a physical UART loopback,
// Or by using the internal IO_MUX to connect the TX signal to the RX pin, creating the
// same loopback internally.
#define USE_INTERNAL_PIN_LOOPBACK 1  // 1 uses the internal loopback, 0 for wiring pins 4 and 5 externally

#define DATA_SIZE 10    // 10 bytes is lower than the default 120 bytes of RX FIFO FULL
#define BAUD      9600  // Any baudrate from 300 to 115200
#define TEST_UART 1     // Serial1 will be used for the loopback testing with different RX FIFO FULL values
#define RXPIN     4     // GPIO 4 => RX for Serial1
#define TXPIN     5     // GPIO 5 => TX for Serial1

uint8_t rxTimeoutTestCases[] = {50, 20, 10, 5, 1};

void setup() {
  // UART0 will be used to log information into Serial Monitor
  Serial.begin(115200);

  // UART1 will have its RX<->TX cross connected
  // GPIO4 <--> GPIO5 using external wire
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
  // UART_CLK_SRC_APB will allow higher values of RX Timeout
  // default for ESP32 and ESP32-S2 is REF_TICK which limits the RX Timeout to 1
  // setClockSource() must be called before begin()
  Serial1.setClockSource(UART_CLK_SRC_APB);
#endif
  Serial1.begin(BAUD, SERIAL_8N1, RXPIN, TXPIN);  // Rx = 4, Tx = 5 will work for ESP32, S2, S3 and C3
#if USE_INTERNAL_PIN_LOOPBACK
  uart_internal_loopback(TEST_UART, RXPIN);
#endif

  for (uint8_t i = 0; i < sizeof(rxTimeoutTestCases); i++) {
    Serial.printf("\n\n================================\nTest Case #%d\n================================\n", i + 1);
    testAndReport(rxTimeoutTestCases[i]);
  }
}

void loop() {}

void testAndReport(uint8_t rxTimeout) {
  // Let's send 10 bytes from Serial1 rx<->tx and mesaure time using different Rx Timeout configurations
  uint8_t bytesReceived = 0;
  uint8_t dataSent[DATA_SIZE], dataReceived[DATA_SIZE];
  uint8_t i;
  // initialize all data
  for (i = 0; i < DATA_SIZE; i++) {
    dataSent[i] = '0' + (i % 10);  // fill it with a repeated sequence of 0..9 characters
    dataReceived[i] = 0;
  }

  Serial.printf("Testing the time for receiving %d bytes at %d baud, using RX Timeout = %d:", DATA_SIZE, BAUD, rxTimeout);
  Serial.flush();                   // wait Serial FIFO to be empty and then spend almost no time processing it
  Serial1.setRxTimeout(rxTimeout);  // testing different results based on Rx Timeout setup
  // For baud rates lower or equal to 57600, ESP32 Arduino makes it get byte-by-byte from FIFO, thus we will change it here:
  Serial1.setRxFIFOFull(120);  // forces it to wait receiving 120 bytes in FIFO before making it available to Arduino

  size_t sentBytes = Serial1.write(dataSent, sizeof(dataSent));  // ESP32 TX FIFO is about 128 bytes, 10 bytes will fit fine
  uint32_t now = millis();
  while (bytesReceived < DATA_SIZE) {
    bytesReceived += Serial1.read(dataReceived, DATA_SIZE);
    // safety for array limit && timeout... in 5 seconds...
    if (millis() - now > 5000) {
      break;
    }
  }

  uint32_t pastTime = millis() - now;  // codespell:ignore pasttime
  Serial.printf("\nIt has sent %d bytes from Serial1 TX to Serial1 RX\n", sentBytes);
  Serial.printf("It took %lu milliseconds to read %d bytes\n", pastTime, bytesReceived);  // codespell:ignore pasttime
  Serial.print("Received data: [");
  Serial.write(dataReceived, DATA_SIZE);
  Serial.println("]");
  Serial.println("========================\nFinished!");
}
