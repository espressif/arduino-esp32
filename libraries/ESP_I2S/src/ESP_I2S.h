#pragma once
#include "Arduino.h"
#include "esp_err.h"
#include "driver/i2s_std.h"

typedef esp_err_t (*i2s_channel_read_fn)(i2s_chan_handle_t handle, char * tmp_buf, void *dest, size_t size, size_t *bytes_read, uint32_t timeout_ms);

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

    void setPins(int8_t bclk, int8_t ws, int8_t dout, int8_t din=-1, int8_t mclk=-1);
    void setInverted(bool bclk, bool ws, bool mclk=false);

    bool begin(uint32_t rate, i2s_data_bit_width_t bits_cfg, i2s_slot_mode_t ch);
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

    int8_t _mclk, _bclk, _ws, _dout, _din;
    bool _mclk_inv, _bclk_inv, _ws_inv;

    bool allocTranformRX(size_t buf_len);
    bool transformRX(i2s_rx_transform_t transform);
};
