#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_nn.hpp"

namespace dl
{
    namespace nn
    {
        /**
         * @brief relu(input).
         * 
         * @param output      as an output
         * @param input       as an input
         * @param assign_core not effective yet
         */
        void relu(Tensor<int16_t> &output,
                  Tensor<int16_t> &input,
                  const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief relu(input).
         * 
         * @param output      as an output
         * @param input       as an input
         * @param assign_core not effective yet
         */
        void relu(Tensor<int8_t> &output,
                  Tensor<int8_t> &input,
                  const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief relu(input)
         * 
         * @tparam inplace: whether directly store the output to input
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         * @param input           as an input
         * @param assign_core     not effective yet
         * @return relu result or no return(result store to input)
         */
        template <bool inplace = false, typename feature_t>
        auto relu(Tensor<feature_t> &input, const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE) -> typename std::conditional<inplace, void, Tensor<feature_t>>::type
        {
            DL_LOG_NN_LATENCY_INIT();
            Tensor<feature_t> output;

            if constexpr (!inplace)
            {
                DL_LOG_NN_LATENCY_START();
                output.set_exponent(input.exponent).set_shape(input.shape).malloc_element();
                DL_LOG_NN_LATENCY_END("apply");

                DL_LOG_NN_LATENCY_START();
                relu(output, input, assign_core);
                DL_LOG_NN_LATENCY_END("relu");

                return output;
            }
            else
            {
                DL_LOG_NN_LATENCY_START();
                relu(input, input, assign_core);
                DL_LOG_NN_LATENCY_END("relu");
            }
        }
    } // namespace nn
} // namespace dl