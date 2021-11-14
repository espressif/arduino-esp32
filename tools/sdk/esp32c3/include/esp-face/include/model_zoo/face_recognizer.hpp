#pragma once

#include "dl_variable.hpp"
#include "face_recognition_tool.hpp"
#include <vector>

using namespace dl;

/**
 * @brief 
 * 
 * @tparam feature_t 
 */
template<typename feature_t>
class FaceRecognizer
{
    public:
        /**
         * @brief Construct a new Face Recognizer object
         * 
         */
        FaceRecognizer();

        /**
         * @brief Destroy the Face Recognizer object
         * 
         */
        virtual ~FaceRecognizer();

        void *model;

        /**
         * @brief Set the face recognition threshold [-1, 1], default thresh: 0.55
         * Note: If the similarity of two faces is greater than the threshold, they will be judged as the same person
         * 
         * @param thresh 
         */
        void set_thresh(float thresh);

        /**
         * @brief Get the current threshold of recognizer.
         * 
         * @return float current threshold.
         */
        float get_thresh();

        /**
         * @brief Get the input shape of the recognizer.
         * 
         * @return std::vector<int> the input shape of the recognizer. 
         */
        std::vector<int> get_input_shape();

        /**
         * @brief do forward
         * 
         * @param model_input            the input data of the face recognition model. 
         * Note: the input data should have been preprocessed.
         * @return Tensor<feature_t>&    the output of the face recognition model.
         */
        Tensor<feature_t> &forward(Tensor<feature_t> &model_input);

        /**
         * @brief recognize face
         * 
         * @param image_input       the pointer of the input image with format bgr565.
         * @param shape             the shape of the input image
         * @param landmarks         face landmarks coordinates
         * @return face_info_t      the recognition result.
         */
        face_info_t recognize(uint16_t *image_input, std::vector<int> shape, std::vector<int> &landmarks);
        
        /**
         * @brief recognize face
         * 
         * @param image_input        the pointer of the input image with format bgr565.
         * @param shape              the shape of the input image  
         * @param aligned_face       the Tensor to store the intermeidate aligned face.
         * @param landmarks          face landmarks coordinates
         * @return face_info_t       the recognition result.
         */
        face_info_t recognize(uint16_t *image_input, std::vector<int> shape, Tensor<uint8_t> &aligned_face, std::vector<int> &landmarks);

        /**
         * @brief recognize face
         * 
         * @param image_input         the Tensor of input image with format bgr888.
         * @param landmarks           face landmarks coordinates
         * @return face_info_t        the recognition result.
         */
        face_info_t recognize(Tensor<uint8_t> &image_input, std::vector<int> &landmarks);

        /**
         * @brief recognize face
         * 
         * @param image_input           the Tensor of input image with format bgr888.
         * @param aligned_face          the Tensor to store the intermeidate aligned face.
         * @param landmarks             face landmarks coordinates
         * @return face_info_t          the recognition result.
         */
        face_info_t recognize(Tensor<uint8_t> &image_input, Tensor<uint8_t> &aligned_face, std::vector<int> &landmarks);

        /**
         * @brief recognize face
         * 
         * @param aligned_face          the Tensor of the input aligned face with format bgr888.
         * @return face_info_t          the recognition result.
         */
        face_info_t recognize(Tensor<uint8_t> &aligned_face);

        /**
         * @brief recognize the face embedding.
         * 
         * @param emb  the normalized face embbeding.
         * @return face_info_t  the recognition result.
         */
        face_info_t recognize(Tensor<float> &emb);

        /**
         * @brief Get the index of the enrolled ids 
         * 
         * @return std::vector<int> a vector of face ids index
         */
        std::vector<face_info_t> get_enrolled_ids();

        /**
         * @brief Get the face embedding 
         * 
         * @param id the face id index
         * @return Tensor<float>  the face embedding of the face id index. 
         *                        if there is no matched id return the embedding of last input image.
         */
        Tensor<float> &get_face_emb(int id=-1);

        /**
         * @brief Get the number of enrolled id
         * 
         * @return int the number of enrolled id
         */
        int get_enrolled_id_num();

        /**
         * @brief enroll face id
         * 
         * @param image_input       the pointer of the input image with format bgr565.
         * @param shape             the shape of the input image
         * @param landmarks         face landmarks coordinates
         * @param name              name of the face id.
         * @return  int             the face id index of the enrolled embedding.
         */
        int enroll_id(uint16_t *image_input, std::vector<int> shape, std::vector<int> &landmarks, std::string name="");
        
        /**
         * @brief enroll face id
         * 
         * @param image_input        the pointer of the input image with format bgr565.
         * @param shape              the shape of the input image  
         * @param aligned_face       the Tensor to store the intermeidate aligned face.
         * @param landmarks          face landmarks coordinates
         * @param name               name of the face id.
         * @return  int              the face id index of the enrolled embedding.
         */
        int enroll_id(uint16_t *image_input, std::vector<int> shape, Tensor<uint8_t> &aligned_face, std::vector<int> &landmarks, std::string name="");

        /**
         * @brief enroll face id
         * 
         * @param image_input         the Tensor of input image with format bgr888.
         * @param landmarks           face landmarks coordinates
         * @param name                name of the face id.
         * @return  int               the face id index of the enrolled embedding.
         */
        int enroll_id(Tensor<uint8_t> &image_input, std::vector<int> &landmarks, std::string name="");

        /**
         * @brief enroll face id
         * 
         * @param image_input           the Tensor of input image with format bgr888.
         * @param aligned_face          the Tensor to store the intermeidate aligned face.
         * @param landmarks             face landmarks coordinates
         * @param name                  name of the face id.
         * @return  int                 the face id index of the enrolled embedding.
         */
        int enroll_id(Tensor<uint8_t> &image_input, Tensor<uint8_t> &aligned_face, std::vector<int> &landmarks, std::string name="");

        /**
         * @brief enroll face id
         * 
         * @param aligned_face          the Tensor of the input aligned face with format bgr888.
         * @param name                  name of the face id.
         * @return  int                 the face id index of the enrolled embedding.
         */
        int enroll_id(Tensor<uint8_t> &aligned_face, std::string name="");
        
        /**
         * @brief       enroll the normalzied face embedding.
         * 
         * @param emb   the normalized face embbeding.
         * @param name  name of the face id.
         * @return int  the face id index of the enrolled embedding.
         */
        int enroll_id(Tensor<float> &emb, std::string name="");

        /**
         * @brief       delete the last enrolled face id.
         * 
         * @return int  the number of remained face ids.
         *              if the face ids list is empty, return -1
         */
        int delete_id();

        /**
         * @brief       delete the face id with id index.
         * 
         * @param id    face id index.
         * @return int  the number of remained face ids.
         *              if there is no matched id return -1
         */
        int delete_id(int id);
};