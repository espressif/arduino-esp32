/* Basic Multi Threading Arduino Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
// Please read file README.md in the folder containing this example./*

#define MAX_LINE_LENGTH (64)

// Define two tasks for reading and writing from and to the serial port.
void TaskWriteToSerial(void *pvParameters);
void TaskReadFromSerial(void *pvParameters);

// Define Queue handle
QueueHandle_t QueueHandle;
const int QueueElementSize = 10;
typedef struct {
  char line[MAX_LINE_LENGTH];
  uint8_t line_length;
} message_t;

// The setup function runs once when you press reset or power on the board.
void setup() {
  // Initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  // Create the queue which will have <QueueElementSize> number of elements, each of size `message_t` and pass the address to <QueueHandle>.
  QueueHandle = xQueueCreate(QueueElementSize, sizeof(message_t));

  // Check if the queue was successfully created
  if (QueueHandle == NULL) {
    Serial.println("Queue could not be created. Halt.");
    while (1) {
      delay(1000);  // Halt at this point as is not possible to continue
    }
  }

  // Set up two tasks to run independently.
  xTaskCreate(
    TaskWriteToSerial, "Task Write To Serial"  // A name just for humans
    ,
    2048  // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);`
    ,
    NULL  // No parameter is used
    ,
    2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,
    NULL  // Task handle is not used here
  );

  xTaskCreate(
    TaskReadFromSerial, "Task Read From Serial", 2048  // Stack size
    ,
    NULL  // No parameter is used
    ,
    1  // Priority
    ,
    NULL  // Task handle is not used here
  );

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
  Serial.printf(
    "\nAnything you write will return as echo.\nMaximum line length is %d characters (+ terminating '0').\nAnything longer will be sent as a separate "
    "line.\n\n",
    MAX_LINE_LENGTH - 1
  );
}

void loop() {
  // Loop is free to do any other work

  delay(1000);  // While not being used yield the CPU to other tasks
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskWriteToSerial(void *pvParameters) {  // This is a task.
  message_t message;
  for (;;) {  // A Task shall never return or exit.
    // One approach would be to poll the function (uxQueueMessagesWaiting(QueueHandle) and call delay if nothing is waiting.
    // The other approach is to use infinite time to wait defined by constant `portMAX_DELAY`:
    if (QueueHandle != NULL) {  // Sanity check just to make sure the queue actually exists
      int ret = xQueueReceive(QueueHandle, &message, portMAX_DELAY);
      if (ret == pdPASS) {
        // The message was successfully received - send it back to Serial port and "Echo: "
        Serial.printf("Echo line of size %d: \"%s\"\n", message.line_length, message.line);
        // The item is queued by copy, not by reference, so lets free the buffer after use.
      } else if (ret == pdFALSE) {
        Serial.println("The `TaskWriteToSerial` was unable to receive data from the Queue");
      }
    }  // Sanity check
  }  // Infinite loop
}

void TaskReadFromSerial(void *pvParameters) {  // This is a task.
  message_t message;
  for (;;) {
    // Check if any data are waiting in the Serial buffer
    message.line_length = Serial.available();
    if (message.line_length > 0) {
      // Check if the queue exists AND if there is any free space in the queue
      if (QueueHandle != NULL && uxQueueSpacesAvailable(QueueHandle) > 0) {
        int max_length = message.line_length < MAX_LINE_LENGTH ? message.line_length : MAX_LINE_LENGTH - 1;
        for (int i = 0; i < max_length; ++i) {
          message.line[i] = Serial.read();
        }
        message.line_length = max_length;
        message.line[message.line_length] = 0;  // Add the terminating nul char

        // The line needs to be passed as pointer to void.
        // The last parameter states how many milliseconds should wait (keep trying to send) if is not possible to send right away.
        // When the wait parameter is 0 it will not wait and if the send is not possible the function will return errQUEUE_FULL
        int ret = xQueueSend(QueueHandle, (void *)&message, 0);
        if (ret == pdTRUE) {
          // The message was successfully sent.
        } else if (ret == errQUEUE_FULL) {
          // Since we are checking uxQueueSpacesAvailable this should not occur, however if more than one task should
          //   write into the same queue it can fill-up between the test and actual send attempt
          Serial.println("The `TaskReadFromSerial` was unable to send data into the Queue");
        }  // Queue send check
      }  // Queue sanity check
    } else {
      delay(100);  // Allow other tasks to run when there is nothing to read
    }  // Serial buffer check
  }  // Infinite loop
}
