#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_tool.hpp"
#include "dl_layer_base.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief 
         * 
         * @tparam feature_t 
         */
        template <typename feature_t>
        class Squeeze : public Layer
        {
        private:
            int output_exponent;           /*<! exponent of output >*/
            Tensor<feature_t> *output;     /*<! output ptr of Squeeze >*/
            bool inplace;                  /*<! true: the output will store to input0
                                                 false: the output will store to a separate memory >*/
            int axis;                      /*<! the dim to to be remove. make sure the length of the dim is equal to 1.
                                            if axis == INT32_MAX, all the dims with length==1 will be removed. >*/
            std::vector<int> output_shape; /*<! output shape of AvgPool2D >*/
        public:
            /**
             * @brief Construct a new Squeeze object
             * 
             * @param axis the dim to to be remove. make sure the length of the dim is equal to 1.
             *             if axis == INT32_MAX, all the dims with length==1 will be removed.
             * @param name      name of Squeeze layer
             * @param inplace   true: the output will store to input0
             *                  false: the output will store to a separate memory
             */
            Squeeze(int axis = INT32_MAX, const char *name = "Squeeze", bool inplace = false) : Layer(name),
                                                                                                output(NULL),
                                                                                                inplace(inplace),
                                                                                                axis(axis),
                                                                                                output_shape({})
            {
            }

            /**
             * @brief Destroy the Squeeze object
             * 
             */
            ~Squeeze()
            {
                if ((!this->inplace) && (this->output != NULL))
                {
                    delete this->output;
                }
            }

            /**
            * @brief Update output shape and exponent
            * 
            * @param input as an input
            * @param print_shape  whether to print the output shape.
            */
            void build(Tensor<feature_t> &input, bool print_shape = false)
            {
                this->output_exponent = input.exponent;
                if (!this->inplace)
                {
                    if (this->output == NULL)
                    {
                        this->output = new Tensor<feature_t>;
                    }
                    this->output->set_exponent(this->output_exponent);
                    this->output->set_shape(input.shape);
                    this->output->squeeze(this->axis);
                    this->output->free_element();
                }
                else
                {
                    this->output = &input;
                    this->output->squeeze(this->axis);
                }
                this->output_shape = this->output->shape;

                if (print_shape)
                {
                    std::cout << this->name << " | ";
                    this->output->print_shape();
                }
            }

            /**
             * @brief Get the output
             * 
             * @return Tensor<feature_t>& Squeeze result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief  Call Squeeze operation.
             * 
             * @param input as an input
             * @return Tensor<feature_t>& Squeeze result
             */
            Tensor<feature_t> &call(Tensor<feature_t> &input)
            {
                DL_LOG_LAYER_LATENCY_INIT();

                if (!this->inplace)
                {
                    DL_LOG_LAYER_LATENCY_START();
                    this->output->set_exponent(input.exponent);
                    this->output->set_shape(this->output_shape);
                    this->output->copy_element(input, true);
                    DL_LOG_LAYER_LATENCY_END(this->name, "Squeeze");
                }
                else
                {
                    DL_LOG_LAYER_LATENCY_START();
                    this->output->set_shape(this->output_shape);
                    DL_LOG_LAYER_LATENCY_END(this->name, "Squeeze");
                }
                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
