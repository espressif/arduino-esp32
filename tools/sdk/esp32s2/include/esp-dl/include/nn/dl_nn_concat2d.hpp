#pragma once

#include <vector>
#include "dl_variable.hpp"

namespace dl
{
    namespace nn
    {
        /**
         * @brief concat2d(input_1, input_2, ...)
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         * @param output as an output
         * @param inputs a bundle of inputs to be concatenated
         */
        template <typename feature_t>
        void concat2d(Tensor<feature_t> &output, std::vector<Tensor<feature_t>> inputs);
    } // namespace nn
} // namespace dl