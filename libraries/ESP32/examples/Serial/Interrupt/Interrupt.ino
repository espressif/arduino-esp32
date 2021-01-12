HardwareSerial hwSerial(0);
HardwareSerial hwSerial2(2);

bool ok = true;

static void IRAM_ATTR onSerialRX(uint8_t c, void* user_arg){  
	hwSerial.print(c);
	((HardwareSerial*)user_arg)->print(c);
}

void setup()
{
  hwSerial.begin(115200);
  hwSerial2.begin(115200);
  hwSerial2.setRxInterrupt(onSerialRX, (void*)&hwSerial2);  
}

void loop()
{
  delay(1);
}
