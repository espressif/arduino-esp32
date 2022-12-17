#pragma once
#include "stdint.h"
#include "esp_wn_iface.h"

#define ESP_MN_RESULT_MAX_NUM 5
#define ESP_MN_MAX_PHRASE_NUM 200
#define ESP_MN_MAX_PHRASE_LEN 63
#define ESP_MN_MIN_PHRASE_LEN 2

#define ESP_MN_PREFIX "mn"
#define ESP_MN_ENGLISH "en"
#define ESP_MN_CHINESE "cn"

typedef enum {
	ESP_MN_STATE_DETECTING = 0,     // detecting
	ESP_MN_STATE_DETECTED = 1,      // detected
    ESP_MN_STATE_TIMEOUT = 2,       // time out
} esp_mn_state_t;

// Return all possible recognition results
typedef struct{
    esp_mn_state_t state;
    int num;                                   // The number of phrase in list, num<=5. When num=0, no phrase is recognized.
    int command_id[ESP_MN_RESULT_MAX_NUM];     // The list of command id.
    int phrase_id[ESP_MN_RESULT_MAX_NUM];      // The list of phrase id.
    float prob[ESP_MN_RESULT_MAX_NUM];         // The list of probability.
} esp_mn_results_t;

typedef struct{
    int16_t num;                                // The number of error phrases, which can not added into model
    int16_t phrase_idx[ESP_MN_MAX_PHRASE_NUM];  // The error phrase index in singly linked list．
} esp_mn_error_t;

typedef struct {
    char phoneme_string[ESP_MN_MAX_PHRASE_LEN + 1];  // phoneme string
    int16_t command_id;                              // the command id
    float threshold;                                 // trigger threshold, default: 0
    int16_t *wave;                                   // prompt wave data of the phrase
} esp_mn_phrase_t;

typedef struct _mn_node_ {
    esp_mn_phrase_t *phrase;
    struct _mn_node_ *next;
} esp_mn_node_t;

/**
 * @brief Initialze a model instance with specified model name.
 *
 * @param model_name  The wakenet model name.
 * @param duration    The duration (ms) to trigger the timeout
 *
 * @returns Handle to the model data.
 */
typedef model_iface_data_t* (*esp_mn_iface_op_create_t)(const char *model_name, int duration);

/**
 * @brief Callback function type to fetch the amount of samples that need to be passed to the detect function
 *
 * Every speech recognition model processes a certain number of samples at the same time. This function
 * can be used to query that amount. Note that the returned amount is in 16-bit samples, not in bytes.
 *
 * @param model       The model object to query
 * @return The amount of samples to feed the detect function
 */
typedef int (*esp_mn_iface_op_get_samp_chunksize_t)(model_iface_data_t *model);

/**
 * @brief Callback function type to fetch the number of frames recognized by the command word
 *
 * @param model       The model object to query
 * @return The number of the frames recognized by the command word
 */
typedef int (*esp_mn_iface_op_get_samp_chunknum_t)(model_iface_data_t *model);

/**
 * @brief Set the detection threshold to manually abjust the probability 
 *
 * @param model The model object to query
 * @param det_treshold The threshold to trigger speech commands, the range of det_threshold is 0.0~0.9999
 */
typedef int (*esp_mn_iface_op_set_det_threshold_t)(model_iface_data_t *model, float det_threshold);

/**
 * @brief Get the sample rate of the samples to feed to the detect function
 *
 * @param model       The model object to query
 * @return The sample rate, in hz
 */
typedef int (*esp_mn_iface_op_get_samp_rate_t)(model_iface_data_t *model);

/**
 * @brief Get the language of model
 *
 * @param model       The language name 
 * @return Language name string defined in esp_mn_models.h, eg: ESP_MN_CHINESE, ESP_MN_ENGLISH
 */
typedef char * (*esp_mn_iface_op_get_language_t)(model_iface_data_t *model);

/**
 * @brief Feed samples of an audio stream to the speech recognition model and detect if there is a speech command found.
 *
 * @param model       The model object to query.
 * @param samples     An array of 16-bit signed audio samples. The array size used can be queried by the 
 *                    get_samp_chunksize function.
 * @return The state of multinet
 */
typedef esp_mn_state_t (*esp_mn_iface_op_detect_t)(model_iface_data_t *model, int16_t *samples);

/**
 * @brief Destroy a speech commands recognition model
 *
 * @param model       The Model object to destroy
 */
typedef void (*esp_mn_iface_op_destroy_t)(model_iface_data_t *model);

/**
 * @brief Get recognition results 
 *
 * @param model       The Model object to query
 * 
 * @return The current results.
 */
typedef esp_mn_results_t* (*esp_mn_iface_op_get_results_t)(model_iface_data_t *model);

/**
 * @brief Open the log print
 *
 * @param model_data       The model object to query.
 *
 */
typedef void (*esp_mn_iface_op_open_log_t)(model_iface_data_t *model_data);

/**
 * @brief Set the speech commands by mn_command_root
 *
 * @param model_data       The model object to query.
 * @param mn_command_root  The speech commands link.
 * @return The error phrase id info.
 */
typedef esp_mn_error_t* (*esp_wn_iface_op_set_speech_commands)(model_iface_data_t *model_data, esp_mn_node_t *mn_command_root);

typedef struct {
    esp_mn_iface_op_create_t create;
    esp_mn_iface_op_get_samp_rate_t get_samp_rate;
    esp_mn_iface_op_get_samp_chunksize_t get_samp_chunksize;
    esp_mn_iface_op_get_samp_chunknum_t get_samp_chunknum;
    esp_mn_iface_op_set_det_threshold_t set_det_threshold;
    esp_mn_iface_op_detect_t detect; 
    esp_mn_iface_op_destroy_t destroy;
    esp_mn_iface_op_get_results_t get_results;
    esp_mn_iface_op_open_log_t open_log;
    esp_wn_iface_op_set_speech_commands set_speech_commands;
} esp_mn_iface_t;
