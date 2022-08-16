#pragma once

typedef struct 
{
    char **model_name;     // the name of models, like "wn9_hilexin"(wakenet9, hilexin), "mn5_en"(multinet5, english)
    char *partition_label;  // partition label used to save the files of model
    int num;               // the number of models
} srmodel_list_t;

#define MODEL_NAME_MAX_LENGTH 64

/**
 * @brief Return all avaliable models in spiffs or selected in Kconfig.
 *
 * @param partition_label    The spiffs label defined in your partition file used to save models.
 * 
 * @return all avaliable models in spiffs,save as srmodel_list_t.
 */
srmodel_list_t* esp_srmodel_init(const char* partition_label);

/**
 * @brief Free srmodel_list_t and unregister SPIFFS filesystem if open SPIFFS filesystem.
 * 
 * @param models    The srmodel_list_t point allocated by esp_srmodel_init function.
 *
 * @return all avaliable models in spiffs,save as srmodel_list_t.
 */
void esp_srmodel_deinit(srmodel_list_t *models);

/**
 * @brief Return the first model name containing the specified keywords
 *        If keyword is NULL, we will ignore the keyword. 
 * 
 * @param models      The srmodel_list_t point allocated by esp_srmodel_init function.
 * @param keyword1    The specified keyword1 , like ESP_WN_PREDIX(the prefix of wakenet), 
 *                                                  ESP_MN_PREFIX(the prefix of multinet),
 *                                                
 * @param keyword2    The specified keyword2, like ESP_MN_ENGLISH(the english multinet)
 *                                                 ESP_MN_CHINESE(the chinese multinet)
 *                                                "alexa" (the "alexa" wakenet)
 * @return return model name if can find one model name containing the keywords otherwise return NULL.
 */
char *esp_srmodel_filter(srmodel_list_t *models, const char *keyword1, const char *keyword2);


/**
 * @brief Check whether the specified model name exists or not.
 * 
 * @param models      The srmodel_list_t point allocated by esp_srmodel_init function.
 * @param model_name  The specified model name
 * @return return index in models if model name exists otherwise return -1
 */
int esp_srmodel_exists(srmodel_list_t *models, char *model_name);

/**
 * @brief Initialize and mount SPIFFS filesystem, return all avaliable models in spiffs.
 *
 * @param partition_label    The spiffs label defined in your partition file used to save models.
 * 
 * @return all avaliable models in spiffs,save as srmodel_list_t.
 */
srmodel_list_t *srmodel_spiffs_init(const char* partition_label);

/**
 * @brief unregister SPIFFS filesystem and free srmodel_list_t.
 * 
 * @param models    The srmodel_list_t point allocated by srmodel_spiffs_init function.
 *
 * @return all avaliable models in spiffs,save as srmodel_list_t.
 */
void srmodel_spiffs_deinit(srmodel_list_t *models);


/**
 * @brief Return base path of srmodel spiffs
 *
 * @return the base path od srmodel spiffs
 */
char *get_model_base_path(void);


#ifdef ESP_PLATFORM
#include "dl_lib_coefgetter_if.h"
/**
 * @brief Return model_coeff_getter_t pointer base on model_name
 *
 * @warning Just support ESP32 to load old wakenet
 * 
 * @param model_name    The model name
 *
 * @return model_coeff_getter_t pointer or NULL
 */
model_coeff_getter_t* srmodel_get_model_coeff(char *model_name);
#endif