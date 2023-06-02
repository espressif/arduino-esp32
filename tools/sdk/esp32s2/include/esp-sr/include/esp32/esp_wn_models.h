#pragma once
#include "esp_wn_iface.h"


// The prefix of wakenet model name is used to filter all wakenet from availabel models.
#define ESP_WN_PREFIX "wn"

/**
 * @brief Get the wakenet handle from model name
 *
 * @param model_name   The name of model 
 * @returns The handle of wakenet
 */
const esp_wn_iface_t *esp_wn_handle_from_name(const char *model_name);

/**
 * @brief Get the wake word name from model name
 *
 * @param model_name   The name of model 
 * @returns The wake word name, like "alexa","hilexin","xiaoaitongxue"
 */
char* esp_wn_wakeword_from_name(const char *model_name);

// /**
//  * @brief Get the model coeff from model name
//  *
//  * @Warning: retuen model_coeff_getter_t, when chip is ESP32, 
//  *           return string for other chips
//  * 
//  * @param model_name   The name of model 
//  * @returns The handle of wakenet
//  */
// void *esp_wn_coeff_from_name(char *model_name);


#if defined CONFIG_USE_WAKENET
/*
 Configure wake word to use based on what's selected in menuconfig.
*/
#if CONFIG_SR_WN_WN5_HILEXIN
#include "hilexin_wn5.h"
#define WAKENET_MODEL_NAME "wn5_hilexin"
#define WAKENET_COEFF get_coeff_hilexin_wn5

#elif CONFIG_SR_WN_WN5X2_HILEXIN
#include "hilexin_wn5X2.h"
#define WAKENET_MODEL_NAME "wn5_hilexinX2"
#define WAKENET_COEFF get_coeff_hilexin_wn5X2


#elif CONFIG_SR_WN_WN5X3_HILEXIN
#include "hilexin_wn5X3.h"
#define WAKENET_MODEL_NAME "wn5_hilexinX3"
#define WAKENET_COEFF get_coeff_hilexin_wn5X3


#elif CONFIG_SR_WN_WN5_NIHAOXIAOZHI
#include "nihaoxiaozhi_wn5.h"
#define WAKENET_MODEL_NAME "wn5_nihaoxiaozhi"
#define WAKENET_COEFF get_coeff_nihaoxiaozhi_wn5


#elif CONFIG_SR_WN_WN5X2_NIHAOXIAOZHI
#include "nihaoxiaozhi_wn5X2.h"
#define WAKENET_MODEL_NAME "wn5_nihaoxiaozhiX2"
#define WAKENET_COEFF get_coeff_nihaoxiaozhi_wn5X2


#elif CONFIG_SR_WN_WN5X3_NIHAOXIAOZHI
#include "nihaoxiaozhi_wn5X3.h"
#define WAKENET_MODEL_NAME "wn5_nihaoxiaozhiX3"
#define WAKENET_COEFF get_coeff_nihaoxiaozhi_wn5X3


#elif CONFIG_SR_WN_WN5X3_NIHAOXIAOXIN
#include "nihaoxiaoxin_wn5X3.h"
#define WAKENET_MODEL_NAME "wn5_nihaoxiaoxinX3"
#define WAKENET_COEFF get_coeff_nihaoxiaoxin_wn5X3


#elif CONFIG_SR_WN_WN5X3_HIJESON
#include "hijeson_wn5X3.h"
#define WAKENET_MODEL_NAME "wn5_hijesonX3"
#define WAKENET_COEFF get_coeff_hijeson_wn5X3

#elif CONFIG_SR_WN_WN5_CUSTOMIZED_WORD
#include "customized_word_wn5.h"
#define WAKENET_MODEL_NAME "wn5_customizedword"
#define WAKENET_COEFF get_coeff_customizedword_wn5

#else
#define WAKENET_MODEL_NAME "NULL"
#define WAKENET_COEFF "COEFF_NULL"
#endif

#else
#define WAKENET_MODEL_NAME "NULL"
#define WAKENET_COEFF "COEFF_NULL"
#endif

/*

static const sr_model_iface_t *model = esp_wn_handle_from_name(model_name);

//Initialize wakeNet model data
static model_iface_data_t *model_data=model->create(model_name, DET_MODE_90);

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
