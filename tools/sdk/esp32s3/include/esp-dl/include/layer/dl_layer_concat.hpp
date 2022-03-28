#pragma once

#include <assert.h>
#include <vector>

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_tool.hpp"
#include "dl_layer_base.hpp"
#include "dl_nn_concat.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief Concat(input1, input2, input3, ...).
         * 
         * @tparam feature_t support all kinds of integer and float data type
         */
        template <typename feature_t>
        class Concat : Layer
        {
        private:
            int output_exponent;           /*<! exponent of output >*/
            int axis;                      /*<! The axis along which the Tensor will be concatenated. >*/
            Tensor<feature_t> *output;     /*<! output ptr of Concat >*/
            std::vector<int> output_shape; /*<! output shape of Concat >*/
        public:
            /**
             * @brief Construct a new Concat object.
             * 
             * @param name name of layer
             * @param axis The axis along which the Tensor will be concatenated.
             */
            Concat(int axis, const char *name = "Concat") : Layer(name), axis(axis), output_shape({})
            {
                this->output = new Tensor<feature_t>;
            }

            /**
             * @brief Destroy the Concat object
             */
            ~Concat()
            {
                if (this->output != NULL)
                {
                    delete this->output;
                }
            }

            /**
             * @brief Collect inputs' channel and memory offset, called in Model.build().
             * 
             * @param args pointers of concatenated Tensor
             * @param print_shape  whether to print the output shape.
             */
            void build(std::vector<Tensor<feature_t> *> args, bool print_shape = false)
            {
                assert(args.size() > 1);
                int shape_size = args[0]->shape.size();

                if (this->axis < 0)
                {
                    this->axis = shape_size + this->axis;
                }
                assert((this->axis < shape_size) && (this->axis > -1));

                int output_shape_axis = args[0]->shape[this->axis];

                for (int i = 1; i < args.size(); i++)
                {
                    assert(shape_size == args[i]->shape.size());
                    assert(args[i]->exponent == args[i - 1]->exponent);
                    output_shape_axis += args[i]->shape[this->axis];

                    for (int j = 0; j < shape_size; j++)
                    {
                        if (j != this->axis)
                        {
                            assert(args[i]->shape[j] == args[i - 1]->shape[j]);
                        }
                    }
                }

                this->output_exponent = args[0]->exponent;
                this->output_shape = args[0]->shape;
                this->output_shape[this->axis] = output_shape_axis;

                this->output->set_shape(this->output_shape);
                this->output->set_exponent(this->output_exponent);
                this->output->free_element();

                if (print_shape)
                {
                    std::cout << this->name << " | ";
                    this->output->print_shape();
                }
            }

            /**
             * @brief Call Concat operation
             * 
             * @param inputs                the pointers of inputs 
             * @param free_inputs           true: free the inputs after call
             *                              false: do not free inputs
             * @return Tensor<feature_t>&   concat result
             */
            Tensor<feature_t> &call(std::vector<Tensor<feature_t> *> inputs, bool free_inputs = false)
            {
                DL_LOG_LAYER_LATENCY_INIT();

                DL_LOG_LAYER_LATENCY_START();
                if (this->output->shape != this->output_shape)
                {
                    this->output->set_shape(this->output_shape);
                }
                this->output->malloc_element();
                this->output->set_exponent(this->output_exponent);
                DL_LOG_LAYER_LATENCY_END(this->name, "apply");

                DL_LOG_LAYER_LATENCY_START();
                nn::concat(*this->output, inputs, this->axis, free_inputs);
                DL_LOG_LAYER_LATENCY_END(this->name, "concat");
                return *this->output;
            }

            /**
             * @brief Get the output
             * 
             * @return Tensor<feature_t>&  Concat result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl