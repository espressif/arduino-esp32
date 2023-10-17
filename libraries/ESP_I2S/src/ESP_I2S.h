#pragma once
#include "Arduino.h"
#include "esp_err.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/i2s_pdm.h"
#include "soc/soc_caps.h"

typedef esp_err_t (*i2s_channel_read_fn)(i2s_chan_handle_t handle, char * tmp_buf, void *dest, size_t size, size_t *bytes_read, uint32_t timeout_ms);

typedef enum {
    I2S_MODE_STD,
    I2S_MODE_TDM,
    I2S_MODE_PDM_TX,
    I2S_MODE_PDM_RX
} i2s_mode_t;

typedef enum {
    I2S_RX_TRANSFORM_NONE,
    I2S_RX_TRANSFORM_32_TO_16,
    I2S_RX_TRANSFORM_16_STEREO_TO_MONO,
    I2S_RX_TRANSFORM_MAX
} i2s_rx_transform_t;

class I2SClass: public Stream {
  public:
    I2SClass();
    ~I2SClass();

    //STD + TDM mode
    void setPins(int8_t bclk, int8_t ws, int8_t dout, int8_t din=-1, int8_t mclk=-1);
    void setInverted(bool bclk, bool ws, bool mclk=false);

    //PDM TX + PDM RX mode
    void setPinsPdmTx(int8_t clk, int8_t dout0, int8_t dout1=-1);
    void setPinsPdmRx(int8_t clk, int8_t din0, int8_t din1=-1, int8_t din2=-1, int8_t din3=-1);
    void setInvertedPdm(bool clk);

    bool begin(i2s_mode_t mode, uint32_t rate, i2s_data_bit_width_t bits_cfg, i2s_slot_mode_t ch, i2s_tdm_slot_mask_t slot_mask=I2S_TDM_SLOT0);
    bool configureTX(uint32_t rate, i2s_data_bit_width_t bits_cfg, i2s_slot_mode_t ch);
    bool configureRX(uint32_t rate, i2s_data_bit_width_t bits_cfg, i2s_slot_mode_t ch, i2s_rx_transform_t transform=I2S_RX_TRANSFORM_NONE);
    bool end();

    size_t readBytes(char *buffer, size_t size);
    size_t write(uint8_t *buffer, size_t size);

    i2s_chan_handle_t txChan();
    uint32_t txSampleRate();
    i2s_data_bit_width_t txDataWidth();
    i2s_slot_mode_t txSlotMode();

    i2s_chan_handle_t rxChan();
    uint32_t rxSampleRate();
    i2s_data_bit_width_t rxDataWidth();
    i2s_slot_mode_t rxSlotMode();

    int lastError();

    int available();
    int peek();
    int read();
    size_t write(uint8_t d);

    // Record short PCM WAV to memory with current RX settings. Returns buffer that must be freed by the user.
    uint8_t * recordWAV(size_t rec_seconds, size_t * out_size);
    // Play short PCM WAV from memory
    void playWAV(uint8_t * data, size_t len);
    // Play short MP3 from memory
    bool playMP3(uint8_t *src, size_t src_len);


  private:
    esp_err_t last_error;
    i2s_mode_t _mode;

    i2s_chan_handle_t tx_chan;
    uint32_t tx_sample_rate;
    i2s_data_bit_width_t tx_data_bit_width;
    i2s_slot_mode_t tx_slot_mode;

    i2s_channel_read_fn rx_fn;
    i2s_rx_transform_t rx_transform;
    char * rx_transform_buf;
    size_t rx_transform_buf_len;

    i2s_chan_handle_t rx_chan;
    uint32_t rx_sample_rate;
    i2s_data_bit_width_t rx_data_bit_width;
    i2s_slot_mode_t rx_slot_mode;

    //STD and TDM mode
    int8_t _mclk, _bclk, _ws, _dout, _din;
    bool _mclk_inv, _bclk_inv, _ws_inv;

    //PDM mode
    int8_t _rx_clk, _rx_din0, _rx_din1, _rx_din2, _rx_din3; //TODO: soc_caps.h 1/4
    bool _rx_clk_inv;
    int8_t _tx_clk, _tx_dout0, _tx_dout1;
    bool _tx_clk_inv;

    bool allocTranformRX(size_t buf_len);
    bool transformRX(i2s_rx_transform_t transform);

    static bool i2sDetachBus(void * bus_pointer);
    bool initSTD(uint32_t rate, i2s_data_bit_width_t bits_cfg, i2s_slot_mode_t ch);
    bool initTDM(uint32_t rate, i2s_data_bit_width_t bits_cfg, i2s_slot_mode_t ch, i2s_tdm_slot_mask_t slot_mask);
    bool initPDMtx(uint32_t rate, i2s_data_bit_width_t bits_cfg, i2s_slot_mode_t ch);
    bool initPDMrx(uint32_t rate, i2s_data_bit_width_t bits_cfg, i2s_slot_mode_t ch);
};
