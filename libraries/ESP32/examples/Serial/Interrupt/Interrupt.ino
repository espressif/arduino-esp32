HardwareSerial hwSerial(0);
HardwareSerial hwSerial2(2);

static void IRAM_ATTR onSerialRX(uint8_t c, void* user_arg){  
	hwSerial.print(c);
	
	// Cast the user_arg containing a void* to the Serial device, to HardwareSerial* to be used
	HardwareSerial* serial = (HardwareSerial*)user_arg;
	serial->print(c);
}

void setup()
{
  hwSerial.begin(115200);
  hwSerial2.begin(115200);
  
  // The user_arg is expected to be a void pointer (void*), 
  // so take the reference of hwSerial2, and cast the HardwareSerial* to a void*
  hwSerial2.setRxInterrupt(onSerialRX, (void*)&hwSerial2);  
}

void loop()
{
  delay(1);
}
