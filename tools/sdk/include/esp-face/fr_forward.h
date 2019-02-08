#pragma once

#if __cplusplus
extern "C"
{
#endif

#include "image_util.h"
#include "dl_lib.h"
#include "frmn.h"

#define FACE_WIDTH 56
#define FACE_HEIGHT 56
#define FACE_ID_SIZE 512
#define FACE_REC_THRESHOLD 0.5

#define LEFT_EYE_X 0
#define LEFT_EYE_Y 1
#define RIGHT_EYE_X 6
#define RIGHT_EYE_Y 7
#define NOSE_X 4
#define NOSE_Y 5

#define EYE_DIST_SET 16.5f
#define NOSE_EYE_RATIO_THRES_MIN 0.49f
#define NOSE_EYE_RATIO_THRES_MAX 2.04f

#define FLASH_INFO_FLAG 12138
#define FLASH_PARTITION_NAME "fr"

/**
 * @brief      HTTP Client events data
 */
    typedef struct
    {
        uint8_t head;               /*!< head index of the id list */
        uint8_t tail;               /*!< tail index of the id list */
        uint8_t count;              /*!< number of enrolled ids */
        uint8_t size;               /*!< max len of id list */
        uint8_t confirm_times;      /*!< images needed for one enrolling */
        dl_matrix3d_t **id_list;    /*!< stores face id vectors */
    } face_id_list;


    /**
     * @brief Initialize face id list
     * 
     * @param l                 Face id list
     * @param size              Size of list, one list contains one vector
     * @param confirm_times     Enroll times for one id
     * @return dl_matrix3du_t*          Size: 1xFACE_WIDTHxFACE_HEIGHTx3
     */
    void face_id_init(face_id_list *l, uint8_t size, uint8_t confirm_times);

    /**
     * @brief Alloc memory for aligned face.
     * 
     * @return dl_matrix3du_t*          Size: 1xFACE_WIDTHxFACE_HEIGHTx3
     */
    dl_matrix3du_t *aligned_face_alloc();

    /**
     * @brief Align detected face to average face according to landmark
     * 
     * @param onet_boxes        Output of MTMN with box and landmark
     * @param src               Image matrix, rgb888 format
     * @param dest              Output image
     * @return ESP_OK           Input face is good for recognition
     * @return ESP_FAIL         Input face is not good for recognition
     */
    int8_t align_face(box_array_t *onet_boxes,
                      dl_matrix3du_t *src,
                      dl_matrix3du_t *dest);

    /**
     * @brief Add src_id to dest_id
     * 
     * @param dest_id 
     * @param src_id 
     */
    void add_face_id(dl_matrix3d_t *dest_id,
                     dl_matrix3d_t *src_id);

    /**
     * @brief Match face with the id_list, and return matched_id.
     * 
     * @param algined_face          An aligned face
     * @param id_list               An ID list
     * @return int8_t               Matched face id
     */
    int8_t recognize_face(face_id_list *l,
                            dl_matrix3du_t *algined_face);

    /**
     * @brief Produce face id according to the input aligned face, and save it to dest_id.
     * 
     * @param l                     face id list
     * @param aligned_face          An aligned face
     * @param enroll_confirm_times  Confirm times for each face id enrollment
     * @return -1                   Wrong input enroll_confirm_times
     * @return 0                    Enrollment finish
     * @return >=1                  The left piece of aligned faces should be input
     */
    int8_t enroll_face(face_id_list *l, 
                    dl_matrix3du_t *aligned_face);

    /**
     * @brief Alloc memory for aligned face.
     * 
     * @param l                     face id list
     * @return uint8_t              left count
     */
    uint8_t delete_face(face_id_list *l);
#if __cplusplus
}
#endif
