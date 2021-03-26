/** Tim Koers - 2021
  *
  * This sketch shows the 'regular' way of receiving Serial data and using it.
  * In Arduino Libraries that communicate with a UART peripheral/device, it is seen many times that a delay is
  * added after each sent message in order to wait for the answer of the device. That is very inefficient since,
  * if not done properly, it might block the CPU (-core) and the function will take x milliseconds/seconds to return.
  * This sketch assumes that when Hi is sent, the device returns an 8 byte message.
  *
  */

void setup()
{
  Serial.begin(115200);
  Serial2.begin(115200);
}

void loop()
{
  Serial.println("Hi");
  // Now wait for the response to arrive
  delay(1000);
  
  // The response MIGHT have arrived, not sure though
  if(Serial.available() == 8){
    Serial.print(Serial2.read());
  }
}
