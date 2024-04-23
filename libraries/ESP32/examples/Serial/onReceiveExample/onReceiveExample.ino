/*

   This Sketch demonstrates how to use onReceive(callbackFunc) with HardwareSerial

   void HardwareSerial::onReceive(OnReceiveCb function, bool onlyOnTimeout = false)

   It is possible to register an UART callback function that will be called
   every time that UART receives data and an associated interrupt is generated.

   In summary, HardwareSerial::onReceive() works like an RX Interrupt callback, that can be adjusted
   using HardwareSerial::setRxFIFOFull() and HardwareSerial::setRxTimeout().

   OnReceive will be called, while receiving a stream of data, when every 120 bytes are received (default FIFO Full),
   which may not help in case that the application needs to get all data at once before processing it.
   Therefore, a way to make it work is by detecting the end of a stream transmission. This can be based on a protocol
   or based on timeout with the UART line in idle (no data received - this is the case of this example).

   In some cases, it is necessary to wait for receiving all the data before processing it and parsing the
   UART input. This example demonstrates a way to create a String with all data received from UART0 and
   signaling it using a Mutex for another task to process it. This example uses a timeout of 500ms as a way to
   know when the reception of data has finished.

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
*/

// this will make UART0 work in any case (using or not USB)
#if ARDUINO_USB_CDC_ON_BOOT
#define UART0 Serial0
#else
#define UART0 Serial
#endif

// global variable to keep the results from onReceive()
String uart_buffer = "";
// a pause of a half second in the UART transmission is considered the end of transmission.
const uint32_t communicationTimeout_ms = 500;

// Create a mutex for the access to uart_buffer
// only one task can read/write it at a certain time
SemaphoreHandle_t uart_buffer_Mutex = NULL;

// UART_RX_IRQ will be executed as soon as data is received by the UART
// This is a callback function executed from a high priority
// task created when onReceive() is used
void UART0_RX_CB() {
  // take the mutex, waits forever until loop() finishes its processing
  if (xSemaphoreTake(uart_buffer_Mutex, portMAX_DELAY)) {
    uint32_t now = millis();  // tracks timeout
    while ((millis() - now) < communicationTimeout_ms) {
      if (UART0.available()) {
        uart_buffer += (char)UART0.read();
        now = millis();  // reset the timer
      }
    }
    // releases the mutex for data processing
    xSemaphoreGive(uart_buffer_Mutex);
  }
}

// setup() and loop() are functions executed by a low priority task
// Therefore, there are 2 tasks running when using onReceive()
void setup() {
  UART0.begin(115200);

  // creates a mutex object to control access to uart_buffer
  uart_buffer_Mutex = xSemaphoreCreateMutex();
  if (uart_buffer_Mutex == NULL) {
    log_e("Error creating Mutex. Sketch will fail.");
    while (true) {
      UART0.println("Mutex error (NULL). Program halted.");
      delay(2000);
    }
  }

  UART0.onReceive(UART0_RX_CB);  // sets the callback function
  UART0.println("Send data to UART0 in order to activate the RX callback");
}

uint32_t counter = 0;
void loop() {
  if (uart_buffer.length() > 0) {
    // signals that the onReceive function shall not change uart_buffer while processing
    if (xSemaphoreTake(uart_buffer_Mutex, portMAX_DELAY)) {
      // process the received data from UART0 - example, just print it beside a counter
      UART0.print("[");
      UART0.print(counter++);
      UART0.print("] [");
      UART0.print(uart_buffer.length());
      UART0.print(" bytes] ");
      UART0.println(uart_buffer);
      uart_buffer = "";  // reset uart_buffer for the next UART reading
      // releases the mutex for more data to be received
      xSemaphoreGive(uart_buffer_Mutex);
    }
  }
  UART0.println("Sleeping for 1 second...");
  delay(1000);
}
