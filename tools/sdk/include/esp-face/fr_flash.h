#pragma once

#if __cplusplus
extern "C"
{
#endif

#include "fr_forward.h"

#define FR_FLASH_TYPE   32
#define FR_FLASH_SUBTYPE   32
#define FR_FLASH_PARTITION_NAME "fr"
#define FR_FLASH_INFO_FLAG 12138

	 /**
     * @brief Produce face id according to the input aligned face, and save it to dest_id and flash.
     * 
     * @param l                     Face id list
     * @param aligned_face          An aligned face
     * @return -2                   Flash partition not found
     * @return 0                    Enrollment finish
     * @return >=1                  The left piece of aligned faces should be input
     */
    int8_t enroll_face_id_to_flash(face_id_list *l,
            dl_matrix3du_t *aligned_face);

    int8_t enroll_face_id_to_flash_with_name(face_id_name_list *l,
            dl_matrix3d_t *new_id,
            char *name);
    /**
     * @brief Read the enrolled face IDs from the flash.
     * 
     * @param l                     Face id list
     * @return int8_t               The number of IDs remaining in flash
     */
    int8_t read_face_id_from_flash(face_id_list *l);

    int8_t read_face_id_from_flash_with_name(face_id_name_list *l);

    /**
     * @brief Delete the enrolled face IDs in the flash.
     * 
     * @param l                     Face id list
     * @return int8_t               The number of IDs remaining in flash
     */
    int8_t delete_face_id_in_flash(face_id_list *l);
	int8_t delete_face_id_in_flash_with_name(face_id_name_list *l, char *name);
    void delete_face_all_in_flash_with_name(face_id_name_list *l);

#if __cplusplus
}
#endif
