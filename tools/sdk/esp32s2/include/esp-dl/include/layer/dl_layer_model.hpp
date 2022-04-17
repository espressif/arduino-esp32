#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief Neural Network Model.
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         */
        template <typename feature_t>
        class Model
        {
        private:
            std::vector<int> input_shape; /*<! input shape in [height, width, channel] >*/

        public:
            /**
             * @brief Destroy the Model object.
             * 
             */
            virtual ~Model() {}

            /**
             * @brief Build a model including update output shape and input padding of each layer.
             * 
             * @param input as an input
             */
            virtual void build(Tensor<feature_t> &input) = 0;

            /**
             * @brief Call the model layer by layer.
             * 
             * @param input as an input.
             */
            virtual void call(Tensor<feature_t> &input) = 0;

            /**
             * @brief If input.shape changes, call Model.build(), otherwise, do not. Then call Model.call().
             * 
             * @param input as an input
             */
            void forward(Tensor<feature_t> &input);
        };
    } // namespace layer
} // namespace dl
