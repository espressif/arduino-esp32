#ifndef ESP_KERNEL_H
#define ESP_KERNEL_H
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

	void esp_dl_nn_conv2d_s8_wrapper(int8_t* input, int8_t* kernel, int8_t* bias, int input_shape[], int kernel_shape[], int output_shape[], int padding[], int dilation[], int strides[], int input_exponent, int kernel_exponent, int output_exponent, char activation_sym[], int8_t* output);
	void esp_dl_nn_depthwise_conv2d_s8_wrapper(int8_t* input, int8_t* kernel, int8_t* bias, int input_shape[], int kernel_shape[], int output_shape[], int padding[], int dilation[], int strides[], int input_exponent, int kernel_exponent, int output_exponent, char activation_sym[], int8_t* output);

#ifdef __cplusplus
}
#endif

#endif // ESP_KERNEL_H

