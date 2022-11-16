#pragma once
#include "esp_mn_iface.h"

//Contains declarations of all available speech recognion models. Pair this up with the right coefficients and you have a model that can recognize
//a specific phrase or word.


/**
 * @brief Get the multinet handle from model name
 *
 * @param model_name   The name of model 
 * @returns The handle of multinet
 */
esp_mn_iface_t *esp_mn_handle_from_name(char *model_name);

/**
 * @brief Get the multinet language from model name
 *
 * @param model_name   The name of model 
 * @returns The language of multinet
 */
char *esp_mn_language_from_name(char *model_name);

/*
 Configure wake word to use based on what's selected in menuconfig.
*/

#ifdef CONFIG_SR_MN_CN_MULTINET2_SINGLE_RECOGNITION
#include "multinet2_ch.h"
#define MULTINET_COEFF get_coeff_multinet2_ch
#define MULTINET_MODEL_NAME "mn2_cn"

#else
#define MULTINET_COEFF      "COEFF_NULL"
#define MULTINET_MODEL_NAME "NULL"
#endif


/* example

static const esp_mn_iface_t *multinet = &MULTINET_MODEL;

//Initialize MultiNet model data
model_iface_data_t *model_data = multinet->create(&MULTINET_COEFF);
add_speech_commands(multinet, model_data);

//Set parameters of buffer
int audio_chunksize=model->get_samp_chunksize(model_data);
int frequency = model->get_samp_rate(model_data);
int16_t *buffer=malloc(audio_chunksize*sizeof(int16_t));

//Detect
int r=model->detect(model_data, buffer);
if (r>0) {
    printf("Detection triggered output %d.\n",  r);
}

//Destroy model
model->destroy(model_data)

*/
