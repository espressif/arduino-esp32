#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_nn.hpp"
#include <stdint.h>

namespace dl
{
    namespace nn
    {
        /**
         * @brief avg_pool2d(input).
         * 
         * @param output        as an output
         * @param input         as an input
         * @param padding       padding size needed in [top, bottom, left, right] of this operation
         * @param filter_shape  filter_shape in [filter_height, filter_width]
         * @param stride_y      stride in height
         * @param stride_x      stride in width
         * @param assign_core   not effective yet
         */
        void avg_pool2d(Tensor<int16_t> &output,
                        Tensor<int16_t> &input,
                        std::vector<int> &padding,
                        std::vector<int> &filter_shape,
                        const int stride_y,
                        const int stride_x,
                        const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief avg_pool2d(input).
         * 
         * @param output        as an output
         * @param input         as an input
         * @param padding       padding size needed in [top, bottom, left, right] of this operation
         * @param filter_shape  filter_shape in [filter_height, filter_width]
         * @param stride_y      stride in height
         * @param stride_x      stride in width
         * @param assign_core   not effective yet
         */
        void avg_pool2d(Tensor<int8_t> &output,
                        Tensor<int8_t> &input,
                        std::vector<int> &padding,
                        std::vector<int> &filter_shape,
                        const int stride_y,
                        const int stride_x,
                        const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief avg_pool2d(input).
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         * @param output_exponent exponent of output
         * @param input           as an input
         * @param filter_shape    filter_shape in [filter_height, filter_width]
         * @param stride_y        stride in height
         * @param stride_x        stride in width
         * @param padding_type    one of PADDING_VALID or PADDING_SAME_END or PADDING_SAME_BEGIN,
         *                        - PADDING_VALID: no padding
         *                        PADDING_SAME_END and PADDING_SAME_BEGIN results in padding with zeros evenly to the left/right or up/down of the input 
         *                        such that output has the same height/width dimension as the input,
         *                        - PADDING_SAME_END results padding in TensorFlow style
         *                        - PADDING_SAME_BEGIN results padding in MXNET style
         * @param assign_core     not effective yet
         * @return avg_pool2d result
         */
        template <typename feature_t>
        Tensor<feature_t> avg_pool2d(const int output_exponent,
                                     Tensor<feature_t> &input,
                                     std::vector<int> filter_shape,
                                     const int stride_y,
                                     const int stride_x,
                                     const padding_type_t padding_type,
                                     const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE)
        {
            DL_LOG_NN_LATENCY_INIT();

            DL_LOG_NN_LATENCY_START();
            std::vector<int> output_shape = get_output_shape(input.shape, filter_shape, stride_y, stride_x, padding_type);
            Tensor<feature_t> output;
            output.set_exponent(output_exponent).set_shape(output_shape).malloc_element();
            DL_LOG_NN_LATENCY_END("apply");
            std::vector<int> padding(4, 0);

            DL_LOG_NN_LATENCY_START();
            if (padding_type == PADDING_SAME_END || padding_type == PADDING_SAME_BEGIN)
            {
                padding = get_pad_size(output_shape, input.shape, filter_shape, stride_y, stride_x, padding_type);
            }
            DL_LOG_NN_LATENCY_END("padding");

            DL_LOG_NN_LATENCY_START();
            avg_pool2d(output, input, padding, filter_shape, stride_y, stride_x, assign_core);
            DL_LOG_NN_LATENCY_END("avg_pool2d");

            return output;
        }
    } // namespace nn
} // namespace dl