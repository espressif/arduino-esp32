/*

   This Sketch demonstrates how to use onReceive(callbackFunc) with HardwareSerial

   void HardwareSerial::onReceive(OnReceiveCb function, bool onlyOnTimeout = false)

   It is possible to register an UART callback function that will be called
   every time that UART receives data and an associated UART interrupt is generated.

   In summary, HardwareSerial::onReceive() works like an RX Interrupt callback, that
   can be adjusted using HardwareSerial::setRxFIFOFull() and HardwareSerial::setRxTimeout().

   In case that <onlyOnTimeout> is not changed or it is set to <false>, the callback function is
   executed whenever any event happens first (FIFO Full or RX Timeout).
   OnReceive will be called when every 120 bytes are received(default FIFO Full),
   or when RX Timeout occurs after 1 UART symbol by default.

   This example demonstrates a way to create a String with all data received from UART0 only
   after RX Timeout. This example uses an RX timeout of about 3.5 Symbols as a way to know
   when the reception of data has finished.
   In order to achieve it, the sketch sets <onlyOnTimeout> to <true>.

   The onReceive() callback is called whenever the RX ISR is triggered.
   It can occur because of two possible events:

   1- UART FIFO FULL: it happens when internal UART FIFO reaches a certain number of bytes.
      Its full capacity is 127 bytes. The FIFO Full threshold for the interrupt can be changed
      using HardwareSerial::setRxFIFOFull(uint8_t fifoFull).
      Default FIFO Full Threshold is set in the UART initialization using HardwareSerial::begin()
      This will depend on the baud rate used when begin() is executed.
      For a baud rate of 115200 or lower, it it just 1 byte, mimicking original Arduino UART driver.
      For a baud rate over 115200 it will be 120 bytes for higher performance.
      Anyway, it can be changed by the application at any time.

   2- UART RX Timeout: it happens, based on a timeout equivalent to a number of symbols at
      the current baud rate. If the UART line is idle for this timeout, it will raise an interrupt.
      This time can be changed by HardwareSerial::setRxTimeout(uint8_t rxTimeout).
      <rxTimeout> is bound to the clock source.
      In order to use it properly, ESP32 and ESP32-S2 shall set the UART Clock Source to APB.

   When any of those two interrupts occur, IDF UART driver will copy FIFO data to its internal
   RingBuffer and then Arduino can read such data. At the same time, Arduino Layer will execute
   the callback function defined with HardwareSerial::onReceive().

   <bool onlyOnTimeout> parameter can be used by the application to tell Arduino to only execute
   the callback when Rx Timeout happens, by setting it to <true>.
   At this time all received data will be available to be read by the Arduino application.
   The application shall set an appropriate RX buffer size using
   HardwareSerial::setRxBufferSize(size_t new_size) before executing begin() for the Serial port.

   MODBUS timeout of 3.5 symbol is based on these documents:
   https://www.automation.com/en-us/articles/2012-1/introduction-to-modbus
   https://minimalmodbus.readthedocs.io/en/stable/serialcommunication.html
*/

// global variable to keep the results from onReceive()
String uart_buffer = "";
// The Modbus RTU standard prescribes a silent period corresponding to 3.5 characters between each
// message, to be able to figure out where one message ends and the next one starts.
const uint32_t modbusRxTimeoutLimit = 4;
const uint32_t baudrate = 19200;

// UART_RX_IRQ will be executed as soon as data is received by the UART and an RX Timeout occurs
// This is a callback function executed from a high priority monitor task
// All data will be buffered into RX Buffer, which may have its size set to whatever necessary
void UART0_RX_CB() {
  while (Serial0.available()) {
    uart_buffer += (char)Serial0.read();
  }
}

// setup() and loop() are functions executed by a low priority task
// Therefore, there are 2 tasks running when using onReceive()
void setup() {
  // Using Serial0 will work in any case (using or not USB CDC on Boot)
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
  // UART_CLK_SRC_APB will allow higher values of RX Timeout
  // default for ESP32 and ESP32-S2 is REF_TICK which limits the RX Timeout to 1
  // setClockSource() must be called before begin()
  Serial0.setClockSource(UART_CLK_SRC_APB);
#endif
  // the amount of data received or waiting to be proessed shall not exceed this limit of 1024 bytes
  Serial0.setRxBufferSize(1024);  // default is 256 bytes
  Serial0.begin(baudrate);        // default pins and default mode 8N1 (8 bits data, no parity bit, 1 stopbit)
  // set RX Timeout based on UART symbols ~ 3.5 symbols of 11 bits (MODBUS standard) ~= 2 ms at 19200
  Serial0.setRxTimeout(modbusRxTimeoutLimit);  // 4 symbols at 19200 8N1 is about 2.08 ms (40 bits)
  // sets the callback function that will be executed only after RX Timeout
  Serial0.onReceive(UART0_RX_CB, true);
  Serial0.println("Send data using Serial Monitor in order to activate the RX callback");
}

uint32_t counter = 0;
void loop() {
  // String <uart_buffer> is filled by the UART Callback whenever data is received and RX Timeout occurs
  if (uart_buffer.length() > 0) {
    // process the received data from Serial - example, just print it beside a counter
    Serial0.print("[");
    Serial0.print(counter++);
    Serial0.print("] [");
    Serial0.print(uart_buffer.length());
    Serial0.print(" bytes] ");
    Serial0.println(uart_buffer);
    uart_buffer = "";  // reset uart_buffer for the next UART reading
  }
  delay(1);
}
