// Copyright 2015-2022 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include "esp_err.h"
#include "esp_mn_iface.h"

/*
esp_mn_node_t is a singly linked list which is used to manage speech commands. 
It is easy to add one speech command into linked list and remove one speech command from linked list.
*/


/**
 * @brief Initialze the speech commands singly linked list.
 *
 * @return
 *     - ESP_OK                  Success
 *     - ESP_ERR_NO_MEM          No memory
 *     - ESP_ERR_INVALID_STATE   The Speech Commands link has been initialized
 */
esp_err_t esp_mn_commands_alloc(void);

/**
 * @brief Clear the speech commands linked list and free root node.
 *
 * @return
 *     - ESP_OK                  Success
 *     - ESP_ERR_INVALID_STATE   The Speech Commands link has not been initialized
 */
esp_err_t esp_mn_commands_free(void);

/**
 * @brief Add one speech commands with phoneme string and command ID
 *
 * @param command_id      The command ID
 * @param phoneme_string  The phoneme string of the speech commands
 *
 * @return
 *     - ESP_OK                  Success
 *     - ESP_ERR_INVALID_STATE   Fail
 */
esp_err_t esp_mn_commands_add(int command_id, char *phoneme_string);

/**
 * @brief Modify one speech commands with new phoneme string
 *
 * @param old_phoneme_string  The old phoneme string of the speech commands
 * @param new_phoneme_string  The new phoneme string of the speech commands
 *
 * @return
 *     - ESP_OK                  Success
 *     - ESP_ERR_INVALID_STATE   Fail
 */
esp_err_t esp_mn_commands_modify(char *old_phoneme_string, char *new_phoneme_string);

/**
 * @brief Remove one speech commands by phoneme string
 *
 * @param phoneme_string  The phoneme string of the speech commands
 *
 * @return
 *     - ESP_OK                  Success
 *     - ESP_ERR_INVALID_STATE   Fail
 */
esp_err_t esp_mn_commands_remove(char *phoneme_string);

/**
 * @brief Clear all speech commands in linked list
 *
 * @return
 *     - ESP_OK                  Success
 *     - ESP_ERR_INVALID_STATE   Fail
 */
esp_err_t esp_mn_commands_clear(void);

/**
 * @brief Get phrase from index, which is the depth from the phrase node to root node 
 *
 * @Warning: The first phrase index is 0, the second phrase index is 1, and so on.
 * 
 * @return
 *     - esp_mn_phrase_t*        Success
 *     - NULL                    Fail
 */
esp_mn_phrase_t *esp_mn_commands_get_from_index(int index);

/**
 * @brief Get phrase from phoneme string
 *
 * @return
 *     - esp_mn_phrase_t*        Success
 *     - NULL                    Fail
 */
esp_mn_phrase_t *esp_mn_commands_get_from_string(const char *phoneme_string);

/**
 * @brief Update the speech commands of MultiNet
 * 
 * @Warning: Must be used after [add/remove/modify/clear] function, 
 *           otherwise the language model of multinet can not be updated.
 *
 * @param multinet            The multinet handle
 * @param model_data          The model object to query
 *
 * @return
 *     - NULL                 Success
 *     - others               The list of error phrase which can not be parsed by multinet.
 */
esp_mn_error_t *esp_mn_commands_update(const esp_mn_iface_t *multinet, model_iface_data_t *model_data);

/**
 * @brief Print the MultiNet Speech Commands.
 */
void esp_mn_print_commands(void);

/**
 * @brief Initialze the esp_mn_phrase_t struct by command id and phoneme string .
 *
 * @return the pointer of esp_mn_phrase_t
 */
esp_mn_phrase_t *esp_mn_phrase_alloc(int command_id, char *phoneme_string);

/**
 * @brief Free esp_mn_phrase_t pointer.
 * 
 * @param phrase              The esp_mn_phrase_t pointer
 */
void esp_mn_phrase_free(esp_mn_phrase_t *phrase);

/**
 * @brief Initialze the esp_mn_node_t struct by esp_mn_phrase_t pointer.
 *
 * @return the pointer of esp_mn_node_t
 */
esp_mn_node_t *esp_mn_node_alloc(esp_mn_phrase_t *phrase);

/**
 * @brief Free esp_mn_node_t pointer.
 * 
 * @param node               The esp_mn_node_free pointer
 */
void esp_mn_node_free(esp_mn_node_t *node);

/**
 * @brief Print phrase linked list.
 */
void esp_mn_commands_print(void);