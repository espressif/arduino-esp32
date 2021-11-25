/*
 This example generates a square wave based tone at a specified frequency
 and sample rate. Then outputs the data using the I2S interface to a
 MAX08357 I2S Amp Breakout board.

 Circuit:
 * Arduino/Genuino Zero, MKR family and Nano 33 IoT
 * MAX08357:
   * GND connected GND
   * VIN connected 5V
   * LRC connected to pin 0 (Zero) or 3 (MKR), A2 (Nano) or 25 (ESP32)
   * BCLK connected to pin 1 (Zero) or 2 (MKR), A3 (Nano) or 5 (ESP32)
   * DIN connected to pin 9 (Zero) or A6 (MKR), 4 (Nano) or 26 (ESP32)

 created 17 November 2016
 by Sandeep Mistry
 */

#define PLAY
//#define PLOT

#include <I2S.h>
const int frequency = 440; // frequency of square wave in Hz
const int amplitude = 500; // amplitude of square wave
const long sampleRate[] = {8000, 11025, 16000, 22050, 24000, 32000, 44100};
const int bitsPerSample[] = {8, 16, 24, 32};

//const i2s_mode_t mode = I2S_PHILIPS_MODE;
//const i2s_mode_t mode = I2S_RIGHT_JUSTIFIED_MODE;
//const i2s_mode_t mode = I2S_LEFT_JUSTIFIED_MODE;
//const i2s_mode_t mode = ADC_DAC_MODE;
const i2s_mode_t mode = PDM_STEREO_MODE;
//const i2s_mode_t mode = PDM_MONO_MODE;

void test()
{
  Serial.println("Try double end()");
  I2S.end();
  I2S.end();
  Serial.println("");

  Serial.println("Try double begin()");
  Serial.printf("First begin() returned %d\n", I2S.begin(I2S_PHILIPS_MODE, 44100, 32));
  Serial.printf("Second begin() returned %d\n", I2S.begin(I2S_PHILIPS_MODE, 44100, 32));
  I2S.end();
  Serial.println("");

  Serial.println("Try normal begin -> end twice");
  Serial.printf("First begin() returned %d\n", I2S.begin(I2S_PHILIPS_MODE, 44100, 32));
  I2S.end();
  Serial.printf("Second begin() returned %d\n", I2S.begin(I2S_PHILIPS_MODE, 44100, 32));
  I2S.end();
  Serial.println("");

  Serial.println("Try begin with all modes");
  Serial.printf("begin(I2S_PHILIPS_MODE) returned %d\n", I2S.begin(I2S_PHILIPS_MODE,  44100, 32));
  I2S.end();
  Serial.printf("begin(I2S_RIGHT_JUSTIFIED_MODE) returned %d\n", I2S.begin(I2S_RIGHT_JUSTIFIED_MODE,  44100, 32));
  I2S.end();
  Serial.printf("begin(I2S_LEFT_JUSTIFIED_MODE) returned %d\n", I2S.begin(I2S_LEFT_JUSTIFIED_MODE,  44100, 32));
  I2S.end();
  Serial.printf("begin(ADC_DAC_MODE) returned %d\n", I2S.begin(ADC_DAC_MODE,  44100, 16));
  I2S.end();
  Serial.printf("begin(PDM_STEREO_MODE) returned %d\n", I2S.begin(PDM_STEREO_MODE,  44100, 32));
  I2S.end();
  Serial.printf("begin(PDM_MONO_MODE) returned %d\n", I2S.begin(PDM_MONO_MODE,  44100, 32));
  I2S.end();
  Serial.println("End of testing");
  Serial.println("");
}

void setup() {
  Serial.begin(115200);
  Serial.println("I2S testing");
  //test();
}

int halfWavelength;
void play_a_while(long seconds){
  long start_time = millis();
  long curr_time = 0;
  int count = 0;
  short sample = amplitude;
  while(curr_time - start_time < seconds*1000){
    //Serial.print("curr_time = "); Serial.print(curr_time); Serial.print("; start_time = "); Serial.print(start_time); Serial.print("; seconds*1000 = "); Serial.println(seconds*1000);
    if (count % halfWavelength == 0 ){
      // invert the sample every half wavelength count multiple to generate square wave
      sample = -1 * sample;
    }

    if(mode == I2S_PHILIPS_MODE || mode == ADC_DAC_MODE || mode == PDM_STEREO_MODE){ // write the same sample twice, once for Right and once for Left channel
      I2S.write(sample); // Right channel
      I2S.write(sample); // Left channel
    }else if(mode == I2S_RIGHT_JUSTIFIED_MODE || mode == I2S_LEFT_JUSTIFIED_MODE || mode == PDM_MONO_MODE){
      // write the same only once - it will be automatically copied to the other channel
      I2S.write(sample);
    }

    // increment the counter for the next sample
    count++;

    //Serial.print("Played "); Serial.print((curr_time-start_time)/1000); Serial.print(" / "); Serial.print(seconds);  Serial.println(" s");

    curr_time = millis();
  }
}

void plot_a_while(long seconds){
  long start_time = millis();
  long curr_time = 0;
  while(curr_time - start_time < seconds*1000){
    //Serial.print("curr_time = "); Serial.print(curr_time); Serial.print("; start_time = "); Serial.print(start_time); Serial.print("; seconds*1000 = "); Serial.println(seconds*1000);
    // read a sample
    int sample = I2S.read();
    //Serial.println(sample);

    if (sample && sample != -1 && sample != 1) {
      Serial.println(sample);
    }
  }
}

void loop() {
  for(int bps = 0; bps < sizeof(bitsPerSample)/sizeof(int); ++bps){
    #ifdef PLAY
      if(mode == ADC_DAC_MODE && bitsPerSample[bps] != 16){
        continue;
      }
    #endif

    for(int sr = 0; sr < sizeof(sampleRate)/sizeof(long); ++sr){
      #ifdef PLAY
        Serial.print("Setting up bps="); Serial.print(bitsPerSample[bps]); Serial.print("; f="); Serial.print(sampleRate[sr]); Serial.println(" Hz");
      #endif
      if (!I2S.begin(mode, sampleRate[sr], bitsPerSample[bps])) {
        Serial.println("Failed to initialize I2S!");
      }else{
        halfWavelength = (sampleRate[sr] / frequency); // half wavelength of square wave
        #ifdef PLAY
          Serial.println("Setup done. Starting playback");
          play_a_while(3);
          Serial.println("Playback finished");
        #endif

        #ifdef PLOT
          plot_a_while(5);
        #endif
        I2S.end();
      } // begin ok
      #ifdef PLAY
        Serial.println("");
      #endif
      delay(1000);
    } // sr
    #ifdef PLAY
      Serial.println("===============================================");
    #endif
    delay(3000);
  } // bps
}
