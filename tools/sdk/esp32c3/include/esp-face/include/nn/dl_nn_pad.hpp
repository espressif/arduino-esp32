#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_nn.hpp"

namespace dl
{
    namespace nn
    {
        /**
         * @brief pad(input)
         * 
         * @tparam feature_t 
         * @param output            as an output
         * @param input             as an input
         * @param paddings          number of values padded to the edges of each dim
         * @param constant_values   used in PADDING_CONSTANT, the values to set the padded values for each dim
         * @param mode              One of the following: PADDING_EMPTY, PADDING_CONSTANT, PADDING_EDGE, PADDING_REFLECT, PADDING_SYMMETRIC
         * @param assign_core       not effective yet
         */
        template <typename feature_t>
        void pad(Tensor<feature_t> &output,
                 Tensor<feature_t> &input,
                 std::vector<int> paddings,
                 std::vector<feature_t> constant_values,
                 padding_mode_t mode,
                 const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        
        /**
         * @brief 
         * 
         * @tparam feature_t 
         * @param input                 as an input
         * @param paddings              number of values padded to the edges of each dim
         * @param constant_values       used in PADDING_CONSTANT, the values to set the padded values for each dim
         * @param mode                  One of the following: PADDING_EMPTY, PADDING_CONSTANT, PADDING_EDGE, PADDING_REFLECT, PADDING_SYMMETRIC
         * @param assign_core           not effective yet
         * @return Tensor<feature_t>    the padded Tensor
         */
        template <typename feature_t>
        Tensor<feature_t> pad(Tensor<feature_t> &input,
                              std::vector<int> paddings,
                              std::vector<feature_t> constant_values,
                              padding_mode_t mode,
                              const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE)
        {
            DL_LOG_NN_LATENCY_INIT();

            DL_LOG_NN_LATENCY_START();

            assert(paddings.size() > 0);
            int input_dims = input.shape.size();
            int padding_dims = input_dims * 2;
            std::vector<int> _paddings(padding_dims, 0);
            if (paddings.size() == 1)
            {
                for (int i = 0; i < padding_dims; ++i)
                {
                    _paddings[i] = paddings[0];
                }
            }
            else if (paddings.size() == 2)
            {
                for (int i = 0; i < input_dims; ++i)
                {
                    _paddings[2 * i] = paddings[0];
                    _paddings[2 * i + 1] = paddings[1];
                }
            }
            else
            {
                assert(paddings.size() == padding_dims);
                _paddings = paddings;
            }

            std::vector<feature_t> _constant_values(padding_dims, 0);
            if (mode == PADDING_CONSTANT)
            {
                if (constant_values.size() == 1)
                {
                    for (int i = 0; i < padding_dims; ++i)
                    {
                        _constant_values[i] = constant_values[0];
                    }
                }
                else if (constant_values.size() == 2)
                {
                    for (int i = 0; i < input_dims; ++i)
                    {
                        _constant_values[2 * i] = constant_values[0];
                        _constant_values[2 * i + 1] = constant_values[1];
                    }
                }
                else
                {
                    assert(constant_values.size() == padding_dims);
                    _constant_values = constant_values;
                }
            }

            std::vector<int> output_shape = input.shape;
            for (int i = 0; i < input_dims; ++i)
            {
                output_shape[i] += (_paddings[2 * i] + _paddings[2 * i + 1]);
            }

            Tensor<feature_t> output;
            output.set_exponent(input.exponent).set_shape(output_shape).malloc_element();
            DL_LOG_NN_LATENCY_END("apply");

            DL_LOG_NN_LATENCY_START();
            pad(output, input, _paddings, _constant_values, mode, assign_core);
            DL_LOG_NN_LATENCY_END("pad");

            return output;
        }
    } // namespace nn
} // namespace dl