#include "unit_tests_common.h"

#define _I2S_DMA_BUFFER_COUNT 4 // BUFFER COUNT must be between 2 and 128
#define _i2s_dma_buffer_size 1024


// This is mostly copy from I2S lib, but we will not use the ring buffers and we will read / write directly with idf-i2s driver
int my_i2s_begin(int mode, uint32_t sampleRate, uint32_t bitsPerSample){

  esp_i2s::i2s_mode_t i2s_mode = (esp_i2s::i2s_mode_t)(esp_i2s::I2S_MODE_RX | esp_i2s::I2S_MODE_TX | esp_i2s::I2S_MODE_SLAVE);

  if(mode == I2S_PHILIPS_MODE || mode == I2S_RIGHT_JUSTIFIED_MODE || mode == I2S_LEFT_JUSTIFIED_MODE){
    if(bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32){
        log_e("Invalid bits per sample for normal mode (requested %d)\nAllowed bps = 8 | 16 | 24 | 32", bitsPerSample);
      return 0; // ERR
    }
  }else if(mode == PDM_STEREO_MODE || mode == PDM_MONO_MODE){ // end of Normal Philips mode; start of PDM mode
    #if (SOC_I2S_SUPPORTS_PDM_TX && SOC_I2S_SUPPORTS_PDM_RX)
      i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_PDM);
    #else
      log_e("This chip does not support PDM");
      return 0; // ERR
    #endif
  } // Mode
  esp_i2s::i2s_config_t i2s_config = {
    .mode = i2s_mode,
    .sample_rate = sampleRate,
    .bits_per_sample = (esp_i2s::i2s_bits_per_sample_t)bitsPerSample,
    .channel_format = esp_i2s::I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (esp_i2s::i2s_comm_format_t)(esp_i2s::I2S_COMM_FORMAT_STAND_I2S), // 0x01 // default
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
    .dma_buf_count = _I2S_DMA_BUFFER_COUNT,
    .dma_buf_len = _i2s_dma_buffer_size,
    .use_apll = true
  };
  // Install and start i2s driver
  if(ESP_OK != esp_i2s::i2s_driver_install((esp_i2s::i2s_port_t) 0, &i2s_config, 0, NULL)){
    log_e("ERROR i2s driver install failed");
    return 0; // ERR
  } //try installing i2s driver

  if(mode == I2S_RIGHT_JUSTIFIED_MODE || mode == I2S_LEFT_JUSTIFIED_MODE || mode == PDM_MONO_MODE){ // mono/single channel
  // Set the clock for MONO. Stereo is not supported yet.
    if(ESP_OK != esp_i2s::i2s_set_clk((esp_i2s::i2s_port_t) 0, sampleRate, (esp_i2s::i2s_bits_per_sample_t)bitsPerSample, esp_i2s::I2S_CHANNEL_MONO)){
      log_e("Setting the I2S Clock has failed!\n");
      return 0; // ERR
    }
  } // mono channel mode

  esp_i2s::i2s_pin_config_t pin_config = {
    .bck_io_num = 14,
    .ws_io_num = 25,
    .data_out_num = 26,
    .data_in_num = 35
  };

  if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) 0, &pin_config)){
    log_e("i2s_set_pin failed");
    return 0; // ERR
  }else{
    return 1; // OK
  }
  return 1; // OK
}

int test_02(){
  int errors = 0;
  int current_errors = 0;
  int data;
  int tmp;
  int tmp2;
  Serial.println("Counterpart for data transfer test");
  Serial.println("==============================================================================");
  // ============================================================================================
  Serial.println("Counterpart reads, UUT writes");
  uint8_t buffer[sizeof(bps32)];
  for(int m = 0; m < sizeof(mode)/sizeof(i2s_mode_t); ++m){ // Note: ADC_DAC_MODE is missing - it will be tested separately
    for(int bps = 0; bps < sizeof(bitsPerSample)/sizeof(int); ++bps){
      for(int sr = 0; sr < sizeof(sampleRate)/sizeof(long); ++sr){
        Serial.print("Testing mode ="); Serial.print(mode[m]); Serial.print("; bps="); Serial.print(bitsPerSample[bps]); Serial.print("; f="); Serial.print(sampleRate[sr]); Serial.println(" Hz");
        if (!my_i2s_begin(mode[m], sampleRate[sr], bitsPerSample[bps])){
        //if (!I2S.begin(mode[m], sampleRate[sr], bitsPerSample[bps], false)){
          Serial.println("Failed to initialize I2S!");
          ++errors;
          Serial1.write(C_ERR);
          continue;
        }

        Serial1.write(C_RDY);
        data = wait_and_read();
        if(data == UUT_ERR){
          Serial.println("UUT reports error");
          ++errors;
          continue;
        }

        if(data != UUT_RDY){
          Serial.print("UUT sends unexpected msg code: ");
          Serial.println(data);
          halt();
          // what now?
        }else{
          Serial.println("UUT reports ready - starting receiving");
        }

        size_t bytes_read;
        int zero_samples = 0;
        esp_err_t ret;
        buffer[0] = 0;

        ret = i2s_read((esp_i2s::i2s_port_t) 0, buffer, 1, &bytes_read, 1000);
        //ret = I2S.read(buffer, 1);
        if(ret != ESP_OK){Serial.printf("i2s_read returned with error %d\n", ret);}
        if(buffer[0] == 0){
          ++zero_samples;
          Serial.println("first try - zero sample");
        }

        while(buffer[0] == 0){
          ret = i2s_read((esp_i2s::i2s_port_t) 0, buffer, 1, &bytes_read, 100);
          //I2S.read(buffer, 1);
          if(ret != ESP_OK){Serial.printf("i2s_read returned with error %d\n", ret);}
          ++zero_samples;
          Serial.print("zero samples = "); Serial.println(zero_samples);
        }
        Serial.print("First non-zero sample after "); Serial.print(zero_samples); Serial.print(" zero samples; the sample is "); Serial.println(buffer[0]);

        Serial.println("UUT reports ready - starting read");
        current_errors = 0;
        switch(bitsPerSample[bps]){
          case 8:
            ret = i2s_read((esp_i2s::i2s_port_t) 0, buffer, sizeof(bps8), &bytes_read, 100);
            //ret = I2S.read(buffer, 1);
            if(ret != ESP_OK){Serial.printf("i2s_read returned with error %d\n", ret);}
            if(bytes_read != sizeof(bps8)){Serial.printf("bytes_read %d != %d sizeof(bps8); break test\n", bytes_read, sizeof(bps8)); ++current_errors; break;}
            for(int i = 0; i < sizeof(bps8); ++i){ if(buffer[i] != bps8[i]){Serial.printf("buffer[%d] %d != %d bps8[%d]\n", i, buffer[i], bps8[i], i); ++current_errors;}}
            break;
          case 16:
            i2s_read((esp_i2s::i2s_port_t) 0, buffer, sizeof(bps16), &bytes_read, 100);
            if(bytes_read != sizeof(bps16)){Serial.printf("bytes_read %d != %d sizeof(bps16); break test\n", bytes_read, sizeof(bps16)); ++current_errors; break;}
            for(int i = 0; i < sizeof(bps16)/2; ++i){ if(((uint16_t*)buffer)[i] != bps16[i]){Serial.printf("buffer[i] %d != %d bps16[i]\n", ((uint16_t*)buffer)[i], bps16[i]); ++current_errors;}}
            break;
          case 24:
            i2s_read((esp_i2s::i2s_port_t) 0, buffer, sizeof(bps24), &bytes_read, 100);
            if(bytes_read != sizeof(bps8)){Serial.printf("bytes_read %d != %d sizeof(bps24); break test\n", bytes_read, sizeof(bps24)); ++current_errors; break;}
            for(int i = 0; i < sizeof(bps24)/4; ++i){ if(((uint32_t*)buffer)[i] != bps24[i]){Serial.printf("buffer[i] %d != %d bps32[i]\n", ((uint32_t*)buffer)[i], bps24[i]); ++current_errors;}}
            break;
          case 32:
            i2s_read((esp_i2s::i2s_port_t) 0, buffer, sizeof(bps32), &bytes_read, 100);
            if(bytes_read != sizeof(bps32)){Serial.printf("bytes_read %d != %d sizeof(bps32); break test\n", bytes_read, sizeof(bps32)); ++current_errors; break;}
            for(int i = 0; i < sizeof(bps32)/4; ++i){ if(((uint32_t*)buffer)[i] != bps32[i]){Serial.printf("buffer[i] %d != %d bps32[i]\n", ((uint32_t*)buffer)[i], bps32[i]);  ++current_errors;}}
            break;
        }
        esp_i2s::i2s_driver_uninstall((esp_i2s::i2s_port_t) 0);
        Serial1.write(current_errors);
        errors += current_errors;
      } // sr
    } // bps
  } // i2s mode
  // ============================================================================================
  return errors; // only for debug
  for(int m = 0; m < sizeof(mode)/sizeof(i2s_mode_t); ++m){ // Note: ADC_DAC_MODE is missing - it will be tested separately
    for(int bps = 0; bps < sizeof(bitsPerSample)/sizeof(int); ++bps){
      for(int sr = 0; sr < sizeof(sampleRate)/sizeof(long); ++sr){
        Serial.print("Testing mode ="); Serial.print(mode[m]); Serial.print("; bps="); Serial.print(bitsPerSample[bps]); Serial.print("; f="); Serial.print(sampleRate[sr]); Serial.println(" Hz");
        if (!my_i2s_begin(mode[m], sampleRate[sr], bitsPerSample[bps])) {
          Serial.println("Failed to initialize I2S!");
          ++errors;
          Serial1.write(C_ERR);
          continue;
        }

        Serial1.write(C_RDY);
        int data = wait_and_read();
        if(data == UUT_ERR){
          Serial.println("UUT reports error");
          ++errors;
          continue;
        }

        if(data != UUT_RDY){
          Serial.print("UUT sends unexpected msg code: ");
          Serial.println(data);
          halt();
          // what now?
        }else{
          Serial.println("UUT reports ready - starting transfer");
        }

        size_t bytes_written;
        current_errors = 0;
        switch(bitsPerSample[bps]){

          case 8:
            esp_i2s::i2s_write((esp_i2s::i2s_port_t) 0, bps8, sizeof(bps8), &bytes_written, 0);
            if(sizeof(bps8) != bytes_written){
              Serial.printf("sizeof(bps8) %d != %d bytes_written\n",sizeof(bps8) ,bytes_written);
              ++current_errors;
            }
            break;
          case 16:
            esp_i2s::i2s_write((esp_i2s::i2s_port_t) 0, bps16, sizeof(bps16), &bytes_written, 0);
            if(sizeof(bps16) != bytes_written){
              Serial.printf("sizeof(bps16) %d != %d bytes_written\n",sizeof(bps16) ,bytes_written);
              ++current_errors;
            }
            break;
          case 24:
            esp_i2s::i2s_write((esp_i2s::i2s_port_t) 0, bps24, sizeof(bps32), &bytes_written, 0);
            if(sizeof(bps32) != bytes_written){
              Serial.printf("sizeof(bps32) %d != %d bytes_written\n",sizeof(bps32) ,bytes_written);
              ++current_errors;
            }
            break;
          case 32:
            esp_i2s::i2s_write((esp_i2s::i2s_port_t) 0, bps32, sizeof(bps32), &bytes_written, 0);
            if(sizeof(bps32) != bytes_written){
              Serial.printf("sizeof(bps32) %d != %d bytes_written\n",sizeof(bps32) ,bytes_written);
              ++current_errors;
            }
            break;
        }
        esp_i2s::i2s_driver_uninstall((esp_i2s::i2s_port_t) 0);
        Serial1.write(current_errors);
        errors += current_errors;
      } // sr
    } // bps
  } // i2s mode
  Serial.println("==============================================================================");

  Serial.print("End of template test. ERRORS = "); Serial.println(errors);
  return errors;
}