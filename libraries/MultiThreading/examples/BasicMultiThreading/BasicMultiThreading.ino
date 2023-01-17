// Please read file README.md in the folder containing this example.

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#define ANALOG_INPUT_PIN 27

#ifndef LED_BUILTIN
  #define LED_BUILTIN 13 // Specify the on which is your LED
#endif

// Define two tasks for Blink & AnalogRead.
void TaskBlink( void *pvParameters );
void TaskAnalogRead( void *pvParameters );
TaskHandle_t task_handle; // You can (don't have to) use this to be able to manipulate a task from somewhere else.

// The setup function runs once when you press reset or power on the board.
void setup() {
  // Initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  
  // Set up two tasks to run independently.
  uint32_t blink_delay = 1000; // Delay between changing state on LED pin
  xTaskCreate(
    TaskBlink
    ,  "Task Blink" // A name just for humans
    ,  1024        // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);`
    ,  (void*) &blink_delay // Task parameter which can modify the task behavior. This must be passed as pointer to void.
    ,  2  // Priority
    ,  NULL // Task handle is not used here - simply pass NULL
    );

  // This variant of task creation can also specify on which core it will be run (only relevant for multi-core ESPs)
  xTaskCreatePinnedToCore(
    TaskAnalogRead
    ,  "Analog Read"
    ,  1024  // Stack size
    ,  NULL  // When no parameter is used, simply pass NULL
    ,  1  // Priority
    ,  &task_handle // With task handle we will be able to manipulate with this task.
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
  uint32_t blink_delay = *((uint32_t*)pvParameters);

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

void TaskAnalogRead(void *pvParameters){  // This is a task.
  (void) pvParameters;
  
/*
  AnalogReadSerial
  Reads an analog input on pin A3, prints the result to the serial monitor.
  Graphical representation is available using serial plotter (Tools > Serial Plotter menu)
  Attach the center pin of a potentiometer to pin A3, and the outside pins to +5V and ground.

  This example code is in the public domain.
*/

  for (;;){
    // read the input on analog pin:
    int sensorValue = analogRead(ANALOG_INPUT_PIN);
    // print out the value you read:
    Serial.println(sensorValue);
    delay(100); // 100ms delay
  }
}
