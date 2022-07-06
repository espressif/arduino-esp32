/*
  This example demonstrates I2S ADC capability to sample high frequency analog signals.
  The PWM signal generated with ledc is only for ease of use when first trying out.
  To sample the generated signal connect default pins 27(PWM) and 32(Sampling) together.
  If you do not wish to generate PWM simply comment out the definition of constant GENERATE_PWM
  Try to change the PWM_DUTY_PERCENT and see how to averaged value changes.

  The maximum for I2S ADC sampling frequency is 5MHz (value 5000000), however there will be many values repeated because the real
  sampling frequency is much lower -

  By default this example will print values compatible with Arduino plotter
  1. signal - all values
  2. signal - averaged value

  You can change the number of sample over which is the signal averaged by changing value of AVERAGE_EVERY_N_SAMPLES
  If you comment the definition altogether the averaging will not be performed nor printed.

  If you do not wish to print every value, simply comment definition of constant PRINT_ALL_VALUES

  Note: ESP prints messages at startup which will pollute Arduino IDE Serial plotter legend.
  To avoid this pollution, start the plotter after startup (op restart)
*/
#include <driver/i2s.h>

// I2S
#define I2S_SAMPLE_RATE (277777) // Max sampling frequency = 277.777 kHz
#define ADC_INPUT (ADC1_CHANNEL_4) //pin 32
#define I2S_DMA_BUF_LEN (1024)

// PWM
#define GENERATE_PWM
#define OUTPUT_PIN (27)
#define PWM_FREQUENCY ((I2S_SAMPLE_RATE)/4)
#define PWM_DUTY_PERCENT (50)
#define PWM_RESOLUTION_BITS (2) // Lower bit resolution enables higher frequency
#define PWM_DUTY_VALUE ((((1<<(PWM_RESOLUTION_BITS)))*(PWM_DUTY_PERCENT))/100) // Duty value used for setup function based on resolution

// Sample post processing
#define PRINT_ALL_VALUES
#define AVERAGE_EVERY_N_SAMPLES (100)

void i2sInit(){
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate =  I2S_SAMPLE_RATE,              // The format of the signal using ADC_BUILT_IN
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = I2S_DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  Serial.printf("Attempting to setup I2S ADC with sampling frequency %d Hz\n", I2S_SAMPLE_RATE);
  if(ESP_OK != i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL)){
    Serial.printf("Error installing I2S. Halt!");
    while(1);
  }
  if(ESP_OK != i2s_set_adc_mode(ADC_UNIT_1, ADC_INPUT)){
    Serial.printf("Error setting up ADC. Halt!");
    while(1);
  }
  if(ESP_OK != adc1_config_channel_atten(ADC_INPUT, ADC_ATTEN_DB_11)){
    Serial.printf("Error setting up ADC attenuation. Halt!");
    while(1);
  }

  if(ESP_OK != i2s_adc_enable(I2S_NUM_0)){
    Serial.printf("Error enabling ADC. Halt!");
    while(1);
  }
  Serial.printf("I2S ADC setup ok\n");
}

void setup() {
  Serial.begin(115200);

#ifdef GENERATE_PWM
  // PWM setup
  Serial.printf("Setting up PWM: frequency = %d; resolution bits %d; Duty cycle = %d; duty value = %d, Output pin = %d\n", PWM_FREQUENCY, PWM_RESOLUTION_BITS, PWM_DUTY_PERCENT, PWM_DUTY_VALUE, OUTPUT_PIN);
  uint32_t freq = ledcSetup(0, PWM_FREQUENCY, PWM_RESOLUTION_BITS);
  if(freq != PWM_FREQUENCY){
    Serial.printf("Error setting up PWM. Halt!");
    while(1);
  }
  ledcAttachPin(OUTPUT_PIN, 0);
  ledcWrite(0, PWM_DUTY_VALUE);
  Serial.printf("PWM setup ok\n");
#endif

  // Initialize the I2S peripheral
  i2sInit();
}

void loop(){
// The 4 high bits are the channel, and the data is inverted
  size_t bytes_read;
  uint16_t buffer[I2S_DMA_BUF_LEN] = {0};

#ifdef AVERAGE_EVERY_N_SAMPLES
  uint32_t read_counter = 0;
  uint32_t averaged_reading = 0;
  uint64_t read_sum = 0;
#endif

  while(1){
    i2s_read(I2S_NUM_0, &buffer, sizeof(buffer), &bytes_read, 15);
    //Serial.printf("read %d Bytes\n", bytes_read);

    for(int i = 0; i < bytes_read/2; ++i){
#ifdef PRINT_ALL_VALUES
      //Serial.printf("[%d] = %d\n", i, buffer[i] & 0x0FFF); // Print with indexes
      Serial.printf("Signal:%d ", buffer[i] & 0x0FFF); // Print compatible with Arduino Plotter
#endif
#ifdef AVERAGE_EVERY_N_SAMPLES
      read_sum += buffer[i] & 0x0FFF;
      ++read_counter;
      if(read_counter == AVERAGE_EVERY_N_SAMPLES){
        averaged_reading = read_sum / AVERAGE_EVERY_N_SAMPLES;
        //Serial.printf("averaged_reading = %d over %d samples\n", averaged_reading, read_counter); // Print with additional info
        Serial.printf("Averaged_signal:%d", averaged_reading); // Print compatible with Arduino Plotter
        read_counter = 0;
        read_sum = 0;
      }
#endif
#if defined(PRINT_ALL_VALUES) || defined (AVERAGE_EVERY_N_SAMPLES)
      Serial.printf("\n");
#endif
    } // for
  } // while
}
