void setup()
{
    //setup on pin 18, channel 0 with frequency 312500 Hz
    sigmaDeltaSetup(18,0, 312500);
    //initialize channel 0 to off
    sigmaDeltaWrite(0, 0);
}

void loop()
{
    //slowly ramp-up the value
    //will overflow at 256
    static uint8_t i = 0;
    sigmaDeltaWrite(0, i++);
    delay(100);
}
