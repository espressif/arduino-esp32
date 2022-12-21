/*
 * 
 * This Sketch demonstrates the effect of changing RX FIFO Full parameter into HardwareSerial Class 
 * Serial.setRxFIFOFull(byte) is used to change it.
 * By default, UART ISR will wait for 120 bytes to arrive into UART before making the data available
 * to be read by an Arduino Sketch. It may also release fewer bytes after an RX Timeout equivalent by 
 * default to 2 UART symbols. 
 * 
 * The way we demonstrate the effect of this parameter is by measuring the time the Sketch takes
 * to read data using Arduino HardwareSerial API.
 * 
 * The higher RX FIFO Full is, the lower consumption of the core to process and make the data available.
 * At the same time, it may take longer for the Sketch to be able to read it, because the data must first
 * populate RX UART FIFO.
 * 
 * The lower RX FIFO Full is, the higher consumption of the core to process and make the data available.
 * This is because the core will be interrupted often and it will copy data from the RX FIFO to the Arduino
 * internal buffer to be read by the sketch. By other hand, the data will be made available to the sketch
 * faster, in a close to byte by byte communication. 
 * 
 * Therefore, it allows the decision of the architecture to be designed by the developer.
 * Some application based on certain protocols may need the sketch to read the Serial Port byte by byte, for example.
 * 
 */

#include <Arduino.h>

// There are two ways to make this sketch work:
// By physically connecting the pins 4 and 5 and then create a physical UART loopback,
// Or by using the internal IO_MUX to connect the TX signal to the RX pin, creating the 
// same loopback internally.
#define USE_INTERNAL_PIN_LOOPBACK 1   // 1 uses the internal loopback, 0 for wiring pins 4 and 5 externally

#define DATA_SIZE 125   // 125 bytes is a bit higher than the default 120 bytes of RX FIFO FULL 
#define BAUD 9600       // Any baudrate from 300 to 115200
#define TEST_UART 1     // Serial1 will be used for the loopback testing with different RX FIFO FULL values
#define RXPIN 4         // GPIO 4 => RX for Serial1
#define TXPIN 5         // GPIO 5 => TX for Serial1

uint8_t fifoFullTestCases[] = {120, 20, 5, 1};

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
    testAndReport(fifoFullTestCases[i]);
  }

}

void loop() {
}

void testAndReport(uint8_t fifoFull) {
  // Let's send 125 bytes from Serial1 rx<->tx and mesaure time using diferent FIFO Full configurations
  uint8_t bytesReceived = 0;
  uint8_t dataSent[DATA_SIZE], dataReceived[DATA_SIZE];
  uint32_t timeStamp[DATA_SIZE], bytesJustReceived[DATA_SIZE];
  uint8_t i;
  // initialize all data
  for (i = 0; i < DATA_SIZE; i++) {
    dataSent[i] = '0' + (i % 10); // fill it with a repeated sequence of 0..9 characters
    dataReceived[i] = 0;
    timeStamp[i] = 0;
    bytesJustReceived[i] = 0;
  }

  Serial.printf("Testing the time for receiving %d bytes at %d baud, using RX FIFO Full = %d:", DATA_SIZE, BAUD, fifoFull);
  Serial.flush(); // wait Serial FIFO to be empty and then spend almost no time processing it
  Serial1.setRxFIFOFull(fifoFull); // testing diferent result based on FIFO Full setup

  size_t sentBytes = Serial1.write(dataSent, sizeof(dataSent)); // ESP32 TX FIFO is about 128 bytes, 125 bytes will fit fine
  uint32_t now = millis();
  i = 0;
  while (bytesReceived < DATA_SIZE) {
    bytesReceived += (bytesJustReceived[i] = Serial1.read(dataReceived + bytesReceived, DATA_SIZE));
    timeStamp[i] = millis();
    if (bytesJustReceived[i] > 0) i++; // next data only when we read something from Serial1
    // safety for array limit && timeout... in 5 seconds...
    if (i == DATA_SIZE || millis() - now > 5000) break;
  }

  uint32_t pastTime = millis() - now;
  Serial.printf("\nIt has sent %d bytes from Serial1 TX to Serial1 RX\n", sentBytes);
  Serial.printf("It took %d milliseconds to read %d bytes\n", pastTime, bytesReceived);
  Serial.printf("Per execution Serial.read() number of bytes data and time information:\n");
  for (i = 0; i < DATA_SIZE; i++) {
    Serial.printf("#%03d - Received %03d bytes after %d ms.\n", i, bytesJustReceived[i], i > 0 ? timeStamp[i] - timeStamp[i - 1] : timeStamp[i] - now);
    if (i != DATA_SIZE - 1 && bytesJustReceived[i + 1] == 0) break;
  }

  Serial.println("========================\nFinished!");
}

