// Serial, Serial1 and Serial2 are all of the type HardwareSerial.

static void IRAM_ATTR onSerialRX(uint8_t c, void* user_arg){  
	Serial.print(c);
	
	// Cast the user_arg containing a void* to the Serial device, to HardwareSerial* to be used
	HardwareSerial* serial = (HardwareSerial*)user_arg;
	serial->print(c);
}

void setup()
{
  Serial.begin(115200);
  Serial2.begin(115200);
  
  // The user_arg is expected to be a void pointer (void*), 
  // so take the reference of Serial2, and cast the HardwareSerial* to a void*
  Serial2.setRxInterrupt(onSerialRX, (void*)&Serial2);  
}

void loop()
{
  delay(1);
}
