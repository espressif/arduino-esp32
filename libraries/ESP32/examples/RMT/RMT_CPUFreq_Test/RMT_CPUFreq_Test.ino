/*
  RMT_CPUFreq_test

  Demonstrates usage of RGB LED driven by RMT to verufy that RMT works on any CPU/APB Frequency.

  RGBLedWrite demonstrates control of each RMT channel:
  void neopixelWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val)
*/

//if not set by pins_arduino.h, define your own WS281x GPIO and default brightness
//#define RGB_BRIGHTNESS 64 // Change white brightness (max 255)
//#define RGB_BUILTIN 38 // S3 GPI 48 or 38

// different frequencies for the CPU
// ESP32C3 max freq is 160, but setting it to 240MHz will just fail and keep the last one
const int bauds = 115200;
uint8_t cpufreqs[] = {240, 160, 80, 40, 20, 10};
uint32_t Freq = 0;
int i = 0;

void setup() {
  Freq = getCpuFrequencyMhz();

  Serial.begin(bauds);
  delay(500);

  Freq = getCpuFrequencyMhz();
  Serial.print("CPU Freq = ");
  Serial.print(Freq);
  Serial.println(" MHz");
  Freq = getXtalFrequencyMhz();
  Serial.print("XTAL Freq = ");
  Serial.print(Freq);
  Serial.println(" MHz");
  Freq = getApbFrequency();
  Serial.print("APB Freq = ");
  Serial.print(Freq);
  Serial.println(" Hz");
}

void loop() {

  Serial.print("\nchange CPU freq to ");
  Serial.println(cpufreqs[i]);
  delay(250);

  setCpuFrequencyMhz(cpufreqs[i]);

  Freq = getCpuFrequencyMhz();

  Serial.end();
  Serial.begin(bauds);

  Serial.print("Sending to serial with baud rate = ");
  Serial.println(bauds);
  Serial.print("CPU Freq = ");
  Serial.print(Freq);
  Serial.println(" MHz");

  i = (i + 1) % sizeof(cpufreqs);

#ifdef RGB_BUILTIN
  // Changes to the CPU Freq demand RMT to reset internal parameters for the Tick
  // This is fixed by reinitializing the RMT peripheral
  rmtInit(RGB_BUILTIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000); // 100ms Tick

  neopixelWrite(RGB_BUILTIN, RGB_BRIGHTNESS, 0, 0); // Red
  Serial.println("LED Red");
  delay(1000);
  neopixelWrite(RGB_BUILTIN, 0, RGB_BRIGHTNESS, 0); // Green
  Serial.println("LED Green");
  delay(1000);
  neopixelWrite(RGB_BUILTIN, 0, 0, RGB_BRIGHTNESS); // Blue
  Serial.println("LED Blue");
  delay(1000);
  neopixelWrite(RGB_BUILTIN, RGB_BRIGHTNESS, RGB_BRIGHTNESS, RGB_BRIGHTNESS); // White
  Serial.println("White");
  delay(1000);
  neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Off / black
  Serial.println("Off");
  delay(1000);
  
#endif
}