/*

   This Sketch demonstrates how to use onReceiveError(callbackFunc) with HardwareSerial

   void HardwareSerial::onReceiveError(OnReceiveErrorCb function)

   It is possible to register a UART callback function that will be called
   everytime that UART detects an error which is also associated to an interrupt.

   There are some possible UART errors:

    UART_BREAK_ERROR - when a BREAK event is detected in the UART line. In that case, a BREAK may
    be read as one or more bytes ZERO as part of the data received by the UART peripheral.

    UART_BUFFER_FULL_ERROR - When the RX UART buffer is full. By default, Arduino will allocate a 256 bytes
    RX buffer. As data is received, it is copied to the UART driver buffer, but when it is full and data can't
    be copied anymore, this Error is issued. To prevent it the application can use
    HardwareSerial::setRxBufferSize(size_t new_size), before using HardwareSerial::begin()

    UART_FIFO_OVF_ERROR - When the UART peripheral RX FIFO is full and data is still arriving, this error is issued.
    The UART driver will stash RX FIFO and the data will be lost. In order to prevent, the application shall set a
    good buffer size using HardwareSerial::setRxBufferSize(size_t new_size), before using HardwareSerial::begin()

    UART_FRAME_ERROR - When the UART peripheral detects a UART frame error, this error is issued. It may happen because
    of line noise or bad impiedance.

    UART_PARITY_ERROR - When the UART peripheral detects a parity bit error, this error will be issued.


   In summary, HardwareSerial::onReceiveError() works like an UART Error Notification callback.

   Errors have priority in the order of the callbacks, therefore, as soon as an error is detected,
   the registered callback is executed firt, and only after that, the OnReceive() registered
   callback function will be executed. This will give opportunity for the Application to take action
   before reading data, if necessary.

   In long UART transmissions, some data will be received based on FIFO Full parameter, and whenever
   an error ocurs, it will raise the UART error interrupt.

   This sketch produces BREAK UART error in the begining of a transmission and also at the end of a
   transmission. It will be possible to understand the order of the events in the logs.

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

#define BREAK_BEFORE_MSG 0
#define BREAK_AT_END_MSG 1


uint8_t fifoFullTestCases[] = {120, 20, 5, 1};
// volatile declaration will avoid any compiler optimization when reading variable values
volatile size_t sent_bytes = 0, received_bytes = 0;

const char *uartErrorStrings[] = {
  "UART_NO_ERROR",
  "UART_BREAK_ERROR",
  "UART_BUFFER_FULL_ERROR",
  "UART_FIFO_OVF_ERROR",
  "UART_FRAME_ERROR",
  "UART_PARITY_ERROR"
};

// Callback function that will treat the UART errors
void onReceiveErrorFunction(hardwareSerial_error_t err) {
  // This is a callback function that will be activated on UART RX Error Events
  Serial.printf("\n-- onReceiveError [ERR#%d:%s] \n", err, uartErrorStrings[err]);
  Serial.printf("-- onReceiveError:: There are %d bytes available.\n", Serial1.available());
}

// Callback function that will deal with arriving UART data
void onReceiveFunction() {
  // This is a callback function that will be activated on UART RX events
  size_t available = Serial1.available();
  received_bytes += available;
  Serial.printf("onReceive Callback:: There are %d bytes available: {", available);
  while (available --) {
    Serial.print((char)Serial1.read());
  }
  Serial.println("}");
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
    Serial.printf("\n\n================================\nTest Case #%d BREAK at END\n================================\n", i + 1);
    // First sending BREAK at the end of the UART data transmission
    testAndReport(fifoFullTestCases[i], BREAK_AT_END_MSG);
    Serial.printf("\n\n================================\nTest Case #%d BREAK at BEGINING\n================================\n", i + 1);
    // Now sending BREAK at the begining of the UART data transmission
    testAndReport(fifoFullTestCases[i], BREAK_BEFORE_MSG);
    Serial.println("========================\nFinished!");
  }

}

void loop() {
}

void testAndReport(uint8_t fifoFull, bool break_at_the_end) {
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
  Serial.println("onReceive is called on both FIFO Full and RX Timeout events.");
  if (break_at_the_end) {
    Serial.printf("BREAK event will be sent at the END of the %d bytes\n", sent_bytes);
  } else {
    Serial.printf("BREAK event will be sent at the BEGINING of the %d bytes\n", sent_bytes);
  }
  Serial.flush(); // wait Serial FIFO to be empty and then spend almost no time processing it
  Serial1.setRxFIFOFull(fifoFull); // testing diferent result based on FIFO Full setup
  Serial1.onReceive(onReceiveFunction); // sets a RX callback function for Serial 1
  Serial1.onReceiveError(onReceiveErrorFunction); // sets a RX callback function for Serial 1

  if (break_at_the_end) {
    sent_bytes = uart_send_msg_with_break(TEST_UART, dataSent, DATA_SIZE);
  } else {
    uart_send_break(TEST_UART);
    sent_bytes = Serial1.write(dataSent, DATA_SIZE);
  }

  Serial.printf("\nSent String: %s\n", dataSent);
  while (received_bytes < sent_bytes) {
    // just wait for receiving all byte in the callback...
  }

  Serial.printf("\nIt has sent %d bytes from Serial1 TX to Serial1 RX\n", sent_bytes);
  Serial.printf("onReceive() has read a total of %d bytes\n", received_bytes);

  Serial1.onReceiveError(NULL); // resets/disables the RX Error callback function for Serial 1
  Serial1.onReceive(NULL); // resets/disables the RX callback function for Serial 1
}
