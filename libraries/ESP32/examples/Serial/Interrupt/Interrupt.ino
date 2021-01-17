/** Tim Koers - 2021
  *
  * This sketch shows the usage of a semaphore to receive Serial data.
  * You can safely use this in a FreeRTOS environment and use the semaphore from different tasks on different CPU cores.
  * However, the InterruptQueue example is much more efficient, since it uses less code.
  * This sketch assumes that when Hi is sent, the device returns an 8 byte message.
  *
  */

#define BUFFER_SIZE 8

// This semaphore is here to handle the interruption of the loop when the interrupt is running.
SemaphoreHandle_t bufferSemaphore;

static volatile char inputBuffer[BUFFER_SIZE];
static volatile size_t inputBufferLength = 0;

bool messageSent = false;

// Please keep in mind, since the ESP32 is dual core, 
// the interrupt will be running on the same core as the setRxInterrupt function was called on.
static void IRAM_ATTR onSerialRX(uint8_t character, void* user_arg){
	
	// Cast the user_arg back to a array
	char* buffer = (char*)user_arg;
	
	BaseType_t xHighPriorityTaskWoken;
	
	if(xSemaphoreTakeFromISR(bufferSemaphore, &xHighPriorityTaskWoken) == pdTRUE){	
		if(inputBufferLength < BUFFER_SIZE){
			buffer[inputBufferLength++] = (char)character;
		}
		xSemaphoreGiveFromISR(bufferSemaphore, &xHighPriorityTaskWoken);
	}
}

void setup()
{
  bufferSemaphore = xSemaphoreCreateBinary();
	
  Serial.begin(115200);
  Serial2.begin(115200);
  
  // The user_arg is expected to be a void pointer (void*), 
  // so take the reference of the variable, and it to a void*
  Serial2.setRxInterrupt(onSerialRX, (void*)&inputBuffer);
}

void loop()
{ 
  if(!messageSent){
	  Serial.println("Hi");
	  messageSent = true;
  }

  // Check if the semaphore can be taken and if the inputBuffer length is long enough.
  // Please keep in mind, since the variable inputBufferLength is also accessed inside the IRQ handler,
  // the semaphore needs to be taken BEFORE you can access that variable.
  if(xSemaphoreTake(bufferSemaphore, portMAX_DELAY) == pdTRUE && inputBufferLength == (BUFFER_SIZE - 1)){
	for(size_t i = 0; i < inputBufferLength; i++){
	  Serial.write(inputBuffer[i]);
	  
	  // Clear the buffer
	  inputBuffer[i] = '\0';
	}
	// Clear the bufferLength
	inputBufferLength = 0;
	
	// Give back the semaphore
	xSemaphoreGive(bufferSemaphore);
	
	// Allow the system to request another data set
	messageSent = false;
  }
  
  delay(10); // Wait for 10ms to allow some data to come in
}
