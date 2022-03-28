// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
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
#ifndef DL_LIB_COEFGETTER_IF_H
#define DL_LIB_COEFGETTER_IF_H

#include "dl_lib_matrix.h"
#include "dl_lib_matrixq.h"
#include "dl_lib_matrixq8.h"
#include "cJSON.h"

//Set this if the coefficient requested is a batch-normalization popvar matrix which needs to be preprocessed by
//dl_batch_normalize_get_sqrtvar first.
#define COEF_GETTER_HINT_BNVAR (1<<0)

/*
This struct describes the basic information of model data: 
word_num: the number of wake words or speech commands
word_list: the name list of wake words or speech commands
thres_list: the threshold list of wake words or speech commands
info_str: the string used to reflect the version and information of model data
          which consist of the architecture of network, the version of model data, wake words and their threshold
*/
typedef struct {
    int word_num;
    char **word_list;
    int *win_list;
    float *thresh_list;
    char *info_str;
} model_info_t;

/*
Alphabet struct describes the basic grapheme or phoneme.
item_num: the number of baisc item(grapheme or phonemr)
items: the list of basic item
*/
typedef struct {
    int item_num;
    char **items;
}alphabet_t;

/*
This struct describes a generic coefficient getter: a way to get the constant coefficients needed for a neural network.
For the two getters, the name describes the name of the coefficient matrix, usually the same as the Numpy filename the
coefficient was originally stored in. The arg argument can be used to optionally pass an additional user-defined argument
to the getter (e.g. the directory to look for files in the case of the Numpy file loader getter). The hint argument
is a bitwise OR of the COEF_GETTER_HINT_* flags or 0 when none is needed. Use the free_f/free_q functions to release the
memory for the returned matrices, when applicable.
*/
typedef struct {
    const dl_matrix2d_t* (*getter_f)(const char *name, void *arg, int hint);
    const dl_matrix2dq_t* (*getter_q)(const char *name, void *arg, int hint);
    const dl_matrix2dq8_t* (*getter_q8)(const char *name, void *arg, int hint);
    void (*free_f)(const dl_matrix2d_t *m);
    void (*free_q)(const dl_matrix2dq_t *m);
    void (*free_q8)(const dl_matrix2dq8_t *m);
    const model_info_t* (*getter_info)(void *arg);
    const alphabet_t* (*getter_alphabet)(void *arg);
    const cJSON* (*getter_config)(void *arg);
} model_coeff_getter_t;

#endif