/*
  Based on "Calculation of PI(= 3.14159...) using FFT and AGM" by T.Ooura, Nov. 1999.
  https://github.com/Fibonacci43/SuperPI
  Modified for Arduino by Lucas Saavedra Vaz, 2024.
*/

#pragma once

#include <ctype.h>

#define PI_FFTC_VER "ver. LG1.1.2-MP1.5.2a.memsave"

/* Please check the following macros before compiling */
#ifndef DBL_ERROR_MARGIN
#define DBL_ERROR_MARGIN 0.4 /* must be < 0.5 */
#endif

#define DGTINT     short int /* sizeof(DGTINT) == 2 */
#define DGTINT_MAX SHRT_MAX

#define DGT_PACK       10
#define DGT_PACK_LINE  5
#define DGT_LINE_BLOCK 20

void pi_calc(int nfft);
void mp_load_0(int n, int radix, int out[]);
void mp_load_1(int n, int radix, int out[]);
void mp_round(int n, int radix, int m, int inout[]);
int mp_cmp(int n, int radix, int in1[], int in2[]);
void mp_add(int n, int radix, int in1[], int in2[], int out[]);
void mp_sub(int n, int radix, int in1[], int in2[], int out[]);
void mp_imul(int n, int radix, int in1[], int in2, int out[]);
int mp_idiv(int n, int radix, int in1[], int in2, int out[]);
void mp_idiv_2(int n, int radix, int in[], int out[]);
double mp_mul_radix_test(int n, int radix, int nfft, double tmpfft[]);
void mp_mul(int n, int radix, int in1[], int in2[], int out[], int tmp[], int nfft, double tmp1fft[], double tmp2fft[], double tmp3fft[]);
void mp_squ(int n, int radix, int in[], int out[], int tmp[], int nfft, double tmp1fft[], double tmp2fft[]);
void mp_mulhf(int n, int radix, int in1[], int in2[], int out[], int tmp[], int nfft, double in1fft[], double tmpfft[]);
void mp_mulhf_use_in1fft(int n, int radix, double in1fft[], int in2[], int out[], int tmp[], int nfft, double tmpfft[]);
void mp_squhf_use_infft(int n, int radix, double infft[], int in[], int out[], int tmp[], int nfft, double tmpfft[]);
void mp_mulh(int n, int radix, int in1[], int in2[], int out[], int nfft, double in1fft[], double outfft[]);
void mp_squh(int n, int radix, int in[], int out[], int nfft, double outfft[]);
int mp_inv(int n, int radix, int in[], int out[], int tmp1[], int tmp2[], int nfft, double tmp1fft[], double tmp2fft[]);
int mp_sqrt(int n, int radix, int in[], int out[], int tmp1[], int tmp2[], int nfft, double tmp1fft[], double tmp2fft[]);
int mp_invisqrt(int n, int radix, int in, int out[], int tmp1[], int tmp2[], int nfft, double tmp1fft[], double tmp2fft[]);
void mp_sprintf(int n, int log10_radix, int in[], char out[]);
void mp_sscanf(int n, int log10_radix, char in[], int out[]);
