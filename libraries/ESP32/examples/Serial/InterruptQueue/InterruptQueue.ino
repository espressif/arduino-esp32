/** Tim Koers - 2021
  *
  * This sketch shows the usage of an queue, to store and read the characters that are sent to the interrupt.
  * You can safely use this in a FreeRTOS environment and use the queue from different tasks on different CPU cores.
  * This sketch assumes that when Hi is sent, the device returns an 8 byte message.
  * 
  */

#define BUFFER_SIZE 8

// This queue is here to handle the interruption of the loop when the interrupt is running.
QueueHandle_t bufferQueue;

bool messageSent = false;

// Please keep in mind, since the ESP32 is dual core, 
// the interrupt will be running on the same core as the setRxInterrupt function was called on.
static void IRAM_ATTR onSerialRX(uint8_t character, void* user_arg){
	
	BaseType_t xHighPriorityTaskWoken;
	
	if(!xQueueSendFromISR(bufferQueue, &character, &xHighPriorityTaskWoken) == pdTRUE){
	  log_e("IRQ", "Failed to put character onto the queue\n");
	}
}

void setup()
{
  bufferQueue = xQueueCreate(BUFFER_SIZE * 4, sizeof(char)); // Create a queue that can hold 4 messages of 8-bytes each.
	
  assert(bufferQueue != NULL);

  Serial.begin(115200);
  Serial2.begin(115200);

  Serial2.setRxInterrupt(onSerialRX, NULL);
}

void loop()
{
  if(!messageSent){
	  Serial.println("Hi");
	  messageSent = true;
  }

  // Check if data in the queue and check if there is a complete message (8-bytes) in the queue
  if(!(uxQueueMessagesWaiting(bufferQueue) % BUFFER_SIZE)){
    char c;
	// Check if the queue is not empty, 0 % 8 returns 0 instead, so does 24 % 8
	while(uxQueueMessagesWaiting(bufferQueue) > 0){
	  // Get the character from the queue, but don't block if someone else is busy with the queue
	  if(xQueueReceive(bufferQueue, &c, 0) == pdTRUE)
		Serial.write(c);
	}
	
	// Allow the system to request another data set
	messageSent = false;
  }
}
