int test_02(){
  int errors = 0;
  int tmp;
  int tmp2;
  int data;
  Serial.println("Data transfer test");
  Serial.println("==============================================================================");
  Serial.println("UUT writes, Counterpart reads");
  // ============================================================================================
  for(int m = 0; m < sizeof(mode)/sizeof(i2s_mode_t); ++m){ // Note: ADC_DAC_MODE is missing - it will be tested separately
    for(int bps = 0; bps < sizeof(bitsPerSample)/sizeof(int); ++bps){
      for(int sr = 0; sr < sizeof(sampleRate)/sizeof(long); ++sr){
        Serial.print("Testing mode ="); Serial.print(mode[m]); Serial.print("; bps="); Serial.print(bitsPerSample[bps]); Serial.print("; f="); Serial.print(sampleRate[sr]); Serial.println(" Hz");
        if (!I2S.begin(mode[m], sampleRate[sr], bitsPerSample[bps])) {
          Serial.println("Failed to initialize I2S!");
          ++errors;
          Serial1.write(UUT_ERR);
          continue;
        }

        Serial1.write(UUT_RDY);
        int data = wait_and_read();
        if(data == C_ERR){
          Serial.println("Counterpart reports error");
          ++errors;
          continue;
        }

        if(data != C_RDY){
          Serial.print("Counterpart sends unexpected msg code: ");
          Serial.println(data);
          // what now?
        }

        Serial.println("Counterpart reports ready - starting transfer");
        switch(bitsPerSample[bps]){
          case 8:  while(1){I2S.write(bps8,  sizeof(bps8));}  break;
          case 16: I2S.write(bps16, sizeof(bps16)); break;
          case 24: I2S.write(bps24, sizeof(bps24)); break;
          case 32: I2S.write(bps32, sizeof(bps32)); break;
        }
        I2S.end();

        // Receive number of errors detected by counterpart
        data = wait_and_read();
        errors += data;
        Serial.print("Counterpart reports "); Serial.print(data); Serial.println(" errors");
      } // sr
    } // bps
  } // i2s mode
  Serial.println("==============================================================================");
return errors; // only for debug
  // ============================================================================================
  Serial.println("Counterpart writes, UUT reads");
  //uint8_t buffer[sizeof(bps32)];
  uint8_t buffer[1024];
  for(int m = 0; m < sizeof(mode)/sizeof(i2s_mode_t); ++m){ // Note: ADC_DAC_MODE is missing - it will be tested separately
    for(int bps = 0; bps < sizeof(bitsPerSample)/sizeof(int); ++bps){
      for(int sr = 0; sr < sizeof(sampleRate)/sizeof(long); ++sr){
        Serial.print("Testing mode ="); Serial.print(mode[m]); Serial.print("; bps="); Serial.print(bitsPerSample[bps]); Serial.print("; f="); Serial.print(sampleRate[sr]); Serial.println(" Hz");
        if (!I2S.begin(mode[m], sampleRate[sr], bitsPerSample[bps])) {
          Serial.println("Failed to initialize I2S!");
          ++errors;
          Serial1.write(UUT_ERR);
          continue;
        }

        Serial1.write(UUT_RDY);
        data = wait_and_read();
        if(data == C_ERR){
          Serial.println("Counterpart reports error");
          ++errors;
          continue;
        }

        if(data != C_RDY){
          Serial.print("Counterpart sends unexpected msg code: ");
          Serial.println(data);
          // what now?
        }

        int zero_samples = 0;
        int ret;
        do{
          ret = I2S.read(buffer, 1);
          ++zero_samples;
          Serial.print("zero samples = "); Serial.println(zero_samples);
        }while(buffer[0] == 0);

        Serial.println("Counterpart reports ready - starting read");
        switch(bitsPerSample[bps]){
          case 8:
            //ret = I2S.read(buffer, sizeof(bps8));
            ret = I2S.read(buffer, 1024);
            if(ret == 0){Serial.println("There was an error reading from I2S: 0 bytes read"); ++errors; break;}
            if(ret == 1024){Serial.println("OK, expected and read Bytes match");}
            for(int i = 0; i < ret; ++i){
              Serial.print("Read sample [");Serial.print(i);Serial.print("] ");Serial.println(buffer[i]);
            }
            if(ret != sizeof(bps8)){
              Serial.print("Requested (");Serial.print(sizeof(bps8));Serial.print(") and read (");Serial.print(ret);Serial.println(") Bytes do not match!");
            }else{
              for(int i = 0; i < sizeof(bps8); ++i){
                if(buffer[i] != bps8[i]){
                 Serial.print("Read sample [");Serial.print(i);Serial.print("] ");Serial.print(buffer[i]);Serial.print(" does not match expected value ");Serial.println(bps8[i]);
                 ++errors;
                }
              }
            }
            break;
          case 16: I2S.read(buffer, sizeof(bps16));
            for(int i = 0; i < sizeof(bps16)/2; ++i){ if(((uint16_t*)buffer)[i] != bps16[i]) ++errors;}
            break;
          case 24: I2S.read(buffer, sizeof(bps24));
            for(int i = 0; i < sizeof(bps24)/4; ++i){ if(((uint32_t*)buffer)[i] != bps24[i]) ++errors;}
            break;
          case 32: I2S.read(buffer, sizeof(bps32));
            for(int i = 0; i < sizeof(bps32)/4; ++i){ if(((uint32_t*)buffer)[i] != bps32[i]) ++errors;}
            break;
        }
        I2S.end();
      } // sr
    } // bps
  } // i2s mode

  Serial.print("End of data transfer test. ERRORS = "); Serial.println(errors);
  return errors;
}