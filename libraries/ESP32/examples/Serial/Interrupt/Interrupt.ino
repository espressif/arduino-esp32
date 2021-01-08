HardwareSerial hwSerial(0);
HardwareSerial hwSerial2(2);

bool ok = true;

static void IRAM_ATTR onSerialRX(char c, void* user_arg){
  if(*((bool*)user_arg))
	hwSerial.print(c);
}

void setup()
{
  hwSerial.begin(115200);
  hwSerial2.begin(115200);
  hwSerial2.setRXInterrupt(onSerialRX, (void*)&ok);
  
}

void loop()
{
  delay(1);
}
