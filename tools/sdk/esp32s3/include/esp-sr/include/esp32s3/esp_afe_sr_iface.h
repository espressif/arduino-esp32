#pragma once
#include "stdint.h"
#include "esp_afe_config.h"

//AFE: Audio Front-End 
//SR:  Speech Recognition
//afe_sr/AFE_SR: the audio front-end for speech recognition

//Opaque AFE_SR data container
typedef struct esp_afe_sr_data_t esp_afe_sr_data_t;

/**
 * @brief The state of vad
 */
typedef enum
{
    AFE_VAD_SILENCE = 0,                    // noise or silence
    AFE_VAD_SPEECH                          // speech
} afe_vad_state_t;

/**
 * @brief The result of fetch function
 */
typedef struct afe_fetch_result_t
{
    int16_t *data;                          // the data of audio.
    int data_size;                          // the size of data. The unit is byte.
    int wakeup_state;                       // the value is afe_wakeup_state_t
    int wake_word_index;                    // if the wake word is detected. It will store the wake word index which start from 1.
    int vad_state;                          // the value is afe_vad_state_t
    int trigger_channel_id;                 // the channel index of output
    int wake_word_length;                   // the length of wake word. It's unit is the number of samples.
    int ret_value;                          // the return state of fetch function
    void* reserved;                         // reserved for future use
} afe_fetch_result_t;

/**
 * @brief Function to initialze a AFE_SR instance
 * 
 * @param afe_config        The config of AFE_SR
 * @returns Handle to the AFE_SR data
 */
typedef esp_afe_sr_data_t* (*esp_afe_sr_iface_op_create_from_config_t)(afe_config_t *afe_config);

/**
 * @brief Get the amount of each channel samples per frame that need to be passed to the function
 *
 * Every speech enhancement AFE_SR processes a certain number of samples at the same time. This function
 * can be used to query that amount. Note that the returned amount is in 16-bit samples, not in bytes.
 *
 * @param afe The AFE_SR object to query
 * @return The amount of samples to feed the fetch function
 */
typedef int (*esp_afe_sr_iface_op_get_samp_chunksize_t)(esp_afe_sr_data_t *afe);

/**
 * @brief Get the total channel number which be config
 * 
 * @param afe   The AFE_SR object to query
 * @return      The amount of total channels
 */
typedef int (*esp_afe_sr_iface_op_get_total_channel_num_t)(esp_afe_sr_data_t *afe);

/**
 * @brief Get the mic channel number which be config
 * 
 * @param afe   The AFE_SR object to query
 * @return      The amount of mic channels
 */
typedef int (*esp_afe_sr_iface_op_get_channel_num_t)(esp_afe_sr_data_t *afe);

/**
 * @brief Get the sample rate of the samples to feed to the function
 *
 * @param afe   The AFE_SR object to query
 * @return      The sample rate, in hz
 */
typedef int (*esp_afe_sr_iface_op_get_samp_rate_t)(esp_afe_sr_data_t *afe);

/**
 * @brief Feed samples of an audio stream to the AFE_SR
 *
 * @Warning  The input data should be arranged in the format of channel interleaving.
 *           The last channel is reference signal if it has reference data.
 *
 * @param afe   The AFE_SR object to query
 * 
 * @param in    The input microphone signal, only support signed 16-bit @ 16 KHZ. The frame size can be queried by the 
 *              `get_feed_chunksize`.
 * @return      The size of input
 */
typedef int (*esp_afe_sr_iface_op_feed_t)(esp_afe_sr_data_t *afe, const int16_t* in);

/**
 * @brief fetch enhanced samples of an audio stream from the AFE_SR
 *
 * @Warning  The output is single channel data, no matter how many channels the input is.
 *
 * @param afe   The AFE_SR object to query
 * @return      The result of output, please refer to the definition of `afe_fetch_result_t`. (The frame size of output audio can be queried by the `get_fetch_chunksize`.)
 */
typedef afe_fetch_result_t* (*esp_afe_sr_iface_op_fetch_t)(esp_afe_sr_data_t *afe);

/**
 * @brief Initial wakenet and wake words coefficient, or reset wakenet and wake words coefficient 
 *        when wakenet has been initialized.  
 *
 * @param afe                The AFE_SR object to query
 * @param wakenet_word       The wakenet word, should be DEFAULT_WAKE_WORD or EXTRA_WAKE_WORD
 * @return             0: fail, 1: success
 */
typedef int (*esp_afe_sr_iface_op_set_wakenet_t)(esp_afe_sr_data_t *afe, char* model_name);

/**
 * @brief Disable wakenet model.
 *
 * @param afe          The AFE_SR object to query
 * @return             0: fail, 1: success
 */
typedef int (*esp_afe_sr_iface_op_disable_wakenet_t)(esp_afe_sr_data_t *afe);

/**
 * @brief Enable wakenet model.
 *
 * @param afe          The AFE_SR object to query
 * @return             0: fail, 1: success
 */
typedef int (*esp_afe_sr_iface_op_enable_wakenet_t)(esp_afe_sr_data_t *afe);

/**
 * @brief Disable AEC algorithm.
 *
 * @param afe          The AFE_SR object to query
 * @return             0: fail, 1: success
 */
typedef int (*esp_afe_sr_iface_op_disable_aec_t)(esp_afe_sr_data_t *afe);

/**
 * @brief Enable AEC algorithm.
 *
 * @param afe          The AFE_SR object to query
 * @return             0: fail, 1: success
 */
typedef int (*esp_afe_sr_iface_op_enable_aec_t)(esp_afe_sr_data_t *afe);

/**
 * @brief Disable SE algorithm.
 *
 * @param afe          The AFE_SR object to query
 * @return             0: fail, 1: success
 */
typedef int (*esp_afe_sr_iface_op_disable_se_t)(esp_afe_sr_data_t *afe);

/**
 * @brief Enable SE algorithm.
 *
 * @param afe          The AFE_SR object to query
 * @return             0: fail, 1: success
 */
typedef int (*esp_afe_sr_iface_op_enable_se_t)(esp_afe_sr_data_t *afe);

/**
 * @brief Destroy a AFE_SR instance
 *
 * @param afe         AFE_SR object to destroy
 */
typedef void (*esp_afe_sr_iface_op_destroy_t)(esp_afe_sr_data_t *afe);


/**
 * This structure contains the functions used to do operations on a AFE_SR.
 */
typedef struct {
    esp_afe_sr_iface_op_create_from_config_t create_from_config;
    esp_afe_sr_iface_op_feed_t feed;
    esp_afe_sr_iface_op_fetch_t fetch;
    esp_afe_sr_iface_op_get_samp_chunksize_t get_feed_chunksize;
    esp_afe_sr_iface_op_get_samp_chunksize_t get_fetch_chunksize;
    esp_afe_sr_iface_op_get_total_channel_num_t get_total_channel_num;
    esp_afe_sr_iface_op_get_channel_num_t get_channel_num;
    esp_afe_sr_iface_op_get_samp_rate_t get_samp_rate;
    esp_afe_sr_iface_op_set_wakenet_t  set_wakenet; 
    esp_afe_sr_iface_op_disable_wakenet_t disable_wakenet;
    esp_afe_sr_iface_op_enable_wakenet_t enable_wakenet;
    esp_afe_sr_iface_op_disable_aec_t disable_aec;
    esp_afe_sr_iface_op_enable_aec_t enable_aec;
    esp_afe_sr_iface_op_disable_se_t disable_se;
    esp_afe_sr_iface_op_enable_se_t enable_se;
    esp_afe_sr_iface_op_destroy_t destroy;
} esp_afe_sr_iface_t;
