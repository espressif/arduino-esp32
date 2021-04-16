/* Code taken from: https://github.com/shahpiyushv/json_parser */
#ifndef _JSON_PARSER_H_
#define _JSON_PARSER_H_

#include <jsmn-changed.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define OS_SUCCESS  0
#define OS_FAIL     -1

typedef _jsmn_parser json_parser_t;
typedef _jsmntok_t json_tok_t;

typedef struct {
	json_parser_t parser;
	char *js;
	json_tok_t *tokens;
	json_tok_t *cur;
	int num_tokens;
} jparse_ctx_t;

int json_parse_start(jparse_ctx_t *jctx, char *js, int len);
int json_parse_end(jparse_ctx_t *jctx);

int json_obj_get_array(jparse_ctx_t *jctx, char *name, int *num_elem);
int json_obj_leave_array(jparse_ctx_t *jctx);
int json_obj_get_object(jparse_ctx_t *jctx, char *name);
int json_obj_leave_object(jparse_ctx_t *jctx);
int json_obj_get_bool(jparse_ctx_t *jctx, char *name, bool *val);
int json_obj_get_int(jparse_ctx_t *jctx, char *name, int *val);
int json_obj_get_int64(jparse_ctx_t *jctx, char *name, int64_t *val);
int json_obj_get_float(jparse_ctx_t *jctx, char *name, float *val);
int json_obj_get_string(jparse_ctx_t *jctx, char *name, char *val, int size);
int json_obj_get_strlen(jparse_ctx_t *jctx, char *name, int *strlen);
int json_obj_get_object_str(jparse_ctx_t *jctx, char *name, char *val, int size);
int json_obj_get_object_strlen(jparse_ctx_t *jctx, char *name, int *strlen);
int json_obj_get_array_str(jparse_ctx_t *jctx, char *name, char *val, int size);
int json_obj_get_array_strlen(jparse_ctx_t *jctx, char *name, int *strlen);

int json_arr_get_array(jparse_ctx_t *jctx, uint32_t index);
int json_arr_leave_array(jparse_ctx_t *jctx);
int json_arr_get_object(jparse_ctx_t *jctx, uint32_t index);
int json_arr_leave_object(jparse_ctx_t *jctx);
int json_arr_get_bool(jparse_ctx_t *jctx, uint32_t index, bool *val);
int json_arr_get_int(jparse_ctx_t *jctx, uint32_t index, int *val);
int json_arr_get_int64(jparse_ctx_t *jctx, uint32_t index, int64_t *val);
int json_arr_get_float(jparse_ctx_t *jctx, uint32_t index, float *val);
int json_arr_get_string(jparse_ctx_t *jctx, uint32_t index, char *val, int size);
int json_arr_get_strlen(jparse_ctx_t *jctx, uint32_t index, int *strlen);

#ifdef __cplusplus
}
#endif

#endif /* _JSON_PARSER_H_ */
