/*
  This example demonstrates basic usage of FreeRTOS Task for multi threading.
  Please refer to other examples in this folder to better utilize their the full potential and safe-guard potential problems.
  It is also advised to read documentation on FreeRTOS web pages:
  https://www.freertos.org/a00106.html

  This example will blink builtin LED and read analog data.
  Additionally this example demonstrates usage of task handle, simply by deleting the analog
  read task after 10 seconds from main loop by calling the function `vTaskDelete`.

  Theory:
  A task is simply a function which runs when the operating system (FreeeRTOS) sees fit.
  This task can have an infinite loop inside if you want to do some work periodically for the entirety of the program run.
  This however can create a problem - no other task will ever run and also the Watch Dog will trigger and your program will restart.
  A nice behaving tasks knows when it useless to keep the processor for itself and gives it away for other tasks to be used.
  This can be achieved by many ways, but the simplest is calling `delay(milliseconds)`.
  During that delay any other task may run and do it's job.
  When the delay runs out the Operating System gives the processor to the task which can continue.
  For other ways to yield the CPU in task please see other examples in this folder.
*/

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

// Define two tasks for Blink & AnalogRead.
void TaskBlink( void *pvParameters );
void TaskAnalogReadA3( void *pvParameters );
TaskHandle_t task_handle; // You can (don't have to) use this to be able to manipulate a task from somewhere else.

// The setup function runs once when you press reset or power on the board.
void setup() {
  // Initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  
  // Set up two tasks to run independently.
  uint32_t blink_delay = 1000; // Delay between changing state on LED pin
  xTaskCreate(
    TaskBlink
    ,  "TaskBlink" // A name just for humans
    ,  1024        // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);`
    ,  (void*) &blink_delay // Task parameter which can modify the task behavior. This must be passed as pointer to void.
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL // Task handle is not used here
    );

  // This variant of task creation can also for the task to run specified core
  xTaskCreatePinnedToCore(
    TaskAnalogReadA3
    ,  "AnalogReadA3"
    ,  1024  // Stack size
    ,  NULL  // When no parameter is used, simply pass NULL
    ,  1  // Priority
    ,  task_handle // With task handle we will be able to manipulate with this task.
    ,  ARDUINO_RUNNING_CORE // Core on which the task will run
    );

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
}

void loop(){
  if(task_handle != NULL){ // Make sure that the task actually exists
    delay(10000);
    vTaskDelete(task_handle); // Delete task
    task_handle = NULL; // prevent calling vTaskDelete on non-existing task
  }
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskBlink(void *pvParameters){  // This is a task.
  uint32_t blink_delay = (uint32_t) *pvParameters;

/*
  Blink
  Turns on an LED on for one second, then off for one second, repeatedly.
    
  If you want to know what pin the on-board LED is connected to on your ESP32 model, check
  the Technical Specs of your board.
*/

  // initialize digital LED_BUILTIN on pin 13 as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  for (;;){ // A Task shall never return or exit.
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    // arduino-esp32 has FreeRTOS configured to have a tick-rate of 1000Hz and portTICK_PERIOD_MS
    // refers to how many milliseconds the period between each ticks is, ie. 1ms.
    delay(blink_delay);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    delay(blink_delay);
  }
}

void TaskAnalogReadA3(void *pvParameters){  // This is a task.
  (void) pvParameters;
  
/*
  AnalogReadSerial
  Reads an analog input on pin A3, prints the result to the serial monitor.
  Graphical representation is available using serial plotter (Tools > Serial Plotter menu)
  Attach the center pin of a potentiometer to pin A3, and the outside pins to +5V and ground.

  This example code is in the public domain.
*/

  for (;;){
    // read the input on analog pin A3:
    int sensorValueA3 = analogRead(A3);
    // print out the value you read:
    Serial.println(sensorValueA3);
    delay(100); // 100ms delay
  }
}
