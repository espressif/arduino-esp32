/*

   This Sketch demonstrates how to use onReceive(callbackFunc) with HardwareSerial

   void HardwareSerial::onReceive(OnReceiveCb function, bool onlyOnTimeout = false)

   It is possible to register a UART callback function that will be called
   everytime that UART receives data and an associated interrupt is generated.

   The receiving data interrupt can occur because of two possible events:

   1- UART FIFO FULL: it happens when internal UART FIFO reaches a certain number of bytes.
      Its full capacity is 127 bytes. The FIFO Full threshold for the interrupt can be changed
      using HardwareSerial::setRxFIFOFull(uint8_t fifoFull).
      Default FIFO Full Threshold is set at the UART initialzation using HardwareSerial::begin()
      This will depend on the baud rate set with when begin() is executed.
      For a baudrate of 115200 or lower, it it just 1 byte, mimicking original Arduino UART driver.
      For a baudrate over 115200 it will be 120 bytes for higher performance.
      Anyway it can be changed by the application at anytime.

   2- UART RX Timeout: it happens, based on a timeout equivalent to a number of symbols at
      the current baud rate. If the UART line is idle for this timeout, it will raise an interrupt.
      This time can be changed by HardwareSerial::setRxTimeout(uint8_t rxTimeout)

   When any of those two interrupts occur, IDF UART driver will copy FIFO data to its internal
   RingBuffer and then Arduino can read such data. At the same time, Arduino Layer will execute
   the callback function defined with HardwareSerial::onReceive().

   <bool onlyOnTimeout> parameter (default false) can be used by the application to tell Arduino to
   only execute the callback when the second event above happens (Rx Timeout). At this time all
   received data will be available to be read by the Arduino application. But if the number of
   received bytes is higher than the FIFO space, it will generate an error of FIFO overflow.
   In order to avoid such problem, the application shall set an appropriate RX buffer size using
   HardwareSerial::setRxBufferSize(size_t new_size) before executing begin() for the Serial port.

   In summary, HardwareSerial::onReceive() works like an RX Interrupt callback, that can be adjusted
   using HardwareSerial::setRxFIFOFull() and HardwareSerial::setRxTimeout().

*/

#include <Arduino.h>

// There are two ways to make this sketch work:
// By physically connecting the pins 4 and 5 and then create a physical UART loopback,
// Or by using the internal IO_MUX to connect the TX signal to the RX pin, creating the
// same loopback internally.
#define USE_INTERNAL_PIN_LOOPBACK 1   // 1 uses the internal loopback, 0 for wiring pins 4 and 5 externally

#define DATA_SIZE 26    // 26 bytes is a lower than RX FIFO size (127 bytes) 
#define BAUD 9600       // Any baudrate from 300 to 115200
#define TEST_UART 1     // Serial1 will be used for the loopback testing with different RX FIFO FULL values
#define RXPIN 4         // GPIO 4 => RX for Serial1
#define TXPIN 5         // GPIO 5 => TX for Serial1

uint8_t fifoFullTestCases[] = {120, 20, 5, 1};
// volatile declaration will avoid any compiler optimization when reading variable values
volatile size_t sent_bytes = 0, received_bytes = 0;


void onReceiveFunction(void) {
  // This is a callback function that will be activated on UART RX events
  size_t available = Serial1.available();
  received_bytes += available;
  Serial.printf("onReceive Callback:: There are %d bytes available: ", available);
  while (available --) {
    Serial.print((char)Serial1.read());
  }
  Serial.println();
}

void setup() {
  // UART0 will be used to log information into Serial Monitor
  Serial.begin(115200);

  // UART1 will have its RX<->TX cross connected
  // GPIO4 <--> GPIO5 using external wire
  Serial1.begin(BAUD, SERIAL_8N1, RXPIN, TXPIN); // Rx = 4, Tx = 5 will work for ESP32, S2, S3 and C3
#if USE_INTERNAL_PIN_LOOPBACK
  uart_internal_loopback(TEST_UART, RXPIN);
#endif


  for (uint8_t i = 0; i < sizeof(fifoFullTestCases); i++) {
    Serial.printf("\n\n================================\nTest Case #%d\n================================\n", i + 1);
    // onReceive callback will be called on FIFO Full and RX timeout - default behaviour
    testAndReport(fifoFullTestCases[i], false);
  }

  Serial.printf("\n\n================================\nTest Case #6\n================================\n");
  // onReceive callback will be called just on RX timeout - using onlyOnTimeout = true
  // FIFO Full parameter (5 bytes) won't matter for the execution of this test case
  // because onReceive() uses only RX Timeout to be activated
  testAndReport(5, true);
}

void loop() {
}

void testAndReport(uint8_t fifoFull, bool onlyOnTimeOut) {
  // Let's send 125 bytes from Serial1 rx<->tx and mesaure time using diferent FIFO Full configurations
  received_bytes = 0;
  sent_bytes = DATA_SIZE;  // 26 characters

  uint8_t dataSent[DATA_SIZE + 1];
  dataSent[DATA_SIZE] = '\0';  // string null terminator, for easy printing.

  // initialize all data
  for (uint8_t i = 0; i < DATA_SIZE; i++) {
    dataSent[i] = 'A' + i; // fill it with characters A..Z
  }

  Serial.printf("\nTesting onReceive for receiving %d bytes at %d baud, using RX FIFO Full = %d.\n", sent_bytes, BAUD, fifoFull);
  if (onlyOnTimeOut) {
    Serial.println("onReceive is called just on RX Timeout!");
  } else {
    Serial.println("onReceive is called on both FIFO Full and RX Timeout events.");
  }
  Serial.flush(); // wait Serial FIFO to be empty and then spend almost no time processing it
  Serial1.setRxFIFOFull(fifoFull); // testing diferent result based on FIFO Full setup
  Serial1.onReceive(onReceiveFunction, onlyOnTimeOut); // sets a RX callback function for Serial 1

  sent_bytes = Serial1.write(dataSent, DATA_SIZE); // ESP32 TX FIFO is about 128 bytes, 125 bytes will fit fine
  Serial.printf("\nSent String: %s\n", dataSent);
  while (received_bytes < sent_bytes) {
    // just wait for receiving all byte in the callback...
  }

  Serial.printf("\nIt has sent %d bytes from Serial1 TX to Serial1 RX\n", sent_bytes);
  Serial.printf("onReceive() has read a total of %d bytes\n", received_bytes);
  Serial.println("========================\nFinished!");

  Serial1.onReceive(NULL); // resets/disables the RX callback function for Serial 1
}
