#pragma once
#include "stdint.h"
// #include "esp_err.h"
#include "dl_lib_coefgetter_if.h"
#include "esp_wn_iface.h"
// //Opaque model data container
// typedef struct model_iface_data_t model_iface_data_t;

/**
 * @brief Initialze a model instance with specified model coefficient.
 *
 * @param coeff       The wakenet model coefficient.
 * @param coeff        The wakenet model coefficient.
 * @parm sample_length Audio length for speech recognition, in ms.
 * @returns Handle to the model data.
 */
typedef model_iface_data_t* (*esp_mn_iface_op_create_t)(const model_coeff_getter_t *coeff, int sample_length);


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
 * @brief Set the detection threshold to manually abjust the probability 
 *
 * @param model The model object to query
 * @param phrase_id The ID of speech command phrase
 * @param det_treshold The threshold to trigger speech command phrases
 */
typedef void (*esp_mn_iface_op_set_command_det_threshold_t)(model_iface_data_t *model, int phrase_id, float det_threshold);

/**
 * @brief Get the detection threshold by phrase ID 
 *
 * @param model The model object to query
 * @param phrase_id The ID of speech command phrase
 * 
 * @return The threshold of speech command phrases
 */
typedef float (*esp_mn_iface_op_get_command_det_threshold_t)(model_iface_data_t *model, int phrase_id);

/**
 * @brief Get the sample rate of the samples to feed to the detect function
 *
 * @param model       The model object to query
 * @return The sample rate, in hz
 */
typedef int (*esp_mn_iface_op_get_samp_rate_t)(model_iface_data_t *model);

/**
 * @brief Feed samples of an audio stream to the speech recognition model and detect if there is a speech command found.
 *
 * @param model       The model object to query.
 * @param samples     An array of 16-bit signed audio samples. The array size used can be queried by the 
 *                    get_samp_chunksize function.
 * @return The command id, return 0 if no command word is detected,
 */
typedef int (*esp_mn_iface_op_detect_t)(model_iface_data_t *model, int16_t *samples);


/**
 * @brief Destroy a speech commands recognition model
 *
 * @param model       The Model object to destroy
 */
typedef void (*esp_mn_iface_op_destroy_t)(model_iface_data_t *model);

/**
 * @brief Reset the speech commands recognition model
 *
 */
typedef void (*esp_mn_iface_op_reset_t)(model_iface_data_t *model_data, char *command_str, char *err_phrase_id);


typedef struct {
    esp_mn_iface_op_create_t create;
    esp_mn_iface_op_get_samp_rate_t get_samp_rate;
    esp_mn_iface_op_get_samp_chunksize_t get_samp_chunksize;
    esp_mn_iface_op_get_samp_chunknum_t get_samp_chunknum;
    esp_mn_iface_op_set_det_threshold_t set_det_threshold;
    esp_mn_iface_op_set_command_det_threshold_t set_command_det_threshold;
    esp_mn_iface_op_get_command_det_threshold_t get_command_det_threshold;
    esp_mn_iface_op_detect_t detect; 
    esp_mn_iface_op_destroy_t destroy;
    esp_mn_iface_op_reset_t reset;
} esp_mn_iface_t;
