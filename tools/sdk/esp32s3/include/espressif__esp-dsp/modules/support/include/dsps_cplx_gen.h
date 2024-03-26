/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _dsps_cplx_gen_H_
#define _dsps_cplx_gen_H_

#include "dsp_err.h"
#include "dsps_cplx_gen_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @brief Ennum defining output data type of the complex generator
 *
 */
typedef enum output_data_type {
    S16_FIXED = 0,              /*!< Q15 fixed point - int16_t*/
    F32_FLOAT = 1,              /*!< Single precision floating point - float*/
} out_d_type;


/**
 * @brief Data struct of the complex signal generator
 *
 * This structure is used by a complex generator internally. A user should access this structure only in case of
 * extensions for the DSP Library.
 * All the fields of this structure are initialized by the dsps_cplx_gen_init(...) function.
 */
typedef struct cplx_sig_s {
    void       *lut;            /*!< Pointer to the lookup table.*/
    int32_t     lut_len;        /*!< Length of the lookup table.*/
    float       freq;           /*!< Frequency of the output signal. Nyquist frequency -1 ... 1*/
    float       phase;          /*!< Phase (initial_phase during init)*/
    out_d_type  d_type;         /*!< Output data type*/
    int16_t     free_status;    /*!< Indicator for cplx_gen_free(...) function*/
} cplx_sig_t;


/**
 * @brief Initialize strucure for complex generator
 *
 * Function initializes a structure for either 16-bit fixed point, or 32-bit floating point complex generator using LUT table.
 * cplx_gen_free(...) must be called, once the generator is not needed anymore to free dynamically allocated memory
 *
 * A user can specify his own LUT table and pass a pointer to the table (void *lut) during the initialization. If the LUT table
 * pointer passed to the init function is a NULL, the LUT table is initialized internally.
 *
 * @param cplx_gen: pointer to the floating point generator structure
 * @param d_type: output data type - out_d_type enum
 * @param lut: pointer to a user-defined LUT, the data type is void so both (S16_FIXED, F32_FLOAT) types could be used
 * @param lut_len: length of the LUT
 * @param freq: Frequency of the output signal in a range of [-1...1], where 1 is a Nyquist frequency
 * @param initial_phase: initial phase of the complex signal in range of [-1..1] where 1 is related to 2Pi and -1 is related to -2Pi
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_cplx_gen_init(cplx_sig_t *cplx_gen, out_d_type d_type, void *lut, int32_t lut_len, float freq, float initial_phase);


/**
 * @brief function sets the output frequency of the complex generator
 *
 * set function can be used after the cplx_gen structure was initialized by the dsps_cplx_gen_init(...) function
 *
 * @param cplx_gen: pointer to the complex signal generator structure
 * @param freq: new frequency to be set in a range of [-1..1] where 1 is a Nyquist frequency
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_DSP_INVALID_PARAM if the frequency is out of the Nyquist frequency range
 */
esp_err_t dsps_cplx_gen_freq_set(cplx_sig_t *cplx_gen, float freq);


/**
 * @brief function gets the output frequency of the complex generator
 *
 * get function can be used after the cplx_gen structure was initialized by the dsps_cplx_gen_init(...) function
 *
 * @param cplx_gen: pointer to the complex signal generator structure
 *
 * @return function returns frequency of the signal generator
 */
float dsps_cplx_gen_freq_get(cplx_sig_t *cplx_gen);


/**
 * @brief function sets the phase of the complex generator
 *
 * set function can be used after the cplx_gen structure was initialized by the dsps_cplx_gen_init(...) function
 *
 * @param cplx_gen: pointer to the complex signal generator structure
 * @param phase: new phase to be set in the range of [-1..1] where 1 is related to 2Pi and -1 is related to -2Pi
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_DSP_INVALID_PARAM if the phase is out of -1 ... 1 range
 */
esp_err_t dsps_cplx_gen_phase_set(cplx_sig_t *cplx_gen, float phase);


/**
 * @brief function gets the phase of the complex generator
 *
 * get function can be used after the cplx_gen structure was initialized by the dsps_cplx_gen_init(...) function
 *
 * @param cplx_gen: pointer to the complex signal generator structure
 *
 * @return function returns phase of the signal generator
 */
float dsps_cplx_gen_phase_get(cplx_sig_t *cplx_gen);


/**
 * @brief function sets the output frequency and the phase of the complex generator
 *
 * set function can be used after the cplx_gen structure was initialized by the dsps_cplx_gen_init(...) function
 *
 * @param cplx_gen: pointer to the complex signal generator structure
 * @param freq: new frequency to be set in the range of [-1..1] where 1 is a Nyquist frequency
 * @param phase: new phase to be set in the range of [-1..1] where 1 is related to 2Pi and -1 is related to -2Pi
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_DSP_INVALID_PARAM if the frequency is out of the Nyquist frequency range
 *                                  if the phase is out of -1 ... 1 range
 */
esp_err_t dsps_cplx_gen_set(cplx_sig_t *cplx_gen, float freq, float phase);


/**
 * @brief function frees dynamically allocated memory, which was allocated in the init function
 *
 * free function must be called after the dsps_cplx_gen_init(...) is called, once the complex generator is not
 * needed anymore
 *
 * @param cplx_gen: pointer to the complex signal generator structure
 */
void cplx_gen_free(cplx_sig_t *cplx_gen);


/**
 * @brief The function generates a complex signal
 *
 * the generated complex signal is in the form of two harmonics signals in either 16-bit signed fixed point
 * or 32-bit floating point
 *
 * x[i]=   A*sin(step*i + ph/180*Pi)
 * x[i+1]= B*cos(step*i + ph/180*Pi)
 * where step = 2*Pi*frequency
 *
 * dsps_cplx_gen_ansi() - The implementation uses ANSI C and could be compiled and run on any platform
 * dsps_cplx_gen_ae32() - Is targetted for Xtensa cores
 *
 * @param cplx_gen: pointer to the generator structure
 * @param output: output array (length of len*2), data type is void so both (S16_FIXED, F32_FLOAT) types could be used
 * @param len: length of the output signal
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_cplx_gen_ansi(cplx_sig_t *cplx_gen, void *output, int32_t len);
esp_err_t dsps_cplx_gen_ae32(cplx_sig_t *cplx_gen, void *output, int32_t len);


#ifdef __cplusplus
}
#endif


#if CONFIG_DSP_OPTIMIZED
#define dsps_cplx_gen dsps_cplx_gen_ae32
#else // CONFIG_DSP_OPTIMIZED
#define dsps_cplx_gen dsps_cplx_gen_ansi
#endif // CONFIG_DSP_OPTIMIZED

#endif // _dsps_cplx_gen_H_
