/*
  This is a very basic driver for the ES8388 based on the driver written by
  me for NuttX. It is not complete and is missing master mode, mono mode, many
  features and configuration options. You can use readReg and writeReg to
  access the registers directly and configure missing features. Feel free to
  contribute by adding missing features and improving the driver.
  It is intended to be used only with arduino-esp32.

  The default configuration can be found in the ES8388.h file.

  This was only tested with the ESP32-LyraT board using 44100Hz 16-bit stereo
  audio. It may not work with other configurations.

  Created for arduino-esp32 on 20 Dec, 2023
  by Lucas Saavedra Vaz (lucasssvaz)
*/

#include <cmath>

#include "ESP_I2S.h"
#include "Wire.h"

#include "ES8388.h"

/****************************************************************************
 * Private Methods
 ****************************************************************************/

/*
  Name: start

  Description:
    Unmute and start the ES8388 codec data transmission.
*/

void ES8388::start() {
  uint8_t prev_regval = 0;
  uint8_t regval = 0;

  log_v("Starting ES8388 transmission...");
  _running = true;
  prev_regval = readReg(ES8388_DACCONTROL21);

  if (_audio_mode == ES_MODULE_LINE) {
    writeReg(ES8388_DACCONTROL16, ES8388_RMIXSEL_RIN2 | ES8388_LMIXSEL_LIN2);

    writeReg(ES8388_DACCONTROL17, ES8388_LI2LOVOL(ES8388_MIXER_GAIN_0DB) | ES8388_LI2LO_ENABLE | ES8388_LD2LO_DISABLE);

    writeReg(ES8388_DACCONTROL20, ES8388_RI2ROVOL(ES8388_MIXER_GAIN_0DB) | ES8388_RI2RO_ENABLE | ES8388_RD2RO_DISABLE);

    writeReg(
      ES8388_DACCONTROL21,
      ES8388_DAC_DLL_PWD_NORMAL | ES8388_ADC_DLL_PWD_NORMAL | ES8388_MCLK_DIS_NORMAL | ES8388_OFFSET_DIS_DISABLE | ES8388_LRCK_SEL_ADC | ES8388_SLRCK_SAME
    );
  } else {
    writeReg(
      ES8388_DACCONTROL21,
      ES8388_DAC_DLL_PWD_NORMAL | ES8388_ADC_DLL_PWD_NORMAL | ES8388_MCLK_DIS_NORMAL | ES8388_OFFSET_DIS_DISABLE | ES8388_LRCK_SEL_DAC | ES8388_SLRCK_SAME
    );
  }

  regval = readReg(ES8388_DACCONTROL21);

  if (regval != prev_regval) {
    writeReg(
      ES8388_CHIPPOWER, ES8388_DACVREF_PDN_PWRUP | ES8388_ADCVREF_PDN_PWRUP | ES8388_DACDLL_PDN_NORMAL | ES8388_ADCDLL_PDN_NORMAL | ES8388_DAC_STM_RST_RESET
                          | ES8388_ADC_STM_RST_RESET | ES8388_DAC_DIGPDN_RESET | ES8388_ADC_DIGPDN_RESET
    );

    writeReg(
      ES8388_CHIPPOWER, ES8388_DACVREF_PDN_PWRUP | ES8388_ADCVREF_PDN_PWRUP | ES8388_DACDLL_PDN_NORMAL | ES8388_ADCDLL_PDN_NORMAL | ES8388_DAC_STM_RST_NORMAL
                          | ES8388_ADC_STM_RST_NORMAL | ES8388_DAC_DIGPDN_NORMAL | ES8388_ADC_DIGPDN_NORMAL
    );
  }

  if (_audio_mode == ES_MODULE_LINE || _audio_mode == ES_MODULE_ADC_DAC || _audio_mode == ES_MODULE_ADC) {
    writeReg(
      ES8388_ADCPOWER, ES8388_INT1LP_NORMAL | ES8388_FLASHLP_NORMAL | ES8388_PDNADCBIASGEN_NORMAL | ES8388_PDNMICB_PWRON | ES8388_PDNADCR_PWRUP
                         | ES8388_PDNADCL_PWRUP | ES8388_PDNAINR_NORMAL | ES8388_PDNAINL_NORMAL
    );
  }

  if (_audio_mode == ES_MODULE_LINE || _audio_mode == ES_MODULE_ADC_DAC || _audio_mode == ES_MODULE_DAC) {
    writeReg(
      ES8388_DACPOWER, ES8388_ROUT2_ENABLE | ES8388_LOUT2_ENABLE | ES8388_ROUT1_ENABLE | ES8388_LOUT1_ENABLE | ES8388_PDNDACR_PWRUP | ES8388_PDNDACL_PWRUP
    );
  }

  setmute(_audio_mode, false);
  log_v("ES8388 transmission started.");
}

/*
  Name: reset

  Description:
    Reset the ES8388 codec to a known state depending on the audio mode.
*/

void ES8388::reset() {
  uint8_t regconfig;

  log_v("Resetting ES8388...");
  log_d(
    "Current configuration: _bpsamp=%d, _samprate=%d, _nchannels=%d, _audio_mode=%d, _dac_output=%d, _adc_input=%d, _mic_gain=%d", _bpsamp, _samprate,
    _nchannels, _audio_mode, _dac_output, _adc_input, _mic_gain
  );

  writeReg(ES8388_DACCONTROL3, ES8388_DACMUTE_MUTED | ES8388_DACLER_NORMAL | ES8388_DACSOFTRAMP_DISABLE | ES8388_DACRAMPRATE_4LRCK);

  writeReg(
    ES8388_CONTROL2,
    ES8388_PDNVREFBUF_NORMAL | ES8388_VREFLO_NORMAL | ES8388_PDNIBIASGEN_NORMAL | ES8388_PDNANA_NORMAL | ES8388_LPVREFBUF_LP | ES8388_LPVCMMOD_NORMAL | (1 << 6)
  ); /* Default value of undocumented bit */

  writeReg(
    ES8388_CHIPPOWER, ES8388_DACVREF_PDN_PWRUP | ES8388_ADCVREF_PDN_PWRUP | ES8388_DACDLL_PDN_NORMAL | ES8388_ADCDLL_PDN_NORMAL | ES8388_DAC_STM_RST_NORMAL
                        | ES8388_ADC_STM_RST_NORMAL | ES8388_DAC_DIGPDN_NORMAL | ES8388_ADC_DIGPDN_NORMAL
  );

  /* Disable the internal DLL to improve 8K sample rate */

  writeReg(0x35, 0xa0);
  writeReg(0x37, 0xd0);
  writeReg(0x39, 0xd0);

  writeReg(ES8388_MASTERMODE, ES8388_BCLKDIV(ES_MCLK_DIV_AUTO) | ES8388_BCLK_INV_NORMAL | ES8388_MCLKDIV2_NODIV | ES8388_MSC_SLAVE);

  writeReg(
    ES8388_DACPOWER, ES8388_ROUT2_DISABLE | ES8388_LOUT2_DISABLE | ES8388_ROUT1_DISABLE | ES8388_LOUT1_DISABLE | ES8388_PDNDACR_PWRDN | ES8388_PDNDACL_PWRDN
  );

  writeReg(
    ES8388_CONTROL1, ES8388_VMIDSEL_500K | ES8388_ENREF_DISABLE | ES8388_SEQEN_DISABLE | ES8388_SAMEFS_SAME | ES8388_DACMCLK_ADCMCLK | ES8388_LRCM_ISOLATED
                       | ES8388_SCPRESET_NORMAL
  );

  setBitsPerSample(_bpsamp);
  setSampleRate(_samprate);

  writeReg(ES8388_DACCONTROL16, ES8388_RMIXSEL_RIN1 | ES8388_LMIXSEL_LIN1);

  writeReg(ES8388_DACCONTROL17, ES8388_LI2LOVOL(ES8388_MIXER_GAIN_0DB) | ES8388_LI2LO_DISABLE | ES8388_LD2LO_ENABLE);

  writeReg(ES8388_DACCONTROL20, ES8388_RI2ROVOL(ES8388_MIXER_GAIN_0DB) | ES8388_RI2RO_DISABLE | ES8388_RD2RO_ENABLE);

  writeReg(
    ES8388_DACCONTROL21,
    ES8388_DAC_DLL_PWD_NORMAL | ES8388_ADC_DLL_PWD_NORMAL | ES8388_MCLK_DIS_NORMAL | ES8388_OFFSET_DIS_DISABLE | ES8388_LRCK_SEL_DAC | ES8388_SLRCK_SAME
  );

  writeReg(ES8388_DACCONTROL23, ES8388_VROI_1_5K);

  writeReg(ES8388_DACCONTROL24, ES8388_LOUT1VOL(ES8388_DAC_CHVOL_DB(0)));

  writeReg(ES8388_DACCONTROL25, ES8388_ROUT1VOL(ES8388_DAC_CHVOL_DB(0)));

  writeReg(ES8388_DACCONTROL26, ES8388_LOUT2VOL(ES8388_DAC_CHVOL_DB(0)));

  writeReg(ES8388_DACCONTROL27, ES8388_ROUT2VOL(ES8388_DAC_CHVOL_DB(0)));

  setmute(ES_MODULE_DAC, ES8388_DEFAULT_MUTE);
  setvolume(ES_MODULE_DAC, ES8388_DEFAULT_VOL_OUT, ES8388_DEFAULT_BALANCE);

  if (_dac_output == ES8388_DAC_OUTPUT_LINE2) {
    regconfig = ES_DAC_CHANNEL_LOUT1 | ES_DAC_CHANNEL_ROUT1;
  } else if (_dac_output == ES8388_DAC_OUTPUT_LINE1) {
    regconfig = ES_DAC_CHANNEL_LOUT2 | ES_DAC_CHANNEL_ROUT2;
  } else {
    regconfig = ES_DAC_CHANNEL_LOUT1 | ES_DAC_CHANNEL_ROUT1 | ES_DAC_CHANNEL_LOUT2 | ES_DAC_CHANNEL_ROUT2;
  }

  writeReg(ES8388_DACPOWER, regconfig);

  writeReg(
    ES8388_ADCPOWER, ES8388_INT1LP_LP | ES8388_FLASHLP_LP | ES8388_PDNADCBIASGEN_LP | ES8388_PDNMICB_PWRDN | ES8388_PDNADCR_PWRDN | ES8388_PDNADCL_PWRDN
                       | ES8388_PDNAINR_PWRDN | ES8388_PDNAINL_PWRDN
  );

  setMicGain(24); /* +24 dB */

  if (_adc_input == ES8388_ADC_INPUT_LINE1) {
    regconfig = ES_ADC_CHANNEL_LINPUT1_RINPUT1;
  } else if (_adc_input == ES8388_ADC_INPUT_LINE2) {
    regconfig = ES_ADC_CHANNEL_LINPUT2_RINPUT2;
  } else {
    regconfig = ES_ADC_CHANNEL_DIFFERENCE;
  }

  writeReg(ES8388_ADCCONTROL2, regconfig);

  writeReg(
    ES8388_ADCCONTROL3, (1 << 1) | /* Default value of undocumented bit */
                          ES8388_TRI_NORMAL | ES8388_MONOMIX_STEREO | ES8388_DS_LINPUT1_RINPUT1
  );

  setBitsPerSample(_bpsamp);
  setSampleRate(_samprate);
  setmute(ES_MODULE_ADC, ES8388_DEFAULT_MUTE);
  setvolume(ES_MODULE_ADC, ES8388_DEFAULT_VOL_IN, ES8388_DEFAULT_BALANCE);

  writeReg(
    ES8388_ADCPOWER, ES8388_INT1LP_LP | ES8388_FLASHLP_NORMAL | ES8388_PDNADCBIASGEN_NORMAL | ES8388_PDNMICB_PWRDN | ES8388_PDNADCR_PWRUP | ES8388_PDNADCL_PWRUP
                       | ES8388_PDNAINR_NORMAL | ES8388_PDNAINL_NORMAL
  );

  /* Stop sequence to avoid noise at boot */

  stop();

  log_v("ES8388 reset.");
}

/*
  Name: stop

  Description:
    Mute and stop the ES8388 codec data transmission.
*/

void ES8388::stop() {
  log_v("Stopping ES8388 transmission...");
  _running = false;

  if (_audio_mode == ES_MODULE_LINE) {
    writeReg(
      ES8388_DACCONTROL21,
      ES8388_DAC_DLL_PWD_NORMAL | ES8388_ADC_DLL_PWD_NORMAL | ES8388_MCLK_DIS_NORMAL | ES8388_OFFSET_DIS_DISABLE | ES8388_LRCK_SEL_DAC | ES8388_SLRCK_SAME
    );

    writeReg(ES8388_DACCONTROL16, ES8388_RMIXSEL_RIN1 | ES8388_LMIXSEL_LIN1);

    writeReg(ES8388_DACCONTROL17, ES8388_LI2LOVOL(ES8388_MIXER_GAIN_0DB) | ES8388_LI2LO_DISABLE | ES8388_LD2LO_ENABLE);

    writeReg(ES8388_DACCONTROL20, ES8388_RI2ROVOL(ES8388_MIXER_GAIN_0DB) | ES8388_RI2RO_DISABLE | ES8388_RD2RO_ENABLE);

    goto stop_msg;
  }

  if (_audio_mode == ES_MODULE_DAC || _audio_mode == ES_MODULE_ADC_DAC) {
    writeReg(
      ES8388_DACPOWER, ES8388_ROUT2_DISABLE | ES8388_LOUT2_DISABLE | ES8388_ROUT1_DISABLE | ES8388_LOUT1_DISABLE | ES8388_PDNDACR_PWRUP | ES8388_PDNDACL_PWRUP
    );
  }

  if (_audio_mode == ES_MODULE_ADC || _audio_mode == ES_MODULE_ADC_DAC) {
    writeReg(
      ES8388_ADCPOWER, ES8388_INT1LP_LP | ES8388_FLASHLP_LP | ES8388_PDNADCBIASGEN_LP | ES8388_PDNMICB_PWRDN | ES8388_PDNADCR_PWRDN | ES8388_PDNADCL_PWRDN
                         | ES8388_PDNAINR_PWRDN | ES8388_PDNAINL_PWRDN
    );
  }

  if (_audio_mode == ES_MODULE_ADC_DAC) {
    writeReg(
      ES8388_DACCONTROL21,
      ES8388_DAC_DLL_PWD_PWRDN | ES8388_ADC_DLL_PWD_PWRDN | ES8388_MCLK_DIS_DISABLE | ES8388_OFFSET_DIS_DISABLE | ES8388_LRCK_SEL_DAC | ES8388_SLRCK_SAME
    );
  }

stop_msg:
  setmute(_audio_mode, true);
  log_v("ES8388 transmission stopped.");
}

/*
  Name: setvolume

  Description:
    Set the volume of the ES8388 codec.

  Input Parameters:
    module - Module to set the volume for.
    volume - Volume level {0..1000}.
    balance - Balance level {0..1000}.
*/

void ES8388::setvolume(es_module_e module, uint16_t volume, uint16_t balance) {
  uint16_t leftlvl;
  int16_t dbleftlvl;
  uint16_t rightlvl;
  int16_t dbrightlvl;

  log_d("Volume = %u, Balance = %u", volume, balance);

  if (volume > 1000) {
    log_w("Warning: Volume greater than 1000, setting to 1000.");
    volume = 1000;
  }

  if (balance > 1000) {
    log_w("Warning: Balance greater than 1000, setting to 1000.");
    balance = 1000;
  }

  _balance = balance;

  /* Calculate the left channel volume level {0..1000} */

  if (_balance <= 500) {
    leftlvl = volume;
  } else if (_balance == 1000) {
    leftlvl = 0;
  } else {
    leftlvl = ((((1000 - _balance) * 100) / 500) * volume) / 100;
  }

  /* Calculate the right channel volume level {0..1000} */

  if (_balance >= 500) {
    rightlvl = volume;
  } else if (_balance == 0) {
    rightlvl = 0;
  } else {
    rightlvl = (((_balance * 100) / 500) * volume) / 100;
  }

  /* Convert from (0..1000) to (-96..0) */

  dbleftlvl = (int16_t)(leftlvl ? (20 * log10f((float)rightlvl / 1000)) : -96);
  dbrightlvl = (int16_t)(rightlvl ? (20 * log10f((float)rightlvl / 1000)) : -96);

  log_v("Volume: dbleftlvl = %d, dbrightlvl = %d", dbleftlvl, dbrightlvl);

  /* Convert and truncate to 1 byte */

  dbleftlvl = ((-dbleftlvl) << 1) & 0xff;
  dbrightlvl = ((-dbrightlvl) << 1) & 0xff;

  /* Set the volume */

  if (module == ES_MODULE_DAC || module == ES_MODULE_ADC_DAC) {
    writeReg(ES8388_DACCONTROL4, ES8388_LDACVOL(dbleftlvl));
    writeReg(ES8388_DACCONTROL5, ES8388_RDACVOL(dbrightlvl));
    _volume_out = volume;
  }

  if (module == ES_MODULE_ADC || module == ES_MODULE_ADC_DAC) {
    writeReg(ES8388_ADCCONTROL8, ES8388_LADCVOL(dbleftlvl));
    writeReg(ES8388_ADCCONTROL9, ES8388_RADCVOL(dbrightlvl));
    _volume_in = volume;
  }
}

/*
  Name: setmute

  Description:
    Mute or unmute the selected ES8388 codec module.

  Input Parameters:
    module - Module to mute or unmute.
    enable - Mute or unmute.
*/

void ES8388::setmute(es_module_e module, bool enable) {
  uint8_t reg = 0;

  log_d("module=%d, mute=%d", module, (int)enable);

  _mute = enable;

  if (module == ES_MODULE_DAC || module == ES_MODULE_ADC_DAC) {
    reg = readReg(ES8388_DACCONTROL3) & (~ES8388_DACMUTE_BITMASK);
    writeReg(ES8388_DACCONTROL3, reg | ES8388_DACMUTE(enable));
  }

  if (module == ES_MODULE_ADC || module == ES_MODULE_ADC_DAC) {
    reg = readReg(ES8388_ADCCONTROL7) & (~ES8388_ADCMUTE_BITMASK);
    writeReg(ES8388_ADCCONTROL7, reg | ES8388_ADCMUTE(enable));
  }
}

/****************************************************************************
 * Public Methods
 ****************************************************************************/

ES8388::~ES8388() {
  end();
}

/*
  Name: begin

  Description:
    Initialize the ES8388 codec. Requires the I2S and I2C buses to be initialized
    before calling this function.

  Input Parameters:
    i2s - I2S bus instance.
    i2c - I2C bus instance. Defaults to Wire.
    addr - I2C address of the ES8388 codec. Defaults to 0x10.

  Return:
    true - Success.
    false - Failure.
*/
bool ES8388::begin(I2SClass &i2s, TwoWire &i2c, uint8_t addr) {
  log_v("Initializing ES8388...");

  _i2c = &i2c;
  _i2s = &i2s;
  _addr = addr;
  _fmt = ES8388_DEFAULT_FMT;
  _mode = ES8388_DEFAULT_MODE;
  _samprate = ES8388_DEFAULT_SAMPRATE;
  _nchannels = ES8388_DEFAULT_NCHANNELS;
  _bpsamp = ES8388_DEFAULT_BPSAMP;
  _audio_mode = ES8388_DEFAULT_AUDIO_MODE;
  _dac_output = ES8388_DEFAULT_DAC_OUTPUT;
  _adc_input = ES8388_DEFAULT_ADC_INPUT;
  _mic_gain = ES8388_DEFAULT_MIC_GAIN;
  _running = false;
  _lclk_div = ES_LCLK_DIV_256;
  _word_length = ES_WORD_LENGTH_16BITS;

  _i2c->beginTransmission(_addr);

  if (_i2c->endTransmission() != 0) {
    log_e("Device not found at address 0x%02x. Check if the I2C and I2S buses are initialized.", _addr);
    return false;
  }

  reset();
  log_v("ES8388 initialized.");
  return true;
}

/*
  Name: end

  Description:
    Stop the ES8388 codec and reset it to a known state.
*/

void ES8388::end() {
  log_v("Ending ES8388...");

  stop();
  setmute(ES_MODULE_ADC_DAC, true);
  _audio_mode = ES_MODULE_ADC_DAC;
  reset();

  log_v("ES8388 ended.");
}

/*
  Name: readReg

  Description:
    Read a register from the ES8388 codec.

  Input Parameters:
    reg - Register address.

  Return:
    Register value.
*/

uint8_t ES8388::readReg(uint8_t reg) {
  int data;

  _i2c->beginTransmission(_addr);
  if (_i2c->write(reg) == 0) {
    log_e("Error writing register address 0x%02x.", reg);
    return 0;
  }

  if (_i2c->endTransmission(false) != 0) {
    log_e("Error ending transmission.");
    return 0;
  }

  if (!_i2c->requestFrom(_addr, (uint8_t)1)) {
    log_e("Error requesting data.");
    return 0;
  }

  if ((data = _i2c->read()) < 0) {
    log_e("Error reading data.");
    return 0;
  }

  return (uint8_t)data;
}

/*
  Name: writeReg

  Description:
    Write a register to the ES8388 codec.

  Input Parameters:
    reg - Register address.
    data - Data to write.
*/

void ES8388::writeReg(uint8_t reg, uint8_t data) {
  _i2c->beginTransmission(_addr);

  if (_i2c->write(reg) == 0) {
    log_e("Error writing register address 0x%02x.", reg);
    return;
  }

  if (_i2c->write(data) == 0) {
    log_e("Error writing data 0x%02x.", data);
    return;
  }

  if (_i2c->endTransmission(true) != 0) {
    log_e("Error ending transmission.");
    return;
  }
}

/*
  Name: setMicGain

  Description:
    Set the microphone gain.

  Input Parameters:
    gain - Gain level in dB {0..24}.
*/

void ES8388::setMicGain(uint8_t gain) {
  static const es_mic_gain_e gain_map[] = {
    ES_MIC_GAIN_0DB,  ES_MIC_GAIN_3DB,  ES_MIC_GAIN_6DB,  ES_MIC_GAIN_9DB,  ES_MIC_GAIN_12DB,
    ES_MIC_GAIN_15DB, ES_MIC_GAIN_18DB, ES_MIC_GAIN_21DB, ES_MIC_GAIN_24DB,
  };

  log_d("gain=%d", gain);

  _mic_gain = gain_map[min(gain, (uint8_t)24) / 3];

  writeReg(ES8388_ADCCONTROL1, ES8388_MICAMPR(_mic_gain) | ES8388_MICAMPL(_mic_gain));

  log_v("Mic gain set to %d", _mic_gain);
}

/*
  Name: setBitsPerSample

  Description:
    Set the number of bits per sample. This also configures the I2S bus.

  Input Parameters:
    bpsamp - Bits per sample {16, 24, 32}.
*/

void ES8388::setBitsPerSample(uint8_t bpsamp) {
  /* ES8388 also supports 18 and 20 bits per sample, but the I2S bus does not */
  switch (bpsamp) {
    case 16: _word_length = ES_WORD_LENGTH_16BITS; break;

    case 24: _word_length = ES_WORD_LENGTH_24BITS; break;

    case 32: _word_length = ES_WORD_LENGTH_32BITS; break;

    default: log_e("Data length not supported."); return;
  }

  _bpsamp = bpsamp;
  _i2s->configureTX(_samprate, (i2s_data_bit_width_t)_bpsamp, I2S_SLOT_MODE_STEREO);
  _i2s->configureRX(_samprate, (i2s_data_bit_width_t)_bpsamp, I2S_SLOT_MODE_STEREO);

  if (_audio_mode == ES_MODULE_ADC || _audio_mode == ES_MODULE_ADC_DAC) {
    writeReg(ES8388_ADCCONTROL4, ES8388_ADCFORMAT(ES_I2S_NORMAL) | ES8388_ADCWL(_word_length) | ES8388_ADCLRP_NORM_2ND | ES8388_DATSEL_LL);
  }

  if (_audio_mode == ES_MODULE_DAC || _audio_mode == ES_MODULE_ADC_DAC) {
    writeReg(ES8388_DACCONTROL1, ES8388_DACFORMAT(ES_I2S_NORMAL) | ES8388_DACWL(_word_length) | ES8388_DACLRP_NORM_2ND | ES8388_DACLRSWAP_NORMAL);
  }

  log_v("Datawidth set to %u", _bpsamp);
}

/*
  Name: setSampleRate

  Description:
    Set the sample rate. This also configures the I2S bus.
    The divider depends on the sample rate and the MCLK frequency.
    This needs to be re-implemented to properly support all cases.
    ES8388 should also support 88100Hz and 96000Hz sample rates in
    double speed mode but setting it makes the audio sound distorted.

  Input Parameters:
    rate - Sample rate {8000, 11025, 12000, 16000, 22050, 24000, 32000,
           44100, 48000}.
*/

void ES8388::setSampleRate(uint32_t rate) {
  /*
    According to the datasheet, this should only matter for the master mode
    but it seems to affect the slave mode as well.
  */

  switch (rate) {
    case 8000: _lclk_div = ES_LCLK_DIV_1536; break;

    case 11025:
    case 12000: _lclk_div = ES_LCLK_DIV_1024; break;

    case 16000: _lclk_div = ES_LCLK_DIV_768; break;

    case 22050:
    case 24000: _lclk_div = ES_LCLK_DIV_512; break;

    case 32000: _lclk_div = ES_LCLK_DIV_384; break;

    case 44100:
    case 48000: _lclk_div = ES_LCLK_DIV_256; break;

    default: log_e("Sample rate not supported."); return;
  }

  _samprate = rate;
  _i2s->configureTX(_samprate, (i2s_data_bit_width_t)_bpsamp, I2S_SLOT_MODE_STEREO);
  _i2s->configureRX(_samprate, (i2s_data_bit_width_t)_bpsamp, I2S_SLOT_MODE_STEREO);

  if (_audio_mode == ES_MODULE_ADC || _audio_mode == ES_MODULE_ADC_DAC) {
    writeReg(ES8388_ADCCONTROL5, ES8388_ADCFSRATIO(_lclk_div));
  }

  if (_audio_mode == ES_MODULE_DAC || _audio_mode == ES_MODULE_ADC_DAC) {
    writeReg(ES8388_DACCONTROL2, ES8388_DACFSRATIO(_lclk_div));
  }

  log_v("Sample rate set to %d in single speed mode", _samprate);
}

/*
  Name: playWAV

  Description:
    Wrapper for the I2SClass::playWAV() method. This method starts the ES8388
    codec before playing the WAV file and stops it after the WAV file has been
    played.

  Input Parameters:
    data - Pointer to the WAV file data.
    len - Length of the WAV file data.
*/

void ES8388::playWAV(uint8_t *data, size_t len) {
  _audio_mode = ES_MODULE_DAC;
  reset();

  log_v("Playing WAV file...");
  start();
  _i2s->playWAV(data, len);
  stop();
  log_v("WAV file played.");
}

/*
  Name: recordWAV

  Description:
    Wrapper for the I2SClass::recordWAV() method. This method starts the ES8388
    codec before recording the WAV file and stops it after the WAV file has been
    recorded.

  Input Parameters:
    rec_seconds - Length of the WAV file to record in seconds.
    out_size - Pointer to the variable that will hold the size of the WAV file.

  Return:
    Pointer to the WAV file data.
*/

uint8_t *ES8388::recordWAV(size_t rec_seconds, size_t *out_size) {
  uint8_t *data;
  size_t size;

  _audio_mode = ES_MODULE_ADC;
  reset();

  log_v("Recording WAV file...");
  start();
  data = _i2s->recordWAV(rec_seconds, &size);
  stop();
  log_v("WAV file recorded.");

  *out_size = size;
  return data;
}

void ES8388::setOutputVolume(uint16_t volume, uint16_t balance) {
  setvolume(ES_MODULE_DAC, volume, balance);
}

void ES8388::setInputVolume(uint16_t volume, uint16_t balance) {
  setvolume(ES_MODULE_ADC, volume, balance);
}

void ES8388::setOutputMute(bool enable) {
  setmute(ES_MODULE_DAC, enable);
}

void ES8388::setInputMute(bool enable) {
  setmute(ES_MODULE_ADC, enable);
}
