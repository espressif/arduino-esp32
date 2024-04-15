/*
cdecode.h - c header for a base64 decoding algorithm

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/

#ifndef BASE64_CDECODE_H
#define BASE64_CDECODE_H

#define base64_decode_expected_len(n) ((n * 3) / 4)

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum {
    step_a,
    step_b,
    step_c,
    step_d
  } base64_decodestep;

  typedef struct {
    base64_decodestep step;
    char plainchar;
  } base64_decodestate;

  void base64_init_decodestate(base64_decodestate* state_in);

  int base64_decode_value(char value_in);

  int base64_decode_block(const char* code_in, const int length_in, char* plaintext_out, base64_decodestate* state_in);

  int base64_decode_chars(const char* code_in, const int length_in, char* plaintext_out);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* BASE64_CDECODE_H */
