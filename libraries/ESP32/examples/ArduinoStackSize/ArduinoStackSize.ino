/*
    ESP32 Arduino creates a task to run setup() and then to execute loop() continuously
    This task can be found at https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/main.cpp

    By default "loopTask" will be created with a stack size of 8KB.
    This should be plenty for most general sketches.

    There is a way to change the stack size of this task by using
    SET_LOOP_TASK_STACK_SIZE(size);
    It will bypass the default stack size of 8KB and allow the user to define a new size.

    It is recommend this value to be higher than 8KB, for instance 16KB.
    This increasing may be necessary for the sketches that use deep recursion for instance.

    In this example, you can verify it by changing or just commenting out SET_LOOP_TASK_STACK_SIZE();
*/

// This sets Arduino Stack Size - comment this line to use default 8K stack size
SET_LOOP_TASK_STACK_SIZE(16 * 1024);  // 16KB

void setup() {
  Serial.begin(115200);

  Serial.printf("Arduino Stack was set to %d bytes", getArduinoLoopTaskStackSize());

  // Print unused stack for the task that is running setup()
  Serial.printf("\nSetup() - Free Stack Space: %d", uxTaskGetStackHighWaterMark(NULL));
}

void loop() {
  delay(1000);

  // Print unused stack for the task that is running loop() - the same as for setup()
  Serial.printf("\nLoop() - Free Stack Space: %d", uxTaskGetStackHighWaterMark(NULL));
}
