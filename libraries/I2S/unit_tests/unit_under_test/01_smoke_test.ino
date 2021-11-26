int test_01(){
  int ret = 0;
  int tmp;
  int tmp2;
  // ============================================================================================

  Serial.println("Try double end()");
  I2S.end();
  I2S.end();
  Serial.println("");
  // ============================================================================================

  Serial.println("Try double begin()");
  tmp = I2S.begin(I2S_PHILIPS_MODE, 44100, 32);
  Serial.print("First begin() returned "); Serial.println(tmp);
  tmp2 = I2S.begin(I2S_PHILIPS_MODE, 44100, 32);
  Serial.printf("Second begin() returned "); Serial.println(tmp2);
  if(tmp != 1 || tmp2 != 0){
    Serial.println("Double begin() test failed");
    ++ret;
  }
  I2S.end();
  Serial.println("");
  // ============================================================================================

  Serial.println("Try normal begin -> end; each sequence twice");
  tmp = I2S.begin(I2S_PHILIPS_MODE, 44100, 32);
  Serial.printf("First begin() returned "); Serial.println(tmp);
  I2S.end();
  tmp2 = I2S.begin(I2S_PHILIPS_MODE, 44100, 32);
  Serial.printf("Second begin() returned "); Serial.println(tmp2);
  I2S.end();
  if(tmp != 1 && tmp2 != 1){
    Serial.println("Double begin() -> end() test failed");
    ++ret;
  }
  Serial.println("");
  // ============================================================================================

  Serial.println("Try begin with all modes");

  tmp = I2S.begin(I2S_PHILIPS_MODE,  44100, 32);
  if(tmp != 1){
    Serial.println("begin with mode I2S_PHILIPS_MODE failed");
    ++ret;
  }
  I2S.end();
  tmp = I2S.begin(I2S_RIGHT_JUSTIFIED_MODE,  44100, 32);
  if(tmp != 1){
    Serial.println("begin with mode I2S_RIGHT_JUSTIFIED_MODE failed");
    ++ret;
  }
  I2S.end();
  tmp = I2S.begin(I2S_LEFT_JUSTIFIED_MODE,  44100, 32);
  if(tmp != 1){
    Serial.println("begin with mode I2S_LEFT_JUSTIFIED_MODE failed");
    ++ret;
  }
  I2S.end();
  tmp = I2S.begin(ADC_DAC_MODE,  44100, 16);
  if(tmp != 1){
    Serial.println("begin with mode ADC_DAC_MODE failed");
    ++ret;
  }
  I2S.end();
  tmp = I2S.begin(PDM_STEREO_MODE,  44100, 32);
  if(tmp != 1){
    Serial.println("begin with mode PDM_STEREO_MODE failed");
    ++ret;
  }
  I2S.end();
  tmp = I2S.begin(PDM_MONO_MODE,  44100, 32);
  if(tmp != 1){
    Serial.println("begin with mode PDM_MONO_MODE failed");
    ++ret;
  }
  I2S.end();
  // ============================================================================================

  Serial.print("End of smoke test. ERRORS = "); Serial.println(ret);
  return ret;
}