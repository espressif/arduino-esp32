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
        class ExpandDims : public Layer
        {
        private:
            std::vector<int> output_shape; /*<! output shape of ExpandDims >*/
            std::vector<int> axis;         /*<! position where the new axis is placed. >*/
            Tensor<feature_t> *output;     /*<! output ptr of ExpandDims >*/
            bool inplace;                  /*<! true: the output will store to input0
                                                false: the output will store to a separate memory >*/

        public:
            int output_exponent;

            /**
             * @brief Construct a new ExpandDims object
             * 
             * @param axis position where the new axis is placed.
             * @param name name of layer
             * @param inplace true: the output will store to input
             *                false: the output will store to a separate memory
             */
            ExpandDims(std::vector<int> axis, const char *name = "ExpandDims", bool inplace = false) : Layer(name),
                                                                                                       output_shape({}),
                                                                                                       axis(axis), 
                                                                                                       output(NULL),
                                                                                                       inplace(inplace) 
            {
            }

            /**
             * @brief Destroy the ExpandDims object
             * 
             */
            ~ExpandDims()
            {
                if ((!this->inplace) && (this->output != NULL))
                {
                    delete this->output;
                }
            }

            /**
            * @brief  Update output shape.
            * 
            * @param input as an input.
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
                    this->output->expand_dims(this->axis);
                    this->output->free_element();
                }
                else
                {
                    this->output = &input;
                    this->output->expand_dims(this->axis);
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
             * @return Tensor<feature_t>& ExpandDims result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief call ExpandDims opeartion
             * 
             * @param input 
             * @return Tensor<feature_t>& ExpandDims result
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
                    DL_LOG_LAYER_LATENCY_END(this->name, "ExpandDims");
                }
                else
                {
                    DL_LOG_LAYER_LATENCY_START();
                    this->output->set_shape(this->output_shape);
                    DL_LOG_LAYER_LATENCY_END(this->name, "ExpandDims");
                }
                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
