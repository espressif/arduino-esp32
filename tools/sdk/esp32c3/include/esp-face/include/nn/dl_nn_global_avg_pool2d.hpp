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
         * @brief global_avg_pool2d(input).
         * 
         * @param output        as an output
         * @param input         as an input
         * @param assign_core   not effective yet
         */
        void global_avg_pool2d(Tensor<int16_t> &output,
                               Tensor<int16_t> &input,
                               const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief global_avg_pool2d(input).
         * 
         * @param output        as an output
         * @param input         as an input
         * @param assign_core   not effective yet
         */
        void global_avg_pool2d(Tensor<int8_t> &output,
                               Tensor<int8_t> &input,
                               const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief global_avg_pool2d(input).
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         * @param output_exponent exponent of output
         * @param input           as an input
         * @param assign_core     not effective yet
         * @return global_avg_pool2d result
         */
        template <typename feature_t>
        Tensor<feature_t> global_avg_pool2d(const int output_exponent,
                                            Tensor<feature_t> &input,
                                            const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE)
        {
            DL_LOG_NN_LATENCY_INIT();

            DL_LOG_NN_LATENCY_START();
            std::vector<int> output_shape(input.shape.size(), 1);
            output_shape[2] = input.shape[2];
            Tensor<feature_t> output;
            output.set_exponent(output_exponent).set_shape(output_shape).malloc_element();
            DL_LOG_NN_LATENCY_END("apply");

            DL_LOG_NN_LATENCY_START();
            global_avg_pool2d(output, input, assign_core);
            DL_LOG_NN_LATENCY_END("global_avg_pool2d");

            return output;
        }
    } // namespace nn
} // namespace dl