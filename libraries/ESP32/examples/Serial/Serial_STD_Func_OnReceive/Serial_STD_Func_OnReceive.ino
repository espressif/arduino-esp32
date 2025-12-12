/*
 * This is C++ example that demonstrates the usage of a std::function as OnReceive Callback function to all the UARTs
 * It basically defines a general onReceive function that receives an extra parameter, the Serial pointer that is
 * executing the callback.
 *
 * For each HardwareSerial object (Serial, Serial1, Serial2), it is necessary to set the callback with
 * the respective Serial pointer. It is done using lambda expression as a std::function.
 * Example:
 * Serial1.onReceive([]() { processOnReceiving(&Serial1); });
 *
 */

// soc/soc_caps.h has information about each SoC target
// in this example, we use SOC_UART_HP_NUM that goes from 1 to 3,
// depending on the number of available UARTs in the ESP32xx
// This makes the code transparent to what SoC is used.
#include "soc/soc_caps.h"

// This example shall use UART1 or UART2 for testing and UART0 for console messages
// If UART0 is used for testing, it is necessary to manually send data to it, using the Serial Monitor/Terminal
// In case that USB CDC is available, it may be used as console for messages.
#define TEST_UART 1  // Serial# (0, 1 or 2) will be used for the loopback
#define RXPIN     4  // GPIO 4 => RX for Serial1 or Serial2
#define TXPIN     5  // GPIO 5 => TX for Serial1 or Serial2

// declare testingSerial (as reference) related to TEST_UART number defined above (only for Serial1 and Serial2)
#if SOC_UART_HP_NUM > 1 && TEST_UART == 1
HardwareSerial &testingSerial = Serial1;
#elif SOC_UART_HP_NUM > 2 && TEST_UART == 2
HardwareSerial &testingSerial = Serial2;
#endif

// General callback function for any UART -- used with a lambda std::function within HardwareSerial::onReceive()
void processOnReceiving(HardwareSerial &mySerial) {
  // detects which Serial# is being used here
  int8_t uart_num = -1;
  if (&mySerial == &Serial0) {
    uart_num = 0;
#if SOC_UART_HP_NUM > 1
  } else if (&mySerial == &Serial1) {
    uart_num = 1;
#endif
#if SOC_UART_HP_NUM > 2
  } else if (&mySerial == &Serial2) {
    uart_num = 2;
#endif
  }

  //Prints some information on the current Serial (UART0 or USB CDC)
  if (uart_num == -1) {
    Serial.println("This is not a know Arduino Serial# object...");
    return;
  }
  Serial.printf("\nOnReceive Callback --> Received Data from UART%d\n", uart_num);
  Serial.printf("Received %d bytes\n", mySerial.available());
  Serial.printf("First byte is '%c' [0x%02x]\n", mySerial.peek(), mySerial.peek());
  uint8_t charPerLine = 0;
  while (mySerial.available()) {
    char c = mySerial.read();
    Serial.printf("'%c' [0x%02x] ", c, c);
    if (++charPerLine == 10) {
      charPerLine = 0;
      Serial.println();
    }
  }
}

void setup() {
  // Serial can be the USB or UART0, depending on the settings and which SoC is used
  Serial.begin(115200);

  // when data is received from UART0, it will call the general function
  // passing Serial0 as parameter for processing
#if TEST_UART == 0
  Serial0.begin(115200);  // keeps default GPIOs
  Serial0.onReceive([]() {
    processOnReceiving(Serial0);
  });
#else
  // and so on for the other UARTs (Serial1 and Serial2)
  // Rx = 4, Tx = 5 will work for ESP32, S2, S3, C3, C6 and H2
  testingSerial.begin(115200, SERIAL_8N1, RXPIN, TXPIN);
  testingSerial.onReceive([]() {
    processOnReceiving(testingSerial);
  });
#endif

  // this helper function will connect TX pin (from TEST_UART number) to its RX pin
  // creating a loopback that will allow to write to TEST_UART number
  // and send it to RX with no need to physically connect both pins
#if TEST_UART > 0
  uart_internal_loopback(TEST_UART, RXPIN);
#else
  // when UART0 is used for testing, it is necessary to send data using the Serial Monitor/Terminal
  // Data must be sent by the CP2102, manually using  the Serial Monitor/Terminal
#endif

  delay(500);
  Serial.printf("\nSend bytes to UART%d in order to\n", TEST_UART);
  Serial.println("see a single processing function display information about");
  Serial.println("the received data.\n");
}

void loop() {
  // All done by the UART callback functions
  // just write a random number of bytes into the testing UART
  char serial_data[24];
  size_t len = random(sizeof(serial_data) - 1) + 1;  // at least 1 byte will be sent
  for (uint8_t i = 0; i < len; i++) {
    serial_data[i] = 'A' + i;
  }

#if TEST_UART > 0
  Serial.println("\n\n==================================");
  Serial.printf("Sending %d bytes to UART%d...\n", len, TEST_UART);
  testingSerial.write(serial_data, len);
#else
  // when UART0 is used for testing, it is necessary to send data using the Serial Monitor/Terminal
  Serial.println("Use the Serial Monitor/Terminal to send data to UART0");
#endif
  Serial.println("pausing for 15 seconds.");
  delay(15000);
}
