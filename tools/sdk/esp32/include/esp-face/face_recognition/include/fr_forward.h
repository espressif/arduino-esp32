#pragma once

#if __cplusplus
extern "C"
{
#endif

#include "image_util.h"
#include "dl_lib_matrix3d.h"
#include "frmn.h"

#define FACE_WIDTH 56
#define FACE_HEIGHT 56
#define FACE_ID_SIZE 512
#define FACE_REC_THRESHOLD 0.55

#define LEFT_EYE_X 0
#define LEFT_EYE_Y 1
#define RIGHT_EYE_X 6
#define RIGHT_EYE_Y 7
#define NOSE_X 4
#define NOSE_Y 5
#define LEFT_MOUTH_X 2
#define LEFT_MOUTH_Y 3
#define RIGHT_MOUTH_X 8
#define RIGHT_MOUTH_Y 9

#define EYE_DIST_SET 16.5f
#define NOSE_EYE_RATIO_THRES_MIN 0.49f
#define NOSE_EYE_RATIO_THRES_MAX 2.04f


#define ENROLL_NAME_LEN 16
    typedef struct tag_face_id_node
    {
        struct tag_face_id_node *next;           /*!< next face id node */
        char id_name[ENROLL_NAME_LEN];           /*!< name corresponding to the face id  */
        dl_matrix3d_t *id_vec;                   /*!< face id */
    } face_id_node;

    typedef struct
    {
        face_id_node *head;    /*!< head pointer of the id list */
        face_id_node *tail;    /*!< tail pointer of the id list */
        uint8_t count;         /*!< number of enrolled ids */
        uint8_t confirm_times; /*!< images needed for one enrolling */
    } face_id_name_list;

    typedef struct
    {
        uint8_t head;            /*!< head index of the id list */
        uint8_t tail;            /*!< tail index of the id list */
        uint8_t count;           /*!< number of enrolled ids */
        uint8_t size;            /*!< max len of id list */
        uint8_t confirm_times;   /*!< images needed for one enrolling */
        dl_matrix3d_t **id_list; /*!< stores face id vectors */
    } face_id_list;

    /**
     * @brief Initialize face id list.
     * 
     * @param l                    Face id list
     * @param size                 Size of list, one list contains one vector
     * @param confirm_times        Enroll times for one id
     */
    void face_id_init(face_id_list *l, uint8_t size, uint8_t confirm_times);

    /**
     * @brief Initialize face id list with name.
     * 
     * @param l                    Face id list
     * @param size                 Size of list, one list contains one vector
     * @param confirm_times        Enroll times for one id
     */
    void face_id_name_init(face_id_name_list *l, uint8_t size, uint8_t confirm_times);

    /**
     * @brief Alloc memory for aligned face.
     * 
     * @return dl_matrix3du_t*          Size: 1xFACE_WIDTHxFACE_HEIGHTx3
     */
    dl_matrix3du_t *aligned_face_alloc();

    /**@{*/
    /**
     * @brief Align detected face to average face according to landmark.
     * 
     * @param onet_boxes        Output of MTMN with box and landmark
     * @param src               Image matrix, rgb888 format
     * @param dest              Output image
     * @return ESP_OK           Input face is good for recognition
     * @return ESP_FAIL         Input face is not good for recognition
     */
    int8_t align_face_rot(box_array_t *onet_boxes,
                      dl_matrix3du_t *src,
                      dl_matrix3du_t *dest);
    
    int8_t align_face_sim(box_array_t *onet_boxes,
                   dl_matrix3du_t *src,
                   dl_matrix3du_t *dest);
    
    inline int8_t align_face(box_array_t *onet_boxes,
                       dl_matrix3du_t *src,
                       dl_matrix3du_t *dest)
    {
        return align_face_sim(onet_boxes, src, dest);              
    }
    /**@}*/

    /**
     * @brief Run the face recognition model to get the face feature
     * 
     * @param aligned_face      A 56x56x3 image, the variable need to do align_face first
     * @return face_id          A 512 vector, size (1, 1, 1, 512)
     */
    dl_matrix3d_t *get_face_id(dl_matrix3du_t *aligned_face);

    /**
     * @brief Add src_id to dest_id
     * 
     * @param dest_id       Face id after accumulation
     * @param src_id        Face id to be added
     */
    void add_face_id(dl_matrix3d_t *dest_id,
                     dl_matrix3d_t *src_id);

    /**
     * @brief Match face with the id_list, and return matched_id.
     *
     * @param l                     An ID list 
     * @param algined_face          An aligned face
     * @return int8_t               Matched face id
     */
    int8_t recognize_face(face_id_list *l, dl_matrix3du_t *algined_face);

    /**
     * @brief Match face id with the id_list, and return matched face id node.
     * 
     * @param l 
     * @param face_id 
     * @return face_id_node* 
     */
    face_id_node *recognize_face_with_name(face_id_name_list *l, dl_matrix3d_t *face_id);
    
    /**
     * @brief Produce face id according to the input aligned face, and save it to dest_id.
     * 
     * @param l                     Face id list
     * @param aligned_face          An aligned face
     * @param enroll_confirm_times  Confirm times for each face id enrollment
     * @return -1                   Wrong input enroll_confirm_times
     * @return 0                    Enrollment finish
     * @return >=1                  The left piece of aligned faces should be input
     */
    int8_t enroll_face(face_id_list *l, dl_matrix3du_t *aligned_face);

    /**
     * @brief Produce face id according to the input aligned face, and save the id-name pairs to dest_id
     * 
     * @param l                      Face id list with name 
     * @param new_id                 A face id that need to be enrolled
     * @param name                   name corresponding to the face id  
     * @return int8_t                The left piece of aligned faces should be input
     */
    int8_t enroll_face_with_name(face_id_name_list *l,
                                 dl_matrix3d_t *new_id,
                                 char *name);

    /**
     * @brief Delete the enrolled face IDs
     * 
     * @param l            Face id list
     * @return uint8_t     The number of IDs remaining in face id list
     */
    uint8_t delete_face(face_id_list *l);

    /**
     * @brief Delete the enrolled face IDs and associated names
     * 
     * @param l             Face id list
     * @param name          The name that needs to be deleted
     * @return int8_t       The number of IDs remaining in face id list
     */
    int8_t delete_face_with_name(face_id_name_list *l, char *name);
    
    /**
     * @brief               Delete all the enrolled face IDs and names paris
     * 
     * @param l             Face id list with names
     */
    void delete_face_all_with_name(face_id_name_list *l);
#if __cplusplus
}
#endif
