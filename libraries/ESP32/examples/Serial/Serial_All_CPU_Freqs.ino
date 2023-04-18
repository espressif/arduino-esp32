/*
   Simple Sketch for testing HardwareSerial with different CPU Frequencies
   Changing the CPU Frequency may affect peripherals and Wireless functionality
   In ESP32 Arduino, UART shall work correctly in order to let the user see DGB info
   and other application messages.

   CPU Frequency is usually lowered in sleep modes
   and some other Low Power configurations

*/

int cpufreqs[6] = {240, 160, 80, 40, 20, 10};
#define NUM_CPU_FREQS (sizeof(cpufreqs) / sizeof(int))

void setup() {

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n Starting...\n");
  Serial.flush();

  // initial information
  uint32_t Freq = getCpuFrequencyMhz();
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
  delay(500);

  // ESP32-C3 and other RISC-V target may not support 240MHz
#ifdef CONFIG_IDF_TARGET_ESP32C3
  uint8_t firstFreq = 1;
#else
  uint8_t firstFreq = 0;
#endif

  // testing HardwareSerial for all possible CPU/APB Frequencies
  for (uint8_t i = firstFreq; i < NUM_CPU_FREQS; i++) {
    Serial.printf("\n------- Trying CPU Freq = %d ---------\n", cpufreqs[i]);
    Serial.flush();  // wait to empty the UART FIFO before changing the CPU Freq.
    setCpuFrequencyMhz(cpufreqs[i]);
    Serial.updateBaudRate(115200);

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
    if (i < NUM_CPU_FREQS - 1) {
      Serial.println("Moving to the next frequency after a pause of 2 seconds.");
      delay(2000);
    }
  }
  Serial.println("\n-------------------\n");
  Serial.println("End of testing...");
  Serial.println("\n-------------------\n");
}

void loop() {
  // Nothing here so far
}
