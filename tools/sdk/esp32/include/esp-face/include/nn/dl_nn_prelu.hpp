#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_nn.hpp"

namespace dl
{
    namespace nn
    {
        /**
         * @brief prelu(input).
         * 
         * @param output                as an output
         * @param input                 as an input
         * @param activation_element    quantized alpha elements along channel axis
         * @param activation_exponent   exponent of quantized alpha elements
         * @param assign_core not effective yet
         */
        void prelu(Tensor<int16_t> &output,
                   Tensor<int16_t> &input,
                   const int16_t *activation_element,
                   const int activation_exponent,
                   const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief prelu(input).
         * 
         * @param output                as an output
         * @param input                 as an input
         * @param activation_element    quantized alpha elements along channel axis
         * @param activation_exponent   exponent of quantized alpha elements
         * @param assign_core not effective yet
         */
        void prelu(Tensor<int8_t> &output,
                   Tensor<int8_t> &input,
                   const int8_t *activation_element,
                   const int activation_exponent,
                   const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief prelu(input)
         * 
         * @tparam inplace: whether directly store the output to input
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         * @param input                 as an input
         * @param activation_element    quantized alpha elements along channel axis
         * @param activation_exponent   exponent of quantized alpha elements
         * @param assign_core           not effective yet
         * @return prelu result or no return(result store to input)
         */
        template <bool inplace = false, typename feature_t>
        auto prelu(Tensor<feature_t> &input,
                   const feature_t *activation_element,
                   const int activation_exponent,
                   const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE) -> typename std::conditional<inplace, void, Tensor<feature_t>>::type
        {
            DL_LOG_NN_LATENCY_INIT();
            Tensor<feature_t> output;
            if constexpr (!inplace)
            {
                DL_LOG_NN_LATENCY_START();
                output.set_exponent(input.exponent).set_shape(input.shape).malloc_element();
                DL_LOG_NN_LATENCY_END("apply");

                DL_LOG_NN_LATENCY_START();
                prelu(output, input, activation_element, activation_exponent, assign_core);
                DL_LOG_NN_LATENCY_END("prelu");

                return output;
            }
            else
            {
                DL_LOG_NN_LATENCY_START();
                prelu(input, input, activation_element, activation_exponent, assign_core);
                DL_LOG_NN_LATENCY_END("prelu");
            }
        }
    } // namespace nn
} // namespace dl