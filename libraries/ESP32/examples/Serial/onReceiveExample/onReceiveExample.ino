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
    uint32_t now = millis(); // tracks timeout
    while ((millis() - now) < communicationTimeout_ms) {
      if (UART0.available()) {
        uart_buffer += (char) UART0.read();
        now = millis();  // reset the timer
      }
    }
    // releases the mutex for data processing
    xSemaphoreGive(uart_buffer_Mutex);
  }
}

// setup() and loop()are callback functions executed by a low priority task
// Therefore, there are 2 tasks running when using onReceive()
void setup() {
  // creates a mutex object to control access to uart_buffer
  uart_buffer_Mutex = xSemaphoreCreateMutex();

  UART0.begin(115200);
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
    }
    // releases the mutex for more data to be received
    xSemaphoreGive(uart_buffer_Mutex);
  }
  UART0.println("Sleeping for 1 second...");
  delay(1000);
}
