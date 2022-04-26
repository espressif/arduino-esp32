#pragma once
#include "stdint.h"
#include "dl_lib_coefgetter_if.h"

//Opaque model data container
typedef struct model_iface_data_t model_iface_data_t;

//Set wake words recognition operating mode
//The probability of being wake words is increased with increasing mode, 
//As a consequence also the false alarm rate goes up
typedef enum {
	DET_MODE_90 = 0,       // Normal
	DET_MODE_95 = 1,       // Aggressive
    DET_MODE_2CH_90 = 2,
    DET_MODE_2CH_95 = 3,
    DET_MODE_3CH_90 = 4,
    DET_MODE_3CH_95 = 5,
} det_mode_t;

typedef struct {
    int wake_word_num;     //The number of all wake words
    char **wake_word_list; //The name list of wake words  
} wake_word_info_t;

/**
 * @brief Easy function type to initialze a model instance with a detection mode and specified wake word coefficient
 *
 * @param det_mode    The wake words detection mode to trigger wake words, DET_MODE_90 or DET_MODE_95
 * @param model_coeff The specified wake word model coefficient
 * @returns Handle to the model data
 */
typedef model_iface_data_t* (*esp_wn_iface_op_create_t)(const model_coeff_getter_t *model_coeff, det_mode_t det_mode);

/**
 * @brief Callback function type to fetch the amount of samples that need to be passed to the detect function
 *
 * Every speech recognition model processes a certain number of samples at the same time. This function
 * can be used to query that amount. Note that the returned amount is in 16-bit samples, not in bytes.
 *
 * @param model The model object to query
 * @return The amount of samples to feed the detect function
 */
typedef int (*esp_wn_iface_op_get_samp_chunksize_t)(model_iface_data_t *model);

/**
 * @brief Callback function type to fetch the channel number of samples that need to be passed to the detect function
 *
 * Every speech recognition model processes a certain number of samples at the same time. This function
 * can be used to query that amount. Note that the returned amount is in 16-bit samples, not in bytes.
 *
 * @param model The model object to query
 * @return The amount of samples to feed the detect function
 */
typedef int (*esp_wn_iface_op_get_channel_num_t)(model_iface_data_t *model);


/**
 * @brief Get the sample rate of the samples to feed to the detect function
 *
 * @param model The model object to query
 * @return The sample rate, in hz
 */
typedef int (*esp_wn_iface_op_get_samp_rate_t)(model_iface_data_t *model);

/**
 * @brief Get the number of wake words
 *
 * @param model The model object to query
 * @returns the number of wake words
 */
typedef int (*esp_wn_iface_op_get_word_num_t)(model_iface_data_t *model);

/**
 * @brief Get the name of wake word by index
 *
 * @Warning The index of wake word start with 1

 * @param model The model object to query
 * @param word_index The index of wake word
 * @returns the detection threshold
 */
typedef char* (*esp_wn_iface_op_get_word_name_t)(model_iface_data_t *model, int word_index);

/**
 * @brief Set the detection threshold to manually abjust the probability 
 *
 * @param model The model object to query
 * @param det_treshold The threshold to trigger wake words, the range of det_threshold is 0.5~0.9999
 * @param word_index The index of wake word
 * @return 0: setting failed, 1: setting success
 */
typedef int (*esp_wn_iface_op_set_det_threshold_t)(model_iface_data_t *model, float det_threshold, int word_index);

/**
 * @brief Get the wake word detection threshold of different modes
 *
 * @param model The model object to query
 * @param word_index The index of wake word
 * @returns the detection threshold
 */
typedef float (*esp_wn_iface_op_get_det_threshold_t)(model_iface_data_t *model, int word_index);

/**
 * @brief Feed samples of an audio stream to the keyword detection model and detect if there is a keyword found.
 *
 * @Warning The index of wake word start with 1, 0 means no wake words is detected.
 *
 * @param model The model object to query
 * @param samples An array of 16-bit signed audio samples. The array size used can be queried by the 
 *        get_samp_chunksize function.
 * @return The index of wake words, return 0 if no wake word is detected, else the index of the wake words.
 */
typedef int (*esp_wn_iface_op_detect_t)(model_iface_data_t *model, int16_t *samples);

/**
 * @brief Get the volume gain
 *
 * @param model The model object to query
 * @param target_db  The target dB to calculate volume gain
 * @returns the volume gain
 */
typedef float (*esp_wn_iface_op_get_vol_gain_t)(model_iface_data_t *model, float target_db);

/**
 * @brief Get the triggered channel index. Channel index starts from zero
 *
 * @param model The model object to query
 * @return The channel index
 */
typedef int (*esp_wn_iface_op_get_triggered_channel_t)(model_iface_data_t *model);

/**
 * @brief Clean all states of model
 *
 * @param model The model object to query
 */
typedef void (*esp_wn_iface_op_clean_t)(model_iface_data_t *model);

/**
 * @brief Destroy a speech recognition model
 *
 * @param model Model object to destroy
 */
typedef void (*esp_wn_iface_op_destroy_t)(model_iface_data_t *model);


/**
 * This structure contains the functions used to do operations on a wake word detection model.
 */
typedef struct {
    esp_wn_iface_op_create_t create;
    esp_wn_iface_op_get_samp_chunksize_t get_samp_chunksize;
    esp_wn_iface_op_get_channel_num_t get_channel_num;
    esp_wn_iface_op_get_samp_rate_t get_samp_rate;
    esp_wn_iface_op_get_word_num_t get_word_num;
    esp_wn_iface_op_get_word_name_t get_word_name;
    esp_wn_iface_op_set_det_threshold_t set_det_threshold;
    esp_wn_iface_op_get_det_threshold_t get_det_threshold;
    esp_wn_iface_op_get_triggered_channel_t  get_triggered_channel;
    esp_wn_iface_op_get_vol_gain_t get_vol_gain;
    esp_wn_iface_op_detect_t detect;
    esp_wn_iface_op_clean_t clean;
    esp_wn_iface_op_destroy_t destroy;
} esp_wn_iface_t;
