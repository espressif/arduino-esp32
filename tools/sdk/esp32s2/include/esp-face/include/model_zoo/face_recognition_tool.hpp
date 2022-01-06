#pragma once

#include "dl_variable.hpp"
#include "dl_define.hpp"
#include "dl_tool.hpp"
#include "dl_math.hpp"
#include "dl_math_matrix.hpp"
#include <vector>
#include <list>
#include <algorithm>
#include <math.h>
#include <string>
#include "esp_partition.h"

/**
 * @brief struct of face similarity
 * 
 */
typedef struct
{
    int id;
    std::string name;
    float similarity;
} face_info_t;


/**
 * @brief Face ID
 * 
 * @tparam feature_t 
 */
template <typename feature_t>
class FaceID
{
public:
    int id;     /*<! id index >*/
    dl::Tensor<feature_t> id_emb;   /*<! id embedding >*/
    std::string name;                /*<! id name >*/

    /**
     * @brief Construct a new Face ID object
     * 
     * @param id        id index
     * @param id_emb    id embedding
     * @param name      id name
     */
    FaceID(int id, dl::Tensor<feature_t> &id_emb, std::string name = "");

    /**
     * @brief Construct a new Face ID which is same as input face_id
     * 
     * @param face_id input face_id
     */
    FaceID(FaceID<feature_t> &face_id);

    /**
     * @brief Destroy the Face ID object
     * 
     */
    ~FaceID() {}

    /**
     * @brief print the face id information
     * 
     */
    void print();
};

namespace face_recognition_tool
{
    /**
     * @brief l2 normalize the feautre
     * 
     * @param feature 
     */
    void l2_norm(dl::Tensor<float> &feature);

    /**
     * @brief calculate the cosine distance of the input ids
     * 
     * @param id_1  id 1
     * @param id_2  id 2
     * @param normalized_ids true: the input ids have been normalized.
     *                       false: the input ids have not been normlized 
     * @param type           0: cos dist: [-1, 1]
     *                       1: normalzied cos dist: [0, 1]
     * @return float  the cosine distance
     */
    float cos_distance(dl::Tensor<float> &id_1, dl::Tensor<float> &id_2, bool normalized_ids = true, int8_t type = 0);

    /**
     * @brief transform the image to the input of a mfn model 
     * 
     * @tparam T 
     * @param image             the input image.
     * @param free_input        true: free the input image.
     *                          false: do not free the input image.
     * @param do_padding        true: pad the result.
     *                          false: do not pad the result. 
     * @return dl::Tensor<T>* 
     */
    template <typename T>
    dl::Tensor<T> *transform_mfn_input(dl::Tensor<uint8_t> &image, bool free_input = false);

    /**
     * @brief  transform the image to the input of a mfn model 
     * 
     * @tparam T 
     * @param image                the input image.
     * @param output               the preprocessed image.
     * @param free_input           true: free the input image.
     *                             false: do not free the input image.
     * @param do_padding           true: pad the result.
     *                             false: do not pad the result
     */
    template <typename T>
    void transform_mfn_input(dl::Tensor<uint8_t> &image, dl::Tensor<T> &output, bool free_input = false);

    /**
     * @brief transform the mfn output embedding to a floating embedding
     * 
     * @tparam T 
     * @param input the input embedding. 
     * @param norm   true: normalize the output embedding.
     *               false: do not normalize the output embedding.
     * @param free_input true: free the input embedding.
     *                   false: do not free the input embedding.
     * @return dl::Tensor<float>* 
     */
    template <typename T>
    dl::Tensor<float> *transform_mfn_output(dl::Tensor<T> &input, bool norm = true, bool free_input = false);

    /**
     * @brief transform the mfn output embedding to a floating embedding
     * 
     * @tparam T 
     * @param input         the input embedding. 
     * @param output        the output embedding.
     * @param norm          true: normalize the output embedding.
     *                      false: do not normalize the output embedding.
     * @param free_input    true: free the input embedding.
     *                      false: do not free the input embedding.
     */
    template <typename T>
    void transform_mfn_output(dl::Tensor<T> &input, dl::Tensor<float> &output, bool norm = true, bool free_input = false);

    /**
     * @brief get the aligned face.
     * 
     * @tparam T 
     * @param input     input tensor 
     * @param output    the output aligned face.
     * @param landmarks the landmarks of the face.
     */
    template <typename T>
    void align_face(dl::Tensor<T> *input, dl::Tensor<T> *output, std::vector<int> &landmarks);

    /**
     * @brief get the aligned face.
     * 
     * @tparam T 
     * @param input     input image with rgb565 format.
     * @param shape     the shape of the input image.
     * @param output    the output aligned face.
     * @param landmarks the landmarks of the face.
     */
    template <typename T>
    void align_face(uint16_t *input, std::vector<int> shape, dl::Tensor<T> *output, std::vector<int> &landmarks);

} // namespace face_recognition_tool
