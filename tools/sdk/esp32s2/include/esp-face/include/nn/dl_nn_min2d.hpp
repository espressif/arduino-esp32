#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_nn.hpp"

namespace dl
{
    namespace nn
    {
        /**
         * @brief min2d(input0, input1) 
         * 
         * @param output      as an output
         * @param input0      as one input
         * @param input1      as another input
         * @param assign_core 
         */
        void min2d(Tensor<int16_t> &output,
                   Tensor<int16_t> &input0,
                   Tensor<int16_t> &input1,
                   const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief min2d(input0, input1)  
         * 
         * @param output      as an output
         * @param input0      as one input
         * @param input1      as another input
         * @param assign_core 
         */
        void min2d(Tensor<int8_t> &output,
                   Tensor<int8_t> &input0,
                   Tensor<int8_t> &input1,
                   const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief min2d(input0, input1)
         * 
         * @tparam inplace: whether directly store the output to input0
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         * @param input0          as one input
         * @param input1          as another input
         * @param assign_core     not effective yet
         * @return min2d result or no return(result store to input0)
         */
        template <bool inplace = false, typename feature_t>
        auto min2d(Tensor<feature_t> &input0,
                   Tensor<feature_t> &input1,
                   const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE) -> typename std::conditional<inplace, void, Tensor<feature_t>>::type
        {
            assert(input0.is_same_shape(input1));
            assert(input0.exponent == input1.exponent);

            DL_LOG_NN_LATENCY_INIT();
            Tensor<feature_t> output;

            if constexpr (!inplace)
            {
                DL_LOG_NN_LATENCY_START();
                output.set_exponent(input0.exponent).set_shape(input0.shape).malloc_element();
                DL_LOG_NN_LATENCY_END("apply");

                DL_LOG_NN_LATENCY_START();
                min2d(output, input0, input1, assign_core);
                DL_LOG_NN_LATENCY_END("min2d");

                return output;
            }
            else
            {
                DL_LOG_NN_LATENCY_START();
                min2d(input0, input0, input1, assign_core);
                DL_LOG_NN_LATENCY_END("min2d");
            }
        }
    } // namespace nn
} // namespace dl