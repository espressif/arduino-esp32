#pragma once

#include <assert.h>
#include <vector>

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_tool.hpp"
#include "dl_layer_base.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief Concat2D(input1, input2, input3, ...).
         * 
         * @tparam feature_t support all kinds of integer and float data type
         */
        template <typename feature_t>
        class Concat2D : Layer
        {
        private:
            std::vector<Tensor<feature_t> *> output_vec; /*<! pointers of concatenated inputs >*/
            std::vector<int> offset;                     /*<! memory offset of each concatenated inputs in entire element >*/
            std::vector<int> channel;                    /*<! channel of concatenated inputs >*/
            Tensor<feature_t> *output;                  /*<! output ptr of Concat2D >*/
            int output_exponent;                        /*<! exponent of output >*/
        public: 

            /**
             * @brief Construct a new Concat2D object.
             * 
             * @param name name of layer
             */
            Concat2D(const char *name = NULL) : Layer(name) {
                this->output = new Tensor<feature_t>;
            }

            /**
             * @brief Destroy the Concat2D object
             */
            ~Concat2D() 
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
             */
            void build(std::vector<Tensor<feature_t> *> args)
            {
                assert(args.size() > 0);

                this->output_vec = args;

                this->offset = std::vector<int>(args.size());
                this->channel = std::vector<int>(args.size());

                this->output_exponent = args[0]->exponent;
                this->offset[0] = 0;
                this->channel[0] = args[0]->shape[2];
                std::vector<int> output_shape = args[0]->shape;

                for (int i = 1; i < args.size(); i++)
                {
                    assert(output_shape[0] == args[i]->shape[0]); // height
                    assert(output_shape[1] == args[i]->shape[1]); // width
                    // assert(this->output_exponent == args[i]->exponent); // exponent

                    this->offset[i] = output_shape[2];
                    this->channel[i] = args[i]->shape[2];
                    output_shape[2] += args[i]->shape[2];
                }
                this->output->set_shape(output_shape);
                this->output->set_exponent(this->output_exponent);
                this->output->free_element();
            }

            /**
             * @brief Get the output
             * 
             * @return Tensor<feature_t>&  Concat2d result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Get the maximum padding among inputs and output-> Then, set to this->output. Called at the end of Model.build().
             * NOTE: Some special situations like C = Concat2D_1(A, B), E = Concat2D_2(C, D), where A, B, C, D, E are Tensor.
             *         For avoiding memory copy, we apply an entire element for E, and take it apart for A, B, D. 
             *         A, B, C, D and E will become other layer's inputs so that result different size of padding.
             *         For get the maximum padding, we should call at the end of Model.build(),
             *              Concat2D_1.backward();  // max_padding_temp = get_max_padding(A, B, C), padding of A, B and C are set to max_padding_temp.
             *              Concat2D_2.backward();  // max_padding = get_max_padding(max_padding_temp, get_max_padding(D, E)) , padding of C, D and E are set to max_padding.
             *                                         However, padding of A and B is still max_padding_temp.
             *              Concat2D_1.backward();  // padding of A and B are set to max_padding.
             *         Or,
             *              Concat2D_2.backward();
             *              Concat2D_1.backward();
             *              Concat2D_2.backward();
             */
            void backward()
            {
                std::vector<int> max_padding = this->output->padding;
                int max_channel_with_padding = this->output->shape_with_padding[2];
                for (int i = 0; i < this->output_vec.size(); i++)
                {
                    for (int j = 0; j < max_padding.size(); j++)
                    {
                        max_padding[j] = DL_MAX(max_padding[j], this->output_vec[i]->padding[j]);
                    }
                    max_channel_with_padding = DL_MAX(max_channel_with_padding, this->output_vec[i]->shape_with_padding[2]);
                }

                this->output->set_padding_size(max_padding);
                this->output->shape_with_padding[2] = max_channel_with_padding;
                for (int i = 0; i < this->output_vec.size(); i++)
                {
                    this->output_vec[i]->set_padding_size(max_padding);
                    this->output_vec[i]->shape_with_padding[2] = max_channel_with_padding;
#if CONFIG_DEBUG_MODE
                    assert(this->output->shape_with_padding[0] == this->output_vec[i]->shape_with_padding[0]);
                    assert(this->output->shape_with_padding[1] == this->output_vec[i]->shape_with_padding[1]);
                    assert(this->output->shape_with_padding[2] == this->output_vec[i]->shape_with_padding[2]);
#endif
                }
            }

            /**
             * @brief Calloc an entire element for concatnate result. Take the entire element apart and deliver element pointers to concatenated layer.
             * NOTE: For example, C = Concat2D(A, B). We apply an entire element for C and deliver two element pointers to A and B.
             *       Let's assume that A result is produced first. We should call Concat2D.calloc_element() just before A result is produced
             *       to make sure the element of A is ready and could be filled.
             */
            void calloc_element()
            {
                DL_LOG_LAYER_LATENCY_INIT();

                DL_LOG_LAYER_LATENCY_START();
                this->output->calloc_element();
                DL_LOG_LAYER_LATENCY_END(this->name, "apply");

                DL_LOG_LAYER_LATENCY_START();
                for (int i = 0; i < this->offset.size(); i++)
                {
                    this->output_vec[i]->element = this->output->element + this->offset[i];
                    this->output_vec[i]->set_auto_free(false);
                }
                DL_LOG_LAYER_LATENCY_END(this->name, "deliver");
            }

            void apply_element()
            {
                DL_LOG_LAYER_LATENCY_INIT();

                DL_LOG_LAYER_LATENCY_START();
                this->output->apply_element();
                this->output->set_exponent(this->output_exponent);
                DL_LOG_LAYER_LATENCY_END(this->name, "apply");

                DL_LOG_LAYER_LATENCY_START();
                for (int i = 0; i < this->offset.size(); i++)
                {
                    this->output_vec[i]->element = this->output->element + this->offset[i];
                    this->output_vec[i]->set_auto_free(false);
                }
                DL_LOG_LAYER_LATENCY_END(this->name, "deliver");
            }
        };
    } // namespace layer
} // namespace dl