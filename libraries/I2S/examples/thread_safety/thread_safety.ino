/*
  This example is only for ESP devices.

  This sketch verify I2S lib thread safety = when multiple threads are accessing same object
  it should not corrupt object state, lead to racing conditions nor crashing.

Hardware:
   1. Any ESP32 or ESP32-S2

 Steps to run:
 1. Select target board:
   Tools -> Board -> ESP32 Arduino -> your board
 2. Upload sketch
   Press upload button (arrow in top left corner)
   When you see in console line like this: "Connecting........_____.....__"
     If loading doesn't start automatically, you may need to press and hold Boot
       button and press EN button shortly. Now you can release both buttons.
     You should see lines like this: "Writing at 0x00010000... (12 %)" with rising percentage on each line.
     If this fails, try the board buttons right after pressing upload button, or reconnect the USB cable.
 3. Open monitor
   Tools -> Serial Monitor
 4. Observe
  No crash should occur

Created by Tomas Pilny
on 30th Sep 2021
*/

#include <I2S.h>
const int sampleRate = 16000; // sample rate in Hz
const int bitsPerSample = 16;
TaskHandle_t _Task1Handle;
TaskHandle_t _Task2Handle;
TaskHandle_t _Task3Handle;
TaskHandle_t _Task4Handle;
TaskHandle_t _Task5Handle;


void task1(void*){
  while(true){
    I2S.setAllPins(); // apply default pins
    if(I2S.available()){
      int foo = I2S.read();
    }
    delay(1);
  } // infinite loop
}

void task2(void*){
  while(true){
    I2S.setAllPins(5, 25, 26, 27);
    I2S.write(123);
    delay(1);
  } // infinite loop
}

void task3(void*){
  // Annoying kid: "What does this switch do?""
  while(true){
    I2S.end(); // flip
    delay(100);
    if (!I2S.begin(I2S_PHILIPS_MODE, 8000, 16)){ // flop
      log_e("Could not start I2S");
      while (1) {delay(100);} // halt
    }
    delay(100);
  } // infinite loop
}

void task4(void*){
  const int buf_size = 64;
  uint8_t buffer [buf_size];
  while(true){
    if(I2S.available() >= buf_size){
      int read = I2S.read(buffer, buf_size);
      if(read != buf_size){
        log_e("task4: did not read complete buffer");
      }
    }
    delay(5);
  } // infinite loop
}

void task5(void*){
  int i2s_buffer_size;
  while(true){
    i2s_buffer_size = I2S.getBufferSize();
    delay(1);
    if(!I2S.setBufferSize(i2s_buffer_size + 1)){
      // unsupported buffer size requested -> halve the size
      i2s_buffer_size /= 2;
    }
    delay(25);
  } // infinite loop
}

void setup() {
  // Open serial communications and wait for port to open:
  // A baud rate of 115200 is used instead of 9600 for a faster data rate
  // on non-native USB ports
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  if (!I2S.begin(I2S_PHILIPS_MODE, 8000, 16)){
    log_e("Could not start I2S");
    while (1) {delay(100);} // halt
  }

  _Task1Handle = NULL;
  xTaskCreate(
    task1, // Function to implement the task
    "task1", // Name of the task
    2000,  // Stack size in words
    NULL,  // Task input parameter
    1,  // Priority of the task
    &_Task1Handle  // Task handle.
    );
  if(_Task1Handle == NULL){
    log_e("Could not create callback task 1");
    while (1) {;} // halt
  }

  _Task2Handle = NULL;
  xTaskCreate(
    task2, // Function to implement the task
    "task2", // Name of the task
    2000,  // Stack size in words
    NULL,  // Task input parameter
    1,  // Priority of the task
    &_Task2Handle  // Task handle.
    );
  if(_Task2Handle == NULL){
    log_e("Could not create callback task 2");
    while (1) {;} // halt
  }

  _Task3Handle = NULL;
  xTaskCreate(
    task3, // Function to implement the task
    "task3", // Name of the task
    2000,  // Stack size in words
    NULL,  // Task input parameter
    1,  // Priority of the task
    &_Task3Handle  // Task handle.
    );
  if(_Task3Handle == NULL){
    log_e("Could not create callback task 3");
    while (1) {;} // halt
  }

  _Task4Handle = NULL;
  xTaskCreate(
    task4, // Function to implement the task
    "task4", // Name of the task
    2000,  // Stack size in words
    NULL,  // Task input parameter
    1,  // Priority of the task
    &_Task4Handle  // Task handle.
    );
  if(_Task4Handle == NULL){
    log_e("Could not create callback task 4");
    while (1) {;} // halt
  }

  _Task5Handle = NULL;
  xTaskCreate(
    task5, // Function to implement the task
    "task5", // Name of the task
    2000,  // Stack size in words
    NULL,  // Task input parameter
    1,  // Priority of the task
    &_Task5Handle  // Task handle.
    );
  if(_Task5Handle == NULL){
    log_e("Could not create callback task 5");
    while (1) {;} // halt
  }

}

void loop() {
  // loop task remains free for other work
  delay(10); // Let the FreeRTOS reset the watchDogTimer
}
