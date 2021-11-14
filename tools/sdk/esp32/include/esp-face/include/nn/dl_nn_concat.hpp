#pragma once

#include <vector>
#include "dl_variable.hpp"
#include "dl_nn.hpp"

namespace dl
{
    namespace nn
    {
        template <typename feature_t>
        void concat(Tensor<feature_t> &output, std::vector<Tensor<feature_t> *> &inputs, int axis, bool free_inputs = false);

        template <typename feature_t>
        Tensor<feature_t> concat(std::vector<Tensor<feature_t> *> &inputs, int axis, bool free_inputs = false)
        {
            DL_LOG_NN_LATENCY_INIT();

            DL_LOG_NN_LATENCY_START();
            assert(inputs.size() > 1);
            int shape_size = inputs[0]->shape.size();

            if (axis < 0)
            {
                axis = shape_size + axis;
            }

            assert((axis < shape_size) && (axis > -1));

            int output_shape_axis = inputs[0]->shape[axis];

            for (int i = 1; i < inputs.size(); i++)
            {
                assert(shape_size == inputs[i]->shape.size());
                assert(inputs[i]->exponent == inputs[i - 1]->exponent);
                output_shape_axis += inputs[i]->shape[axis];

                for (int j = 0; j < shape_size; j++)
                {
                    if (j != axis)
                    {
                        assert(inputs[i]->shape[j] == inputs[i - 1]->shape[j]);
                    }
                }
            }
            DL_LOG_NN_LATENCY_END("assert");

            DL_LOG_NN_LATENCY_START();
            Tensor<feature_t> output;
            std::vector<int> output_shape = inputs[0]->shape;
            output_shape[axis] = output_shape_axis;
            output.set_shape(output_shape);
            output.set_exponent(inputs[0]->exponent);
            output.malloc_element();
            DL_LOG_NN_LATENCY_END("malloc");

            DL_LOG_NN_LATENCY_START();
            concat(output, inputs, axis, free_inputs);
            DL_LOG_NN_LATENCY_END("concat");
            return output;
        }
    } // namespace nn
} // namespace dl
