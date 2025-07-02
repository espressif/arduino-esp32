/*
  Based on "Calculation of PI(= 3.14159...) using FFT and AGM" by T.Ooura, Nov. 1999.
  https://github.com/Fibonacci43/SuperPI
  Modified for Arduino by Lucas Saavedra Vaz, 2024.
*/

#include <Arduino.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>

#include "fftsg_h.h"
#include "pi_fftcs.h"

void pi_calc(int nfft) {
  int log2_nfft, radix, log10_radix, n, npow, nprc;
#if PRINT_DIGITS
  int j = 0, k = 0, l = 0;
#endif
  double err;
  int *a, *b, *c, *e, *i1, *i2;
  double *d1, *d2, *d3;
  char *dgt;
  uint32_t start_time;
  double elap_time, loop_time;
  log_d("Calculation of PI using FFT and AGM, %s", PI_FFTC_VER);

  // DGTINT is defined as short int, so it should be 2 bytes
  assert(sizeof(DGTINT) == 2);

  log_d("initializing...");
  nfft /= 4;
  start_time = millis();
  for (log2_nfft = 1; (1 << log2_nfft) < nfft; log2_nfft++);
  nfft = 1 << log2_nfft;
  n = nfft + 2;
  a = (int *)malloc(2 * sizeof(int) + n * sizeof(DGTINT));
  b = (int *)malloc(2 * sizeof(int) + n * sizeof(DGTINT));
  c = (int *)malloc(2 * sizeof(int) + n * sizeof(DGTINT));
  e = (int *)malloc(2 * sizeof(int) + n * sizeof(DGTINT));
  i1 = (int *)malloc(2 * sizeof(int) + n * sizeof(DGTINT));
  i2 = (int *)malloc(2 * sizeof(int) + n * sizeof(DGTINT));
  d1 = (double *)malloc((nfft + 2) * sizeof(double));
  d2 = (double *)malloc((nfft + 2) * sizeof(double));
  d3 = (double *)malloc((nfft + 2) * sizeof(double));
  if (d3 == NULL) {
    printf("Allocation Failure!\n");
    exit(1);
  }
  /* ---- radix test ---- */
  log10_radix = 1;
  radix = 10;
  err = mp_mul_radix_test(n, radix, nfft, d1);
  err += DBL_EPSILON * (n * radix * radix / 4);
  while (100 * err < DBL_ERROR_MARGIN && radix <= DGTINT_MAX / 20) {
    err *= 100;
    log10_radix++;
    radix *= 10;
  }
  log_d("nfft= %d, radix= %d, error_margin= %g", nfft, radix, err);
  log_d("calculating %d digits of PI...", log10_radix * (n - 2));
  /*
   * ---- a formula based on the AGM (Arithmetic-Geometric Mean) ----
   *   c = sqrt(0.125);
   *   a = 1 + 3 * c;
   *   b = sqrt(a);
   *   e = b - 0.625;
   *   b = 2 * b;
   *   c = e - c;
   *   a = a + e;
   *   npow = 4;
   *   do {
   *       npow = 2 * npow;
   *       e = (a + b) / 2;
   *       b = sqrt(a * b);
   *       e = e - b;
   *       b = 2 * b;
   *       c = c - e;
   *       a = e + b;
   *   } while (e > SQRT_SQRT_EPSILON);
   *   e = e * e / 4;
   *   a = a + b;
   *   pi = (a * a - e - e / 2) / (a * c - e) / npow;
   * ---- modification ----
   *   This is a modified version of Gauss-Legendre formula
   *   (by T.Ooura). It is faster than original version.
   * ---- reference ----
   *   1. E.Salamin,
   *      Computation of PI Using Arithmetic-Geometric Mean,
   *      Mathematics of Computation, Vol.30 1976.
   *   2. R.P.Brent,
   *      Fast Multiple-Precision Evaluation of Elementary Functions,
   *      J. ACM 23 1976.
   *   3. D.Takahasi, Y.Kanada,
   *      Calculation of PI to 51.5 Billion Decimal Digits on
   *      Distributed Memoriy Parallel Processors,
   *      Transactions of Information Processing Society of Japan,
   *      Vol.39 No.7 1998.
   *   4. T.Ooura,
   *      Improvement of the PI Calculation Algorithm and
   *      Implementation of Fast Multiple-Precision Computation,
   *      Information Processing Society of Japan SIG Notes,
   *      98-HPC-74, 1998.
   */
  /* ---- c = 1 / sqrt(8) ---- */
  mp_invisqrt(n, radix, 8, c, i1, i2, nfft, d1, d2);
  /* ---- a = 1 + 3 * c ---- */
  mp_imul(n, radix, c, 3, e);
  mp_sscanf(n, log10_radix, (char *)"1", a);
  mp_add(n, radix, a, e, a);
  /* ---- b = sqrt(a) ---- */
  mp_sqrt(n, radix, a, b, i1, i2, nfft, d1, d2);
  /* ---- e = b - 0.625 ---- */
  mp_sscanf(n, log10_radix, (char *)"0.625", e);
  mp_sub(n, radix, b, e, e);
  /* ---- b = 2 * b ---- */
  mp_add(n, radix, b, b, b);
  /* ---- c = e - c ---- */
  mp_sub(n, radix, e, c, c);
  /* ---- a = a + e ---- */
  mp_add(n, radix, a, e, a);
  log_d("AGM iteration");
  npow = 4;
  elap_time = ((double)(millis() - start_time)) / 1000;

  do {
    uint32_t start_loop_time = millis();
    npow *= 2;
    /* ---- e = (a + b) / 2 ---- */
    mp_add(n, radix, a, b, e);
    mp_idiv_2(n, radix, e, e);
    /* ---- b = sqrt(a * b) ---- */
    mp_mul(n, radix, a, b, a, i1, nfft, d1, d2, d3);
    mp_sqrt(n, radix, a, b, i1, i2, nfft, d1, d2);
    /* ---- e = e - b ---- */
    mp_sub(n, radix, e, b, e);
    /* ---- b = 2 * b ---- */
    mp_add(n, radix, b, b, b);
    /* ---- c = c - e ---- */
    mp_sub(n, radix, c, e, c);
    /* ---- a = e + b ---- */
    mp_add(n, radix, e, b, a);
    /* ---- convergence check ---- */
    nprc = -e[1];
    if (e[0] == 0) {
      nprc = n;
    }
    loop_time = ((double)(millis() - start_loop_time)) / 1000;
    elap_time += loop_time;
    log_d("precision= %d: %0.2f sec", 4 * nprc * log10_radix, loop_time);
  } while (4 * nprc <= n);
  start_time = millis();
  /* ---- e = e * e / 4 (half precision) ---- */
  mp_idiv_2(n, radix, e, e);
  mp_squh(n, radix, e, e, nfft, d1);
  /* ---- a = a + b ---- */
  mp_add(n, radix, a, b, a);
  /* ---- a = (a * a - e - e / 2) / (a * c - e) / npow ---- */
  mp_mulhf(n, radix, a, c, c, i1, nfft, d1, d2);
  mp_sub(n, radix, c, e, c);
  mp_inv(n, radix, c, b, i1, i2, nfft, d2, d3);
  mp_squhf_use_infft(n, radix, d1, a, a, i1, nfft, d2);
  mp_sub(n, radix, a, e, a);
  mp_idiv_2(n, radix, e, e);
  mp_sub(n, radix, a, e, a);
  mp_mul(n, radix, a, b, a, i1, nfft, d1, d2, d3);
  mp_idiv(n, radix, a, npow, a);
  /* ---- output ---- */
  dgt = (char *)d1;
  mp_sprintf(n - 1, log10_radix, a, dgt);
  elap_time = ((double)(millis() - start_time)) / 1000;

#if PRINT_DIGITS
  do {
    if (!isdigit(*dgt)) {
      if (isalpha(*dgt) != 0) {
        fputc('\n', stdout);
        fputc('\n', stdout);
      }
      fputc(*dgt, stdout);
      fputc('\n', stdout);
      fputc('\n', stdout);
      j = 0;
      k = 0;
      l = 0;
      continue;
    }
    fputc(*dgt, stdout);
    if (++j >= DGT_PACK) {
      j = 0;
      if (++k >= DGT_PACK_LINE) {
        k = 0;
        fputc('\n', stdout);
        if (++l >= DGT_LINE_BLOCK) {
          l = 0;
          fputc('\n', stdout);
        }
      } else {
        fputc(' ', stdout);
      }
    }
  } while (*dgt++ && *dgt != 'e');
  fputc('\n', stdout);
  fprintf(stdout, "%s\n", dgt);
#endif

  free(d3);
  free(d2);
  free(d1);
  free(i2);
  free(i1);
  free(e);
  free(c);
  free(b);
  free(a);
  /* ---- difftime ---- */
  log_d("%0.2f sec. (real time)", elap_time);
}

/* -------- multiple precision routines -------- */

/* -------- mp_load routines -------- */

void mp_load_0(int n, int radix, int out[]) {
  int j;
  DGTINT *outr;

  outr = ((DGTINT *)&out[2]) - 2;
  out[0] = 0;
  out[1] = 0;
  for (j = 2; j <= n + 1; j++) {
    outr[j] = 0;
  }
}

void mp_load_1(int n, int radix, int out[]) {
  int j;
  DGTINT *outr;

  outr = ((DGTINT *)&out[2]) - 2;
  out[0] = 1;
  out[1] = 0;
  outr[2] = 1;
  for (j = 3; j <= n + 1; j++) {
    outr[j] = 0;
  }
}

void mp_round(int n, int radix, int m, int inout[]) {
  int j, x;
  DGTINT *inoutr;

  inoutr = ((DGTINT *)&inout[2]) - 2;
  if (m < n) {
    for (j = n + 1; j > m + 2; j--) {
      inoutr[j] = 0;
    }
    x = 2 * inoutr[m + 2];
    inoutr[m + 2] = 0;
    if (x >= radix) {
      for (j = m + 1; j >= 2; j--) {
        x = inoutr[j] + 1;
        if (x < radix) {
          inoutr[j] = (DGTINT)x;
          break;
        }
        inoutr[j] = 0;
      }
      if (x >= radix) {
        inoutr[2] = 1;
        inout[1]++;
      }
    }
  }
}

/* -------- mp_add routines -------- */

int mp_cmp(int n, int radix, int in1[], int in2[]) {
  int mp_unsgn_cmp(int n, int in1[], int in2[]);

  if (in1[0] > in2[0]) {
    return 1;
  } else if (in1[0] < in2[0]) {
    return -1;
  }
  return in1[0] * mp_unsgn_cmp(n, &in1[1], &in2[1]);
}

void mp_add(int n, int radix, int in1[], int in2[], int out[]) {
  int mp_unsgn_cmp(int n, int in1[], int in2[]);
  int mp_unexp_add(int n, int radix, int expdif, DGTINT in1[], DGTINT in2[], DGTINT out[]);
  int mp_unexp_sub(int n, int radix, int expdif, DGTINT in1[], DGTINT in2[], DGTINT out[]);
  int outsgn, outexp, expdif;

  expdif = in1[1] - in2[1];
  outexp = in1[1];
  if (expdif < 0) {
    outexp = in2[1];
  }
  outsgn = in1[0] * in2[0];
  if (outsgn >= 0) {
    if (outsgn > 0) {
      outsgn = in1[0];
    } else {
      outsgn = in1[0] + in2[0];
      outexp = in1[1] + in2[1];
      expdif = 0;
    }
    if (expdif >= 0) {
      outexp += mp_unexp_add(n, radix, expdif, (DGTINT *)&in1[2], (DGTINT *)&in2[2], (DGTINT *)&out[2]);
    } else {
      outexp += mp_unexp_add(n, radix, -expdif, (DGTINT *)&in2[2], (DGTINT *)&in1[2], (DGTINT *)&out[2]);
    }
  } else {
    outsgn = mp_unsgn_cmp(n, &in1[1], &in2[1]);
    if (outsgn >= 0) {
      expdif = mp_unexp_sub(n, radix, expdif, (DGTINT *)&in1[2], (DGTINT *)&in2[2], (DGTINT *)&out[2]);
    } else {
      expdif = mp_unexp_sub(n, radix, -expdif, (DGTINT *)&in2[2], (DGTINT *)&in1[2], (DGTINT *)&out[2]);
    }
    outexp -= expdif;
    outsgn *= in1[0];
    if (expdif == n) {
      outsgn = 0;
    }
  }
  if (outsgn == 0) {
    outexp = 0;
  }
  out[0] = outsgn;
  out[1] = outexp;
}

void mp_sub(int n, int radix, int in1[], int in2[], int out[]) {
  int mp_unsgn_cmp(int n, int in1[], int in2[]);
  int mp_unexp_add(int n, int radix, int expdif, DGTINT in1[], DGTINT in2[], DGTINT out[]);
  int mp_unexp_sub(int n, int radix, int expdif, DGTINT in1[], DGTINT in2[], DGTINT out[]);
  int outsgn, outexp, expdif;

  expdif = in1[1] - in2[1];
  outexp = in1[1];
  if (expdif < 0) {
    outexp = in2[1];
  }
  outsgn = in1[0] * in2[0];
  if (outsgn <= 0) {
    if (outsgn < 0) {
      outsgn = in1[0];
    } else {
      outsgn = in1[0] - in2[0];
      outexp = in1[1] + in2[1];
      expdif = 0;
    }
    if (expdif >= 0) {
      outexp += mp_unexp_add(n, radix, expdif, (DGTINT *)&in1[2], (DGTINT *)&in2[2], (DGTINT *)&out[2]);
    } else {
      outexp += mp_unexp_add(n, radix, -expdif, (DGTINT *)&in2[2], (DGTINT *)&in1[2], (DGTINT *)&out[2]);
    }
  } else {
    outsgn = mp_unsgn_cmp(n, &in1[1], &in2[1]);
    if (outsgn >= 0) {
      expdif = mp_unexp_sub(n, radix, expdif, (DGTINT *)&in1[2], (DGTINT *)&in2[2], (DGTINT *)&out[2]);
    } else {
      expdif = mp_unexp_sub(n, radix, -expdif, (DGTINT *)&in2[2], (DGTINT *)&in1[2], (DGTINT *)&out[2]);
    }
    outexp -= expdif;
    outsgn *= in1[0];
    if (expdif == n) {
      outsgn = 0;
    }
  }
  if (outsgn == 0) {
    outexp = 0;
  }
  out[0] = outsgn;
  out[1] = outexp;
}

/* -------- mp_add child routines -------- */

int mp_unsgn_cmp(int n, int in1[], int in2[]) {
  int j, cmp;
  DGTINT *in1r, *in2r;

  in1r = ((DGTINT *)&in1[1]) - 1;
  in2r = ((DGTINT *)&in2[1]) - 1;
  cmp = in1[0] - in2[0];
  for (j = 1; j <= n && cmp == 0; j++) {
    cmp = in1r[j] - in2r[j];
  }
  if (cmp > 0) {
    cmp = 1;
  } else if (cmp < 0) {
    cmp = -1;
  }
  return cmp;
}

int mp_unexp_add(int n, int radix, int expdif, DGTINT in1[], DGTINT in2[], DGTINT out[]) {
  int j, x, carry;

  carry = 0;
  if (expdif == 0 && in1[0] + in2[0] >= radix) {
    x = in1[n - 1] + in2[n - 1];
    carry = x >= radix ? -1 : 0;
    for (j = n - 1; j > 0; j--) {
      x = in1[j - 1] + in2[j - 1] - carry;
      carry = x >= radix ? -1 : 0;
      out[j] = (DGTINT)(x - (radix & carry));
    }
    out[0] = (DGTINT)-carry;
  } else {
    if (expdif > n) {
      expdif = n;
    }
    for (j = n - 1; j >= expdif; j--) {
      x = in1[j] + in2[j - expdif] - carry;
      carry = x >= radix ? -1 : 0;
      out[j] = (DGTINT)(x - (radix & carry));
    }
    for (j = expdif - 1; j >= 0; j--) {
      x = in1[j] - carry;
      carry = x >= radix ? -1 : 0;
      out[j] = (DGTINT)(x - (radix & carry));
    }
    if (carry != 0) {
      for (j = n - 1; j > 0; j--) {
        out[j] = out[j - 1];
      }
      out[0] = (DGTINT)-carry;
    }
  }
  return -carry;
}

int mp_unexp_sub(int n, int radix, int expdif, DGTINT in1[], DGTINT in2[], DGTINT out[]) {
  int j, x, borrow, ncancel;

  if (expdif > n) {
    expdif = n;
  }
  borrow = 0;
  for (j = n - 1; j >= expdif; j--) {
    x = in1[j] - in2[j - expdif] + borrow;
    borrow = x < 0 ? -1 : 0;
    out[j] = (DGTINT)(x + (radix & borrow));
  }
  for (j = expdif - 1; j >= 0; j--) {
    x = in1[j] + borrow;
    borrow = x < 0 ? -1 : 0;
    out[j] = (DGTINT)(x + (radix & borrow));
  }
  ncancel = 0;
  for (j = 0; j < n && out[j] == 0; j++) {
    ncancel = j + 1;
  }
  if (ncancel > 0 && ncancel < n) {
    for (j = 0; j < n - ncancel; j++) {
      out[j] = out[j + ncancel];
    }
    for (j = n - ncancel; j < n; j++) {
      out[j] = 0;
    }
  }
  return ncancel;
}

/* -------- mp_imul routines -------- */

void mp_imul(int n, int radix, int in1[], int in2, int out[]) {
  void mp_unsgn_imul(int n, double dradix, int in1[], double din2, int out[]);

  if (in2 > 0) {
    out[0] = in1[0];
  } else if (in2 < 0) {
    out[0] = -in1[0];
    in2 = -in2;
  } else {
    out[0] = 0;
  }
  mp_unsgn_imul(n, radix, &in1[1], in2, &out[1]);
  if (out[0] == 0) {
    out[1] = 0;
  }
}

int mp_idiv(int n, int radix, int in1[], int in2, int out[]) {
  void mp_load_0(int n, int radix, int out[]);
  void mp_unsgn_idiv(int n, double dradix, int in1[], double din2, int out[]);

  if (in2 == 0) {
    return -1;
  }
  if (in2 > 0) {
    out[0] = in1[0];
  } else {
    out[0] = -in1[0];
    in2 = -in2;
  }
  if (in1[0] == 0) {
    mp_load_0(n, radix, out);
    return 0;
  }
  mp_unsgn_idiv(n, radix, &in1[1], in2, &out[1]);
  return 0;
}

void mp_idiv_2(int n, int radix, int in[], int out[]) {
  int j, ix, carry, shift;
  DGTINT *inr, *outr;

  inr = ((DGTINT *)&in[2]) - 2;
  outr = ((DGTINT *)&out[2]) - 2;
  out[0] = in[0];
  shift = 0;
  if (inr[2] == 1) {
    shift = 1;
  }
  out[1] = in[1] - shift;
  carry = -shift;
  for (j = 2; j <= n + 1 - shift; j++) {
    ix = inr[j + shift] + (radix & carry);
    carry = -(ix & 1);
    outr[j] = (DGTINT)(ix >> 1);
  }
  if (shift > 0) {
    outr[n + 1] = (DGTINT)((radix & carry) >> 1);
  }
}

/* -------- mp_imul child routines -------- */

void mp_unsgn_imul(int n, double dradix, int in1[], double din2, int out[]) {
  int j, carry, shift;
  double x, d1_radix;
  DGTINT *in1r, *outr;

  in1r = ((DGTINT *)&in1[1]) - 1;
  outr = ((DGTINT *)&out[1]) - 1;
  d1_radix = 1.0 / dradix;
  carry = 0;
  for (j = n; j >= 1; j--) {
    x = din2 * in1r[j] + carry + 0.5;
    carry = (int)(d1_radix * x);
    outr[j] = (DGTINT)(x - dradix * carry);
  }
  shift = 0;
  x = carry + 0.5;
  while (x > 1) {
    x *= d1_radix;
    shift++;
  }
  out[0] = in1[0] + shift;
  if (shift > 0) {
    while (shift > n) {
      carry = (int)(d1_radix * carry + 0.5);
      shift--;
    }
    for (j = n; j >= shift + 1; j--) {
      outr[j] = outr[j - shift];
    }
    for (j = shift; j >= 1; j--) {
      x = carry + 0.5;
      carry = (int)(d1_radix * x);
      outr[j] = (DGTINT)(x - dradix * carry);
    }
  }
}

void mp_unsgn_idiv(int n, double dradix, int in1[], double din2, int out[]) {
  int j, ix, carry, shift;
  double x, d1_in2;
  DGTINT *in1r, *outr;

  in1r = ((DGTINT *)&in1[1]) - 1;
  outr = ((DGTINT *)&out[1]) - 1;
  d1_in2 = 1.0 / din2;
  shift = 0;
  x = 0;
  do {
    shift++;
    x *= dradix;
    if (shift <= n) {
      x += in1r[shift];
    }
  } while (x < din2 - 0.5);
  x += 0.5;
  ix = (int)(d1_in2 * x);
  carry = (int)(x - din2 * ix);
  outr[1] = (DGTINT)ix;
  shift--;
  out[0] = in1[0] - shift;
  if (shift >= n) {
    shift = n - 1;
  }
  for (j = 2; j <= n - shift; j++) {
    x = in1r[j + shift] + dradix * carry + 0.5;
    ix = (int)(d1_in2 * x);
    carry = (int)(x - din2 * ix);
    outr[j] = (DGTINT)ix;
  }
  for (j = n - shift + 1; j <= n; j++) {
    x = dradix * carry + 0.5;
    ix = (int)(d1_in2 * x);
    carry = (int)(x - din2 * ix);
    outr[j] = (DGTINT)ix;
  }
}

/* -------- mp_mul routines -------- */

double mp_mul_radix_test(int n, int radix, int nfft, double tmpfft[]) {
  void mp_mul_csqu(int nfft, double d1[]);
  double mp_mul_d2i_test(int radix, int nfft, double din[]);
  int j, ndata, radix_2;

  ndata = (nfft >> 1) + 1;
  if (ndata > n) {
    ndata = n;
  }
  tmpfft[nfft + 1] = radix - 1;
  for (j = nfft; j > ndata; j--) {
    tmpfft[j] = 0;
  }
  radix_2 = (radix + 1) / 2;
  for (j = ndata; j > 2; j--) {
    tmpfft[j] = radix_2;
  }
  tmpfft[2] = radix;
  tmpfft[1] = radix - 1;
  tmpfft[0] = 0;
  mp_mul_csqu(nfft, tmpfft);
  return 2 * mp_mul_d2i_test(radix, nfft, tmpfft);
}

void mp_mul(int n, int radix, int in1[], int in2[], int out[], int tmp[], int nfft, double tmp1fft[], double tmp2fft[], double tmp3fft[]) {
  void mp_add(int n, int radix, int in1[], int in2[], int out[]);
  void mp_mul_i2d(int n, int radix, int nfft, int shift, int in[], double dout[]);
  void mp_mul_cmul_nt_out(int nfft, double d1[], double d2[]);
  void mp_mul_cmul_nt_d2(int nfft, double d1[], double d2[]);
  void mp_mul_cmul_nt_d1_add(int nfft, double d1[], double d2[], double d3[]);
  void mp_mul_d2i(int n, int radix, int nfft, double din[], int out[]);
  int n_h, shift;
  DGTINT *in1r, *in2r;

  in1r = ((DGTINT *)&in1[2]) - 2;
  in2r = ((DGTINT *)&in2[2]) - 2;
  shift = (nfft >> 1) + 1;
  while (n > shift) {
    if (in1r[shift + 2] + in2r[shift + 2] != 0) {
      break;
    }
    shift++;
  }
  n_h = n / 2 + 1;
  if (n_h < n - shift) {
    n_h = n - shift;
  }
  /* ---- tmp3fft = (upper) in1 * (lower) in2 ---- */
  mp_mul_i2d(n, radix, nfft, 0, in1, tmp1fft);
  mp_mul_i2d(n, radix, nfft, shift, in2, tmp3fft);
  mp_mul_cmul_nt_out(nfft, tmp1fft, tmp3fft);
  /* ---- tmp = (upper) in1 * (upper) in2 ---- */
  mp_mul_i2d(n, radix, nfft, 0, in2, tmp2fft);
  mp_mul_cmul_nt_d2(nfft, tmp2fft, tmp1fft);
  mp_mul_d2i(n, radix, nfft, tmp1fft, tmp);
  /* ---- tmp3fft += (upper) in2 * (lower) in1 ---- */
  mp_mul_i2d(n, radix, nfft, shift, in1, tmp1fft);
  mp_mul_cmul_nt_d1_add(nfft, tmp2fft, tmp1fft, tmp3fft);
  /* ---- out = tmp + tmp3fft ---- */
  mp_mul_d2i(n_h, radix, nfft, tmp3fft, out);
  mp_add(n, radix, out, tmp, out);
}

void mp_squ(int n, int radix, int in[], int out[], int tmp[], int nfft, double tmp1fft[], double tmp2fft[]) {
  void mp_add(int n, int radix, int in1[], int in2[], int out[]);
  void mp_mul_i2d(int n, int radix, int nfft, int shift, int in[], double dout[]);
  void mp_mul_cmul(int nfft, double d1[], double d2[]);
  void mp_mul_csqu_nt_d1(int nfft, double d1[]);
  void mp_mul_d2i(int n, int radix, int nfft, double din[], int out[]);
  int n_h, shift;
  DGTINT *inr;

  inr = ((DGTINT *)&in[2]) - 2;
  shift = (nfft >> 1) + 1;
  while (n > shift) {
    if (inr[shift + 2] != 0) {
      break;
    }
    shift++;
  }
  n_h = n / 2 + 1;
  if (n_h < n - shift) {
    n_h = n - shift;
  }
  /* ---- tmp = 2 * (upper) in * (lower) in ---- */
  mp_mul_i2d(n, radix, nfft, 0, in, tmp1fft);
  mp_mul_i2d(n, radix, nfft, shift, in, tmp2fft);
  mp_mul_cmul(nfft, tmp1fft, tmp2fft);
  mp_mul_d2i(n_h, radix, nfft, tmp2fft, tmp);
  mp_add(n_h, radix, tmp, tmp, tmp);
  /* ---- out = tmp + ((upper) in)^2 ---- */
  mp_mul_csqu_nt_d1(nfft, tmp1fft);
  mp_mul_d2i(n, radix, nfft, tmp1fft, out);
  mp_add(n, radix, out, tmp, out);
}

void mp_mulhf(int n, int radix, int in1[], int in2[], int out[], int tmp[], int nfft, double in1fft[], double tmpfft[]) {
  void mp_add(int n, int radix, int in1[], int in2[], int out[]);
  void mp_mul_i2d(int n, int radix, int nfft, int shift, int in[], double dout[]);
  void mp_mul_cmul(int nfft, double d1[], double d2[]);
  void mp_mul_cmul_nt_d1(int nfft, double d1[], double d2[]);
  void mp_mul_d2i(int n, int radix, int nfft, double din[], int out[]);
  int n_h, shift;
  DGTINT *in2r;

  in2r = ((DGTINT *)&in2[2]) - 2;
  shift = (nfft >> 1) + 1;
  while (n > shift) {
    if (in2r[shift + 2] != 0) {
      break;
    }
    shift++;
  }
  n_h = n / 2 + 1;
  if (n_h < n - shift) {
    n_h = n - shift;
  }
  /* ---- tmp = (upper) in1 * (upper) in2 ---- */
  mp_mul_i2d(n, radix, nfft, 0, in1, in1fft);
  mp_mul_i2d(n, radix, nfft, 0, in2, tmpfft);
  mp_mul_cmul(nfft, in1fft, tmpfft);
  mp_mul_d2i(n, radix, nfft, tmpfft, tmp);
  /* ---- out = tmp + (upper) in1 * (lower) in2 ---- */
  mp_mul_i2d(n, radix, nfft, shift, in2, tmpfft);
  mp_mul_cmul_nt_d1(nfft, in1fft, tmpfft);
  mp_mul_d2i(n_h, radix, nfft, tmpfft, out);
  mp_add(n, radix, out, tmp, out);
}

void mp_mulhf_use_in1fft(int n, int radix, double in1fft[], int in2[], int out[], int tmp[], int nfft, double tmpfft[]) {
  void mp_add(int n, int radix, int in1[], int in2[], int out[]);
  void mp_mul_i2d(int n, int radix, int nfft, int shift, int in[], double dout[]);
  void mp_mul_cmul_nt_d1(int nfft, double d1[], double d2[]);
  void mp_mul_d2i(int n, int radix, int nfft, double din[], int out[]);
  int n_h, shift;
  DGTINT *in2r;

  in2r = ((DGTINT *)&in2[2]) - 2;
  shift = (nfft >> 1) + 1;
  while (n > shift) {
    if (in2r[shift + 2] != 0) {
      break;
    }
    shift++;
  }
  n_h = n / 2 + 1;
  if (n_h < n - shift) {
    n_h = n - shift;
  }
  /* ---- tmp = (upper) in1fft * (upper) in2 ---- */
  mp_mul_i2d(n, radix, nfft, 0, in2, tmpfft);
  mp_mul_cmul_nt_d1(nfft, in1fft, tmpfft);
  mp_mul_d2i(n, radix, nfft, tmpfft, tmp);
  /* ---- out = tmp + (upper) in1 * (lower) in2 ---- */
  mp_mul_i2d(n, radix, nfft, shift, in2, tmpfft);
  mp_mul_cmul_nt_d1(nfft, in1fft, tmpfft);
  mp_mul_d2i(n_h, radix, nfft, tmpfft, out);
  mp_add(n, radix, out, tmp, out);
}

void mp_squhf_use_infft(int n, int radix, double infft[], int in[], int out[], int tmp[], int nfft, double tmpfft[]) {
  void mp_add(int n, int radix, int in1[], int in2[], int out[]);
  void mp_mul_i2d(int n, int radix, int nfft, int shift, int in[], double dout[]);
  void mp_mul_cmul_nt_d1(int nfft, double d1[], double d2[]);
  void mp_mul_csqu_nt_d1(int nfft, double d1[]);
  void mp_mul_d2i(int n, int radix, int nfft, double din[], int out[]);
  int n_h, shift;
  DGTINT *inr;

  inr = ((DGTINT *)&in[2]) - 2;
  shift = (nfft >> 1) + 1;
  while (n > shift) {
    if (inr[shift + 2] != 0) {
      break;
    }
    shift++;
  }
  n_h = n / 2 + 1;
  if (n_h < n - shift) {
    n_h = n - shift;
  }
  /* ---- tmp = (upper) infft * (lower) in ---- */
  mp_mul_i2d(n, radix, nfft, shift, in, tmpfft);
  mp_mul_cmul_nt_d1(nfft, infft, tmpfft);
  mp_mul_d2i(n_h, radix, nfft, tmpfft, tmp);
  /* ---- out = tmp + ((upper) infft)^2 ---- */
  mp_mul_csqu_nt_d1(nfft, infft);
  mp_mul_d2i(n, radix, nfft, infft, out);
  mp_add(n, radix, out, tmp, out);
}

void mp_mulh(int n, int radix, int in1[], int in2[], int out[], int nfft, double in1fft[], double outfft[]) {
  void mp_mul_i2d(int n, int radix, int nfft, int shift, int in[], double dout[]);
  void mp_mul_cmul(int nfft, double d1[], double d2[]);
  void mp_mul_d2i(int n, int radix, int nfft, double din[], int out[]);

  mp_mul_i2d(n, radix, nfft, 0, in1, in1fft);
  mp_mul_i2d(n, radix, nfft, 0, in2, outfft);
  mp_mul_cmul(nfft, in1fft, outfft);
  mp_mul_d2i(n, radix, nfft, outfft, out);
}

void mp_mulh_use_in1fft(int n, int radix, double in1fft[], int shift, int in2[], int out[], int nfft, double outfft[]) {
  void mp_mul_i2d(int n, int radix, int nfft, int shift, int in[], double dout[]);
  void mp_mul_cmul_nt_d1(int nfft, double d1[], double d2[]);
  void mp_mul_d2i(int n, int radix, int nfft, double din[], int out[]);
  DGTINT *in2r;

  in2r = ((DGTINT *)&in2[2]) - 2;
  while (n > shift) {
    if (in2r[shift + 2] != 0) {
      break;
    }
    shift++;
  }
  mp_mul_i2d(n, radix, nfft, shift, in2, outfft);
  mp_mul_cmul_nt_d1(nfft, in1fft, outfft);
  mp_mul_d2i(n, radix, nfft, outfft, out);
}

void mp_squh(int n, int radix, int in[], int out[], int nfft, double outfft[]) {
  void mp_mul_i2d(int n, int radix, int nfft, int shift, int in[], double dout[]);
  void mp_mul_csqu(int nfft, double d1[]);
  void mp_mul_d2i(int n, int radix, int nfft, double din[], int out[]);

  mp_mul_i2d(n, radix, nfft, 0, in, outfft);
  mp_mul_csqu(nfft, outfft);
  mp_mul_d2i(n, radix, nfft, outfft, out);
}

void mp_squh_save_infft(int n, int radix, int in[], int out[], int nfft, double infft[], double outfft[]) {
  void mp_mul_i2d(int n, int radix, int nfft, int shift, int in[], double dout[]);
  void mp_mul_csqu_save_d1(int nfft, double d1[], double d2[]);
  void mp_mul_d2i(int n, int radix, int nfft, double din[], int out[]);

  mp_mul_i2d(n, radix, nfft, 0, in, infft);
  mp_mul_csqu_save_d1(nfft, infft, outfft);
  mp_mul_d2i(n, radix, nfft, outfft, out);
}

void mp_squh_use_in1fft(int n, int radix, double inoutfft[], int out[], int nfft) {
  void mp_mul_csqu_nt_d1(int nfft, double d1[]);
  void mp_mul_d2i(int n, int radix, int nfft, double din[], int out[]);

  mp_mul_csqu_nt_d1(nfft, inoutfft);
  mp_mul_d2i(n, radix, nfft, inoutfft, out);
}

/* -------- mp_mul child routines -------- */

void mp_mul_i2d(int n, int radix, int nfft, int shift, int in[], double dout[]) {
  int j, x, carry, ndata, radix_2, topdgt;
  DGTINT *inr;

  inr = ((DGTINT *)&in[2]) - 2;
  ndata = 0;
  topdgt = 0;
  if (n > shift) {
    topdgt = inr[shift + 2];
    ndata = (nfft >> 1) + 1;
    if (ndata > n - shift) {
      ndata = n - shift;
    }
  }
  dout[nfft + 1] = in[0] * topdgt;
  for (j = nfft; j > ndata; j--) {
    dout[j] = 0;
  }
  /* ---- abs(dout[j]) <= radix/2 (to keep FFT precision) ---- */
  if (ndata > 1) {
    radix_2 = radix / 2;
    carry = 0;
    for (j = ndata + 1; j > 3; j--) {
      x = inr[j + shift] - carry;
      carry = x >= radix_2 ? -1 : 0;
      dout[j - 1] = x - (radix & carry);
    }
    dout[2] = inr[shift + 3] - carry;
  }
  dout[1] = topdgt;
  dout[0] = in[1] - shift;
}

void mp_mul_cmul(int nfft, double d1[], double d2[]) {
  void cdft(int n, int isgn, double *a);
  void mp_mul_rcmul(int n, double *a, double *b);
  double xr, xi;

  cdft(nfft, 1, &d1[1]);
  cdft(nfft, 1, &d2[1]);
  d2[0] += d1[0];
  xr = d1[1] * d2[1] + d1[2] * d2[2];
  xi = d1[1] * d2[2] + d1[2] * d2[1];
  d2[1] = xr;
  d2[2] = xi;
  if (nfft > 2) {
    mp_mul_rcmul(nfft, &d1[1], &d2[1]);
  }
  d2[nfft + 1] *= d1[nfft + 1];
  cdft(nfft, -1, &d2[1]);
}

void mp_mul_cmul_nt_d1(int nfft, double d1[], double d2[]) {
  void cdft(int n, int isgn, double *a);
  void mp_mul_rcmul_nt_in1(int n, double *a, double *b);
  double xr, xi;

  cdft(nfft, 1, &d2[1]);
  d2[0] += d1[0];
  xr = d1[1] * d2[1] + d1[2] * d2[2];
  xi = d1[1] * d2[2] + d1[2] * d2[1];
  d2[1] = xr;
  d2[2] = xi;
  if (nfft > 2) {
    mp_mul_rcmul_nt_in1(nfft, &d1[1], &d2[1]);
  }
  d2[nfft + 1] *= d1[nfft + 1];
  cdft(nfft, -1, &d2[1]);
}

void mp_mul_cmul_nt_d2(int nfft, double d1[], double d2[]) {
  void cdft(int n, int isgn, double *a);
  void mp_mul_rcmul_nt_in2(int n, double *a, double *b);
  double xr, xi;

  cdft(nfft, 1, &d1[1]);
  d2[0] += d1[0];
  xr = d1[1] * d2[1] + d1[2] * d2[2];
  xi = d1[1] * d2[2] + d1[2] * d2[1];
  d2[1] = xr;
  d2[2] = xi;
  if (nfft > 2) {
    mp_mul_rcmul_nt_in2(nfft, &d1[1], &d2[1]);
  }
  d2[nfft + 1] *= d1[nfft + 1];
  cdft(nfft, -1, &d2[1]);
}

void mp_mul_cmul_nt_out(int nfft, double d1[], double d2[]) {
  void cdft(int n, int isgn, double *a);
  void mp_mul_rcmul_nt_out(int n, double *a, double *b);
  double xr, xi;

  cdft(nfft, 1, &d1[1]);
  cdft(nfft, 1, &d2[1]);
  d2[0] += d1[0];
  xr = d1[1] * d2[1] + d1[2] * d2[2];
  xi = d1[1] * d2[2] + d1[2] * d2[1];
  d2[1] = xr;
  d2[2] = xi;
  if (nfft > 2) {
    mp_mul_rcmul_nt_out(nfft, &d1[1], &d2[1]);
  }
  d2[nfft + 1] *= d1[nfft + 1];
}

void mp_mul_cmul_nt_d1_add(int nfft, double d1[], double d2[], double d3[]) {
  void cdft(int n, int isgn, double *a);
  void mp_mul_rcmul_nt_in1_add(int n, double *a, double *b, double *badd);
  double xr, xi;

  cdft(nfft, 1, &d2[1]);
  xr = d1[1] * d2[1] + d1[2] * d2[2];
  xi = d1[1] * d2[2] + d1[2] * d2[1];
  d3[1] += xr;
  d3[2] += xi;
  if (nfft > 2) {
    mp_mul_rcmul_nt_in1_add(nfft, &d1[1], &d2[1], &d3[1]);
  }
  d3[nfft + 1] += d1[nfft + 1] * d2[nfft + 1];
  cdft(nfft, -1, &d3[1]);
}

void mp_mul_csqu(int nfft, double d1[]) {
  void cdft(int n, int isgn, double *a);
  void mp_mul_rcsqu(int n, double *a);
  double xr, xi;

  cdft(nfft, 1, &d1[1]);
  d1[0] *= 2;
  xr = d1[1] * d1[1] + d1[2] * d1[2];
  xi = 2 * d1[1] * d1[2];
  d1[1] = xr;
  d1[2] = xi;
  if (nfft > 2) {
    mp_mul_rcsqu(nfft, &d1[1]);
  }
  d1[nfft + 1] *= d1[nfft + 1];
  cdft(nfft, -1, &d1[1]);
}

void mp_mul_csqu_save_d1(int nfft, double d1[], double d2[]) {
  void cdft(int n, int isgn, double *a);
  void mp_mul_rcsqu_save(int n, double *a, double *b);
  double xr, xi;

  cdft(nfft, 1, &d1[1]);
  d2[0] = 2 * d1[0];
  xr = d1[1] * d1[1] + d1[2] * d1[2];
  xi = 2 * d1[1] * d1[2];
  d2[1] = xr;
  d2[2] = xi;
  if (nfft > 2) {
    mp_mul_rcsqu_save(nfft, &d1[1], &d2[1]);
  }
  d2[nfft + 1] = d1[nfft + 1] * d1[nfft + 1];
  cdft(nfft, -1, &d2[1]);
}

void mp_mul_csqu_nt_d1(int nfft, double d1[]) {
  void cdft(int n, int isgn, double *a);
  void mp_mul_rcsqu_nt_in(int n, double *a);
  double xr, xi;

  d1[0] *= 2;
  xr = d1[1] * d1[1] + d1[2] * d1[2];
  xi = 2 * d1[1] * d1[2];
  d1[1] = xr;
  d1[2] = xi;
  if (nfft > 2) {
    mp_mul_rcsqu_nt_in(nfft, &d1[1]);
  }
  d1[nfft + 1] *= d1[nfft + 1];
  cdft(nfft, -1, &d1[1]);
}

void mp_mul_d2i(int n, int radix, int nfft, double din[], int out[]) {
  int j, carry, carry1, carry2, shift, ndata;
  double x, scale, d1_radix, d1_radix2, pow_radix, topdgt;
  DGTINT *outr;

  outr = ((DGTINT *)&out[2]) - 2;
  scale = 2.0 / nfft;
  d1_radix = 1.0 / radix;
  d1_radix2 = d1_radix * d1_radix;
  topdgt = din[nfft + 1];
  x = topdgt < 0 ? -topdgt : topdgt;
  shift = x + 0.5 >= radix ? 1 : 0;
  /* ---- correction of cyclic convolution of din[1] ---- */
  x *= nfft * 0.5;
  din[nfft + 1] = din[1] - x;
  din[1] = x;
  /* ---- output of digits ---- */
  ndata = n;
  if (n > nfft + 1 + shift) {
    ndata = nfft + 1 + shift;
    for (j = n + 1; j > ndata + 1; j--) {
      outr[j] = 0;
    }
  }
  x = 0;
  pow_radix = 1;
  for (j = ndata + 1 - shift; j <= nfft + 1; j++) {
    x += pow_radix * din[j];
    pow_radix *= d1_radix;
    if (pow_radix < DBL_EPSILON) {
      break;
    }
  }
  x = d1_radix2 * (scale * x + 0.5);
  carry2 = ((int)x) - 1;
  carry = (int)(radix * (x - carry2) + 0.5);
  for (j = ndata; j > 1; j--) {
    x = d1_radix2 * (scale * din[j - shift] + carry + 0.5);
    carry = carry2;
    carry2 = ((int)x) - 1;
    x = radix * (x - carry2);
    carry1 = (int)x;
    outr[j + 1] = (DGTINT)(radix * (x - carry1));
    carry += carry1;
  }
  x = carry + ((double)radix) * carry2 + 0.5;
  if (shift == 0) {
    x += scale * din[1];
  }
  carry = (int)(d1_radix * x);
  outr[2] = (DGTINT)(x - ((double)radix) * carry);
  if (carry > 0) {
    for (j = n + 1; j > 2; j--) {
      outr[j] = outr[j - 1];
    }
    outr[2] = (DGTINT)carry;
    shift++;
  }
  /* ---- output of exp, sgn ---- */
  x = din[0] + shift + 0.5;
  shift = ((int)x) - 1;
  out[1] = shift + ((int)(x - shift));
  out[0] = topdgt > 0.5 ? 1 : -1;
  if (outr[2] == 0) {
    out[0] = 0;
    out[1] = 0;
  }
}

double mp_mul_d2i_test(int radix, int nfft, double din[]) {
  int j, carry, carry1, carry2;
  double x, scale, d1_radix, d1_radix2, err;

  scale = 2.0 / nfft;
  d1_radix = 1.0 / radix;
  d1_radix2 = d1_radix * d1_radix;
  /* ---- correction of cyclic convolution of din[1] ---- */
  x = din[nfft + 1] * nfft * 0.5;
  if (x < 0) {
    x = -x;
  }
  din[nfft + 1] = din[1] - x;
  /* ---- check of digits ---- */
  err = 0;
  carry = 0;
  carry2 = 0;
  for (j = nfft + 1; j > 1; j--) {
    x = d1_radix2 * (scale * din[j] + carry + 0.5);
    carry = carry2;
    carry2 = ((int)x) - 1;
    x = radix * (x - carry2);
    carry1 = (int)x;
    x = radix * (x - carry1);
    carry += carry1;
    x = x - 0.5 - ((int)x);
    if (x > err) {
      err = x;
    } else if (-x > err) {
      err = -x;
    }
  }
  return err;
}

/* -------- mp_mul child^2 routines (mix RFFT routines) -------- */

#ifndef M_PI_2
#define M_PI_2 1.570796326794896619231321691639751442098584699687
#endif

#ifndef RDFT_LOOP_DIV /* control of the RDFT's speed & tolerance */
#define RDFT_LOOP_DIV 64
#endif

void mp_mul_rcmul(int n, double *a, double *b) {
  int i, i0, j, k;
  double ec, w1r, w1i, wkr, wki, wdr, wdi, ss;
  double xr, xi, yr, yi, ajr, aji, akr, aki, bjr, bji, bkr, bki;

  ec = 2 * M_PI_2 / n;
  wkr = 0;
  wki = 0;
  wdi = cos(ec);
  wdr = sin(ec);
  wdi *= wdr;
  wdr *= wdr;
  w1r = 1 - 2 * wdr;
  w1i = 2 * wdi;
  ss = 2 * w1i;
  i = n >> 1;
  xr = a[i];
  xi = a[i + 1];
  yr = b[i];
  yi = b[i + 1];
  b[i] = xr * yr - xi * yi;
  b[i + 1] = xr * yi + xi * yr;
  for (;;) {
    i0 = i - 4 * RDFT_LOOP_DIV;
    if (i0 < 2) {
      i0 = 2;
    }
    for (j = i - 2; j >= i0; j -= 2) {
      k = n - j;
      xr = wkr + ss * wdi;
      xi = wki + ss * (0.5 - wdr);
      wkr = wdr;
      wki = wdi;
      wdr = xr;
      wdi = xi;
      /* ---- transform CFFT data a[] into RFFT data ---- */
      xr = a[j] - a[k];
      xi = a[j + 1] + a[k + 1];
      yr = wkr * xr - wki * xi;
      yi = wkr * xi + wki * xr;
      ajr = a[j] - yr;
      aji = a[j + 1] - yi;
      akr = a[k] + yr;
      aki = a[k + 1] - yi;
      a[j] = ajr;
      a[j + 1] = aji;
      a[k] = akr;
      a[k + 1] = aki;
      /* ---- transform CFFT data b[] into RFFT data ---- */
      xr = b[j] - b[k];
      xi = b[j + 1] + b[k + 1];
      yr = wkr * xr - wki * xi;
      yi = wkr * xi + wki * xr;
      xr = b[j] - yr;
      xi = b[j + 1] - yi;
      yr = b[k] + yr;
      yi = b[k + 1] - yi;
      /* ---- cmul ---- */
      bjr = ajr * xr - aji * xi;
      bji = ajr * xi + aji * xr;
      bkr = akr * yr - aki * yi;
      bki = akr * yi + aki * yr;
      /* ---- transform RFFT data bxx into CFFT data ---- */
      xr = bjr - bkr;
      xi = bji + bki;
      yr = wkr * xr + wki * xi;
      yi = wkr * xi - wki * xr;
      b[j] = bjr - yr;
      b[j + 1] = bji - yi;
      b[k] = bkr + yr;
      b[k + 1] = bki - yi;
    }
    if (i0 == 2) {
      break;
    }
    wkr = 0.5 * sin(ec * i0);
    wki = 0.5 * cos(ec * i0);
    wdr = 0.5 - (wkr * w1r - wki * w1i);
    wdi = wkr * w1i + wki * w1r;
    wkr = 0.5 - wkr;
    i = i0;
  }
}

void mp_mul_rcmul_nt_in1(int n, double *a, double *b) {
  int i, i0, j, k;
  double ec, w1r, w1i, wkr, wki, wdr, wdi, ss;
  double xr, xi, yr, yi, bjr, bji, bkr, bki;

  ec = 2 * M_PI_2 / n;
  wkr = 0;
  wki = 0;
  wdi = cos(ec);
  wdr = sin(ec);
  wdi *= wdr;
  wdr *= wdr;
  w1r = 1 - 2 * wdr;
  w1i = 2 * wdi;
  ss = 2 * w1i;
  i = n >> 1;
  xr = a[i];
  xi = a[i + 1];
  yr = b[i];
  yi = b[i + 1];
  b[i] = xr * yr - xi * yi;
  b[i + 1] = xr * yi + xi * yr;
  for (;;) {
    i0 = i - 4 * RDFT_LOOP_DIV;
    if (i0 < 2) {
      i0 = 2;
    }
    for (j = i - 2; j >= i0; j -= 2) {
      k = n - j;
      xr = wkr + ss * wdi;
      xi = wki + ss * (0.5 - wdr);
      wkr = wdr;
      wki = wdi;
      wdr = xr;
      wdi = xi;
      /* ---- transform CFFT data b[] into RFFT data ---- */
      xr = b[j] - b[k];
      xi = b[j + 1] + b[k + 1];
      yr = wkr * xr - wki * xi;
      yi = wkr * xi + wki * xr;
      xr = b[j] - yr;
      xi = b[j + 1] - yi;
      yr = b[k] + yr;
      yi = b[k + 1] - yi;
      /* ---- cmul ---- */
      bjr = a[j] * xr - a[j + 1] * xi;
      bji = a[j] * xi + a[j + 1] * xr;
      bkr = a[k] * yr - a[k + 1] * yi;
      bki = a[k] * yi + a[k + 1] * yr;
      /* ---- transform RFFT data bxx into CFFT data ---- */
      xr = bjr - bkr;
      xi = bji + bki;
      yr = wkr * xr + wki * xi;
      yi = wkr * xi - wki * xr;
      b[j] = bjr - yr;
      b[j + 1] = bji - yi;
      b[k] = bkr + yr;
      b[k + 1] = bki - yi;
    }
    if (i0 == 2) {
      break;
    }
    wkr = 0.5 * sin(ec * i0);
    wki = 0.5 * cos(ec * i0);
    wdr = 0.5 - (wkr * w1r - wki * w1i);
    wdi = wkr * w1i + wki * w1r;
    wkr = 0.5 - wkr;
    i = i0;
  }
}

void mp_mul_rcmul_nt_in2(int n, double *a, double *b) {
  int i, i0, j, k;
  double ec, w1r, w1i, wkr, wki, wdr, wdi, ss;
  double xr, xi, yr, yi, bjr, bji, bkr, bki;

  ec = 2 * M_PI_2 / n;
  wkr = 0;
  wki = 0;
  wdi = cos(ec);
  wdr = sin(ec);
  wdi *= wdr;
  wdr *= wdr;
  w1r = 1 - 2 * wdr;
  w1i = 2 * wdi;
  ss = 2 * w1i;
  i = n >> 1;
  xr = a[i];
  xi = a[i + 1];
  yr = b[i];
  yi = b[i + 1];
  b[i] = xr * yr - xi * yi;
  b[i + 1] = xr * yi + xi * yr;
  for (;;) {
    i0 = i - 4 * RDFT_LOOP_DIV;
    if (i0 < 2) {
      i0 = 2;
    }
    for (j = i - 2; j >= i0; j -= 2) {
      k = n - j;
      xr = wkr + ss * wdi;
      xi = wki + ss * (0.5 - wdr);
      wkr = wdr;
      wki = wdi;
      wdr = xr;
      wdi = xi;
      /* ---- transform CFFT data a[] into RFFT data ---- */
      xr = a[j] - a[k];
      xi = a[j + 1] + a[k + 1];
      yr = wkr * xr - wki * xi;
      yi = wkr * xi + wki * xr;
      xr = a[j] - yr;
      xi = a[j + 1] - yi;
      yr = a[k] + yr;
      yi = a[k + 1] - yi;
      a[j] = xr;
      a[j + 1] = xi;
      a[k] = yr;
      a[k + 1] = yi;
      /* ---- cmul ---- */
      bjr = b[j] * xr - b[j + 1] * xi;
      bji = b[j] * xi + b[j + 1] * xr;
      bkr = b[k] * yr - b[k + 1] * yi;
      bki = b[k] * yi + b[k + 1] * yr;
      /* ---- transform RFFT data bxx into CFFT data ---- */
      xr = bjr - bkr;
      xi = bji + bki;
      yr = wkr * xr + wki * xi;
      yi = wkr * xi - wki * xr;
      b[j] = bjr - yr;
      b[j + 1] = bji - yi;
      b[k] = bkr + yr;
      b[k + 1] = bki - yi;
    }
    if (i0 == 2) {
      break;
    }
    wkr = 0.5 * sin(ec * i0);
    wki = 0.5 * cos(ec * i0);
    wdr = 0.5 - (wkr * w1r - wki * w1i);
    wdi = wkr * w1i + wki * w1r;
    wkr = 0.5 - wkr;
    i = i0;
  }
}

void mp_mul_rcmul_nt_out(int n, double *a, double *b) {
  int i, i0, j, k;
  double ec, w1r, w1i, wkr, wki, wdr, wdi, ss;
  double xr, xi, yr, yi, ajr, aji, akr, aki;

  ec = 2 * M_PI_2 / n;
  wkr = 0;
  wki = 0;
  wdi = cos(ec);
  wdr = sin(ec);
  wdi *= wdr;
  wdr *= wdr;
  w1r = 1 - 2 * wdr;
  w1i = 2 * wdi;
  ss = 2 * w1i;
  i = n >> 1;
  xr = a[i];
  xi = a[i + 1];
  yr = b[i];
  yi = b[i + 1];
  b[i] = xr * yr - xi * yi;
  b[i + 1] = xr * yi + xi * yr;
  for (;;) {
    i0 = i - 4 * RDFT_LOOP_DIV;
    if (i0 < 2) {
      i0 = 2;
    }
    for (j = i - 2; j >= i0; j -= 2) {
      k = n - j;
      xr = wkr + ss * wdi;
      xi = wki + ss * (0.5 - wdr);
      wkr = wdr;
      wki = wdi;
      wdr = xr;
      wdi = xi;
      /* ---- transform CFFT data a[] into RFFT data ---- */
      xr = a[j] - a[k];
      xi = a[j + 1] + a[k + 1];
      yr = wkr * xr - wki * xi;
      yi = wkr * xi + wki * xr;
      ajr = a[j] - yr;
      aji = a[j + 1] - yi;
      akr = a[k] + yr;
      aki = a[k + 1] - yi;
      a[j] = ajr;
      a[j + 1] = aji;
      a[k] = akr;
      a[k + 1] = aki;
      /* ---- transform CFFT data b[] into RFFT data ---- */
      xr = b[j] - b[k];
      xi = b[j + 1] + b[k + 1];
      yr = wkr * xr - wki * xi;
      yi = wkr * xi + wki * xr;
      xr = b[j] - yr;
      xi = b[j + 1] - yi;
      yr = b[k] + yr;
      yi = b[k + 1] - yi;
      /* ---- cmul ---- */
      b[j] = ajr * xr - aji * xi;
      b[j + 1] = ajr * xi + aji * xr;
      b[k] = akr * yr - aki * yi;
      b[k + 1] = akr * yi + aki * yr;
    }
    if (i0 == 2) {
      break;
    }
    wkr = 0.5 * sin(ec * i0);
    wki = 0.5 * cos(ec * i0);
    wdr = 0.5 - (wkr * w1r - wki * w1i);
    wdi = wkr * w1i + wki * w1r;
    wkr = 0.5 - wkr;
    i = i0;
  }
}

void mp_mul_rcmul_nt_in1_add(int n, double *a, double *b, double *badd) {
  int i, i0, j, k;
  double ec, w1r, w1i, wkr, wki, wdr, wdi, ss;
  double xr, xi, yr, yi, bjr, bji, bkr, bki;

  ec = 2 * M_PI_2 / n;
  wkr = 0;
  wki = 0;
  wdi = cos(ec);
  wdr = sin(ec);
  wdi *= wdr;
  wdr *= wdr;
  w1r = 1 - 2 * wdr;
  w1i = 2 * wdi;
  ss = 2 * w1i;
  i = n >> 1;
  xr = a[i];
  xi = a[i + 1];
  yr = b[i];
  yi = b[i + 1];
  badd[i] += xr * yr - xi * yi;
  badd[i + 1] += xr * yi + xi * yr;
  for (;;) {
    i0 = i - 4 * RDFT_LOOP_DIV;
    if (i0 < 2) {
      i0 = 2;
    }
    for (j = i - 2; j >= i0; j -= 2) {
      k = n - j;
      xr = wkr + ss * wdi;
      xi = wki + ss * (0.5 - wdr);
      wkr = wdr;
      wki = wdi;
      wdr = xr;
      wdi = xi;
      /* ---- transform CFFT data b[] into RFFT data ---- */
      xr = b[j] - b[k];
      xi = b[j + 1] + b[k + 1];
      yr = wkr * xr - wki * xi;
      yi = wkr * xi + wki * xr;
      xr = b[j] - yr;
      xi = b[j + 1] - yi;
      yr = b[k] + yr;
      yi = b[k + 1] - yi;
      /* ---- cmul + add ---- */
      bjr = badd[j] + (a[j] * xr - a[j + 1] * xi);
      bji = badd[j + 1] + (a[j] * xi + a[j + 1] * xr);
      bkr = badd[k] + (a[k] * yr - a[k + 1] * yi);
      bki = badd[k + 1] + (a[k] * yi + a[k + 1] * yr);
      /* ---- transform RFFT data bxx into CFFT data ---- */
      xr = bjr - bkr;
      xi = bji + bki;
      yr = wkr * xr + wki * xi;
      yi = wkr * xi - wki * xr;
      badd[j] = bjr - yr;
      badd[j + 1] = bji - yi;
      badd[k] = bkr + yr;
      badd[k + 1] = bki - yi;
    }
    if (i0 == 2) {
      break;
    }
    wkr = 0.5 * sin(ec * i0);
    wki = 0.5 * cos(ec * i0);
    wdr = 0.5 - (wkr * w1r - wki * w1i);
    wdi = wkr * w1i + wki * w1r;
    wkr = 0.5 - wkr;
    i = i0;
  }
}

void mp_mul_rcsqu(int n, double *a) {
  int i, i0, j, k;
  double ec, w1r, w1i, wkr, wki, wdr, wdi, ss;
  double xr, xi, yr, yi, ajr, aji, akr, aki;

  ec = 2 * M_PI_2 / n;
  wkr = 0;
  wki = 0;
  wdi = cos(ec);
  wdr = sin(ec);
  wdi *= wdr;
  wdr *= wdr;
  w1r = 1 - 2 * wdr;
  w1i = 2 * wdi;
  ss = 2 * w1i;
  i = n >> 1;
  xr = a[i];
  xi = a[i + 1];
  a[i] = xr * xr - xi * xi;
  a[i + 1] = 2 * xr * xi;
  for (;;) {
    i0 = i - 4 * RDFT_LOOP_DIV;
    if (i0 < 2) {
      i0 = 2;
    }
    for (j = i - 2; j >= i0; j -= 2) {
      k = n - j;
      xr = wkr + ss * wdi;
      xi = wki + ss * (0.5 - wdr);
      wkr = wdr;
      wki = wdi;
      wdr = xr;
      wdi = xi;
      /* ---- transform CFFT data a[] into RFFT data ---- */
      xr = a[j] - a[k];
      xi = a[j + 1] + a[k + 1];
      yr = wkr * xr - wki * xi;
      yi = wkr * xi + wki * xr;
      xr = a[j] - yr;
      xi = a[j + 1] - yi;
      yr = a[k] + yr;
      yi = a[k + 1] - yi;
      /* ---- csqu ---- */
      ajr = xr * xr - xi * xi;
      aji = 2 * xr * xi;
      akr = yr * yr - yi * yi;
      aki = 2 * yr * yi;
      /* ---- transform RFFT data axx into CFFT data ---- */
      xr = ajr - akr;
      xi = aji + aki;
      yr = wkr * xr + wki * xi;
      yi = wkr * xi - wki * xr;
      a[j] = ajr - yr;
      a[j + 1] = aji - yi;
      a[k] = akr + yr;
      a[k + 1] = aki - yi;
    }
    if (i0 == 2) {
      break;
    }
    wkr = 0.5 * sin(ec * i0);
    wki = 0.5 * cos(ec * i0);
    wdr = 0.5 - (wkr * w1r - wki * w1i);
    wdi = wkr * w1i + wki * w1r;
    wkr = 0.5 - wkr;
    i = i0;
  }
}

void mp_mul_rcsqu_save(int n, double *a, double *b) {
  int i, i0, j, k;
  double ec, w1r, w1i, wkr, wki, wdr, wdi, ss;
  double xr, xi, yr, yi, ajr, aji, akr, aki;

  ec = 2 * M_PI_2 / n;
  wkr = 0;
  wki = 0;
  wdi = cos(ec);
  wdr = sin(ec);
  wdi *= wdr;
  wdr *= wdr;
  w1r = 1 - 2 * wdr;
  w1i = 2 * wdi;
  ss = 2 * w1i;
  i = n >> 1;
  xr = a[i];
  xi = a[i + 1];
  b[i] = xr * xr - xi * xi;
  b[i + 1] = 2 * xr * xi;
  for (;;) {
    i0 = i - 4 * RDFT_LOOP_DIV;
    if (i0 < 2) {
      i0 = 2;
    }
    for (j = i - 2; j >= i0; j -= 2) {
      k = n - j;
      xr = wkr + ss * wdi;
      xi = wki + ss * (0.5 - wdr);
      wkr = wdr;
      wki = wdi;
      wdr = xr;
      wdi = xi;
      /* ---- transform CFFT data a[] into RFFT data ---- */
      xr = a[j] - a[k];
      xi = a[j + 1] + a[k + 1];
      yr = wkr * xr - wki * xi;
      yi = wkr * xi + wki * xr;
      xr = a[j] - yr;
      xi = a[j + 1] - yi;
      yr = a[k] + yr;
      yi = a[k + 1] - yi;
      a[j] = xr;
      a[j + 1] = xi;
      a[k] = yr;
      a[k + 1] = yi;
      /* ---- csqu ---- */
      ajr = xr * xr - xi * xi;
      aji = 2 * xr * xi;
      akr = yr * yr - yi * yi;
      aki = 2 * yr * yi;
      /* ---- transform RFFT data axx into CFFT data ---- */
      xr = ajr - akr;
      xi = aji + aki;
      yr = wkr * xr + wki * xi;
      yi = wkr * xi - wki * xr;
      b[j] = ajr - yr;
      b[j + 1] = aji - yi;
      b[k] = akr + yr;
      b[k + 1] = aki - yi;
    }
    if (i0 == 2) {
      break;
    }
    wkr = 0.5 * sin(ec * i0);
    wki = 0.5 * cos(ec * i0);
    wdr = 0.5 - (wkr * w1r - wki * w1i);
    wdi = wkr * w1i + wki * w1r;
    wkr = 0.5 - wkr;
    i = i0;
  }
}

void mp_mul_rcsqu_nt_in(int n, double *a) {
  int i, i0, j, k;
  double ec, w1r, w1i, wkr, wki, wdr, wdi, ss;
  double xr, xi, yr, yi, ajr, aji, akr, aki;

  ec = 2 * M_PI_2 / n;
  wkr = 0;
  wki = 0;
  wdi = cos(ec);
  wdr = sin(ec);
  wdi *= wdr;
  wdr *= wdr;
  w1r = 1 - 2 * wdr;
  w1i = 2 * wdi;
  ss = 2 * w1i;
  i = n >> 1;
  xr = a[i];
  xi = a[i + 1];
  a[i] = xr * xr - xi * xi;
  a[i + 1] = 2 * xr * xi;
  for (;;) {
    i0 = i - 4 * RDFT_LOOP_DIV;
    if (i0 < 2) {
      i0 = 2;
    }
    for (j = i - 2; j >= i0; j -= 2) {
      k = n - j;
      xr = wkr + ss * wdi;
      xi = wki + ss * (0.5 - wdr);
      wkr = wdr;
      wki = wdi;
      wdr = xr;
      wdi = xi;
      /* ---- csqu ---- */
      xr = a[j];
      xi = a[j + 1];
      yr = a[k];
      yi = a[k + 1];
      ajr = xr * xr - xi * xi;
      aji = 2 * xr * xi;
      akr = yr * yr - yi * yi;
      aki = 2 * yr * yi;
      /* ---- transform RFFT data axx into CFFT data ---- */
      xr = ajr - akr;
      xi = aji + aki;
      yr = wkr * xr + wki * xi;
      yi = wkr * xi - wki * xr;
      a[j] = ajr - yr;
      a[j + 1] = aji - yi;
      a[k] = akr + yr;
      a[k + 1] = aki - yi;
    }
    if (i0 == 2) {
      break;
    }
    wkr = 0.5 * sin(ec * i0);
    wki = 0.5 * cos(ec * i0);
    wdr = 0.5 - (wkr * w1r - wki * w1i);
    wdi = wkr * w1i + wki * w1r;
    wkr = 0.5 - wkr;
    i = i0;
  }
}

/* -------- mp_inv routines -------- */

int mp_inv(int n, int radix, int in[], int out[], int tmp1[], int tmp2[], int nfft, double tmp1fft[], double tmp2fft[]) {
  int mp_get_nfft_init(int radix, int nfft_max);
  void mp_inv_init(int n, int radix, int in[], int out[]);
  int mp_inv_newton(int n, int radix, int in[], int inout[], int tmp1[], int tmp2[], int nfft, double tmp1fft[], double tmp2fft[]);
  int n_nwt, nfft_nwt, thr, prc;

  if (in[0] == 0) {
    return -1;
  }
  nfft_nwt = mp_get_nfft_init(radix, nfft);
  n_nwt = nfft_nwt + 2;
  if (n_nwt > n) {
    n_nwt = n;
  }
  mp_inv_init(n_nwt, radix, in, out);
  thr = 8;
  do {
    n_nwt = nfft_nwt + 2;
    if (n_nwt > n) {
      n_nwt = n;
    }
    prc = mp_inv_newton(n_nwt, radix, in, out, tmp1, tmp2, nfft_nwt, tmp1fft, tmp2fft);
#ifdef DEBUG
    printf("n=%d, nfft=%d, prc=%d\n", n_nwt, nfft_nwt, prc);
#endif
    if (thr * nfft_nwt >= nfft) {
      thr = 0;
      if (2 * prc <= n_nwt - 2) {
        nfft_nwt >>= 1;
      }
    } else {
      if (3 * prc < n_nwt - 2) {
        nfft_nwt >>= 1;
      }
    }
    nfft_nwt <<= 1;
  } while (nfft_nwt <= nfft);
  return 0;
}

int mp_sqrt(int n, int radix, int in[], int out[], int tmp1[], int tmp2[], int nfft, double tmp1fft[], double tmp2fft[]) {
  void mp_load_0(int n, int radix, int out[]);
  int mp_get_nfft_init(int radix, int nfft_max);
  void mp_sqrt_init(int n, int radix, int in[], int out[], int out_rev[]);
  int mp_sqrt_newton(int n, int radix, int in[], int inout[], int inout_rev[], int tmp[], int nfft, double tmp1fft[], double tmp2fft[], int *n_tmp1fft);
  int n_nwt, nfft_nwt, thr, prc, n_tmp1fft;

  if (in[0] < 0) {
    return -1;
  } else if (in[0] == 0) {
    mp_load_0(n, radix, out);
    return 0;
  }
  nfft_nwt = mp_get_nfft_init(radix, nfft);
  n_nwt = nfft_nwt + 2;
  if (n_nwt > n) {
    n_nwt = n;
  }
  mp_sqrt_init(n_nwt, radix, in, out, tmp1);
  n_tmp1fft = 0;
  thr = 8;
  do {
    n_nwt = nfft_nwt + 2;
    if (n_nwt > n) {
      n_nwt = n;
    }
    prc = mp_sqrt_newton(n_nwt, radix, in, out, tmp1, tmp2, nfft_nwt, tmp1fft, tmp2fft, &n_tmp1fft);
#ifdef DEBUG
    printf("n=%d, nfft=%d, prc=%d\n", n_nwt, nfft_nwt, prc);
#endif
    if (thr * nfft_nwt >= nfft) {
      thr = 0;
      if (2 * prc <= n_nwt - 2) {
        nfft_nwt >>= 1;
      }
    } else {
      if (3 * prc < n_nwt - 2) {
        nfft_nwt >>= 1;
      }
    }
    nfft_nwt <<= 1;
  } while (nfft_nwt <= nfft);
  return 0;
}

int mp_invisqrt(int n, int radix, int in, int out[], int tmp1[], int tmp2[], int nfft, double tmp1fft[], double tmp2fft[]) {
  int mp_get_nfft_init(int radix, int nfft_max);
  void mp_invisqrt_init(int n, int radix, int in, int out[]);
  int mp_invisqrt_newton(int n, int radix, int in, int inout[], int tmp1[], int tmp2[], int nfft, double tmp1fft[], double tmp2fft[]);
  int n_nwt, nfft_nwt, thr, prc;

  if (in <= 0) {
    return -1;
  }
  nfft_nwt = mp_get_nfft_init(radix, nfft);
  n_nwt = nfft_nwt + 2;
  if (n_nwt > n) {
    n_nwt = n;
  }
  mp_invisqrt_init(n_nwt, radix, in, out);
  thr = 8;
  do {
    n_nwt = nfft_nwt + 2;
    if (n_nwt > n) {
      n_nwt = n;
    }
    prc = mp_invisqrt_newton(n_nwt, radix, in, out, tmp1, tmp2, nfft_nwt, tmp1fft, tmp2fft);
#ifdef DEBUG
    printf("n=%d, nfft=%d, prc=%d\n", n_nwt, nfft_nwt, prc);
#endif
    if (thr * nfft_nwt >= nfft) {
      thr = 0;
      if (2 * prc <= n_nwt - 2) {
        nfft_nwt >>= 1;
      }
    } else {
      if (3 * prc < n_nwt - 2) {
        nfft_nwt >>= 1;
      }
    }
    nfft_nwt <<= 1;
  } while (nfft_nwt <= nfft);
  return 0;
}

/* -------- mp_inv child routines -------- */

int mp_get_nfft_init(int radix, int nfft_max) {
  int nfft_init;
  double r;

  r = radix;
  nfft_init = 1;
  do {
    r *= r;
    nfft_init <<= 1;
  } while (DBL_EPSILON * r < 1 && nfft_init < nfft_max);
  return nfft_init;
}

void mp_inv_init(int n, int radix, int in[], int out[]) {
  void mp_unexp_d2mp(int n, int radix, double din, DGTINT out[]);
  double mp_unexp_mp2d(int n, int radix, DGTINT in[]);
  int outexp;
  double din;

  out[0] = in[0];
  outexp = -in[1];
  din = 1.0 / mp_unexp_mp2d(n, radix, (DGTINT *)&in[2]);
  while (din < 1) {
    din *= radix;
    outexp--;
  }
  out[1] = outexp;
  mp_unexp_d2mp(n, radix, din, (DGTINT *)&out[2]);
}

void mp_sqrt_init(int n, int radix, int in[], int out[], int out_rev[]) {
  void mp_unexp_d2mp(int n, int radix, double din, DGTINT out[]);
  double mp_unexp_mp2d(int n, int radix, DGTINT in[]);
  int outexp;
  double din;

  out[0] = 1;
  out_rev[0] = 1;
  outexp = in[1];
  din = mp_unexp_mp2d(n, radix, (DGTINT *)&in[2]);
  if (outexp % 2 != 0) {
    din *= radix;
    outexp--;
  }
  outexp /= 2;
  din = sqrt(din);
  if (din < 1) {
    din *= radix;
    outexp--;
  }
  out[1] = outexp;
  mp_unexp_d2mp(n, radix, din, (DGTINT *)&out[2]);
  outexp = -outexp;
  din = 1.0 / din;
  while (din < 1) {
    din *= radix;
    outexp--;
  }
  out_rev[1] = outexp;
  mp_unexp_d2mp(n, radix, din, (DGTINT *)&out_rev[2]);
}

void mp_invisqrt_init(int n, int radix, int in, int out[]) {
  void mp_unexp_d2mp(int n, int radix, double din, DGTINT out[]);
  int outexp;
  double dout;

  out[0] = 1;
  outexp = 0;
  dout = sqrt(1.0 / in);
  while (dout < 1) {
    dout *= radix;
    outexp--;
  }
  out[1] = outexp;
  mp_unexp_d2mp(n, radix, dout, (DGTINT *)&out[2]);
}

void mp_unexp_d2mp(int n, int radix, double din, DGTINT out[]) {
  int j, x;

  for (j = 0; j < n; j++) {
    x = (int)din;
    if (x >= radix) {
      x = radix - 1;
      din = radix;
    }
    din = radix * (din - x);
    out[j] = (DGTINT)x;
  }
}

double mp_unexp_mp2d(int n, int radix, DGTINT in[]) {
  int j;
  double d1_radix, dout;

  d1_radix = 1.0 / radix;
  dout = 0;
  for (j = n - 1; j >= 0; j--) {
    dout = d1_radix * dout + in[j];
  }
  return dout;
}

int mp_inv_newton(int n, int radix, int in[], int inout[], int tmp1[], int tmp2[], int nfft, double tmp1fft[], double tmp2fft[]) {
  void mp_load_1(int n, int radix, int out[]);
  void mp_round(int n, int radix, int m, int inout[]);
  void mp_add(int n, int radix, int in1[], int in2[], int out[]);
  void mp_sub(int n, int radix, int in1[], int in2[], int out[]);
  void mp_mulh(int n, int radix, int in1[], int in2[], int out[], int nfft, double in1fft[], double outfft[]);
  void mp_mulh_use_in1fft(int n, int radix, double in1fft[], int shift, int in2[], int out[], int nfft, double outfft[]);
  int n_h, shift, prc;

  shift = (nfft >> 1) + 1;
  n_h = n / 2 + 1;
  if (n_h < n - shift) {
    n_h = n - shift;
  }
  /* ---- tmp1 = inout * (upper) in (half to normal precision) ---- */
  mp_round(n, radix, shift, inout);
  mp_mulh(n, radix, inout, in, tmp1, nfft, tmp1fft, tmp2fft);
  /* ---- tmp2 = 1 - tmp1 ---- */
  mp_load_1(n, radix, tmp2);
  mp_sub(n, radix, tmp2, tmp1, tmp2);
  /* ---- tmp2 -= inout * (lower) in (half precision) ---- */
  mp_mulh_use_in1fft(n, radix, tmp1fft, shift, in, tmp1, nfft, tmp2fft);
  mp_sub(n_h, radix, tmp2, tmp1, tmp2);
  /* ---- get precision ---- */
  prc = -tmp2[1];
  if (tmp2[0] == 0) {
    prc = nfft + 1;
  }
  /* ---- tmp2 *= inout (half precision) ---- */
  mp_mulh_use_in1fft(n_h, radix, tmp1fft, 0, tmp2, tmp2, nfft, tmp2fft);
  /* ---- inout += tmp2 ---- */
  mp_add(n, radix, inout, tmp2, inout);
  return prc;
}

int mp_sqrt_newton(int n, int radix, int in[], int inout[], int inout_rev[], int tmp[], int nfft, double tmp1fft[], double tmp2fft[], int *n_tmp1fft) {
  void mp_round(int n, int radix, int m, int inout[]);
  void mp_add(int n, int radix, int in1[], int in2[], int out[]);
  void mp_sub(int n, int radix, int in1[], int in2[], int out[]);
  void mp_idiv_2(int n, int radix, int in[], int out[]);
  void mp_mulh(int n, int radix, int in1[], int in2[], int out[], int nfft, double in1fft[], double outfft[]);
  void mp_squh(int n, int radix, int in[], int out[], int nfft, double outfft[]);
  void mp_squh_use_in1fft(int n, int radix, double inoutfft[], int out[], int nfft);
  int n_h, nfft_h, shift, prc;

  nfft_h = nfft >> 1;
  shift = nfft_h + 1;
  if (nfft_h < 2) {
    nfft_h = 2;
  }
  n_h = n / 2 + 1;
  if (n_h < n - shift) {
    n_h = n - shift;
  }
  /* ---- tmp = inout_rev^2 (1/4 to half precision) ---- */
  mp_round(n_h, radix, (nfft_h >> 1) + 1, inout_rev);
  if (*n_tmp1fft != nfft_h) {
    mp_squh(n_h, radix, inout_rev, tmp, nfft_h, tmp1fft);
  } else {
    mp_squh_use_in1fft(n_h, radix, tmp1fft, tmp, nfft_h);
  }
  /* ---- tmp = inout_rev - inout * tmp (half precision) ---- */
  mp_round(n, radix, shift, inout);
  mp_mulh(n_h, radix, inout, tmp, tmp, nfft, tmp1fft, tmp2fft);
  mp_sub(n_h, radix, inout_rev, tmp, tmp);
  /* ---- inout_rev += tmp ---- */
  mp_add(n_h, radix, inout_rev, tmp, inout_rev);
  /* ---- tmp = in - inout^2 (half to normal precision) ---- */
  mp_squh_use_in1fft(n, radix, tmp1fft, tmp, nfft);
  mp_sub(n, radix, in, tmp, tmp);
  /* ---- get precision ---- */
  prc = in[1] - tmp[1];
  if (((DGTINT *)&in[2])[0] > ((DGTINT *)&tmp[2])[0]) {
    prc++;
  }
  if (tmp[0] == 0) {
    prc = nfft + 1;
  }
  /* ---- tmp = tmp * inout_rev / 2 (half precision) ---- */
  mp_round(n_h, radix, shift, inout_rev);
  mp_mulh(n_h, radix, inout_rev, tmp, tmp, nfft, tmp1fft, tmp2fft);
  *n_tmp1fft = nfft;
  mp_idiv_2(n_h, radix, tmp, tmp);
  /* ---- inout += tmp ---- */
  mp_add(n, radix, inout, tmp, inout);
  return prc;
}

int mp_invisqrt_newton(int n, int radix, int in, int inout[], int tmp1[], int tmp2[], int nfft, double tmp1fft[], double tmp2fft[]) {
  void mp_load_1(int n, int radix, int out[]);
  void mp_round(int n, int radix, int m, int inout[]);
  void mp_add(int n, int radix, int in1[], int in2[], int out[]);
  void mp_sub(int n, int radix, int in1[], int in2[], int out[]);
  void mp_imul(int n, int radix, int in1[], int in2, int out[]);
  void mp_idiv_2(int n, int radix, int in[], int out[]);
  void mp_squh_save_infft(int n, int radix, int in[], int out[], int nfft, double infft[], double outfft[]);
  void mp_mulh_use_in1fft(int n, int radix, double in1fft[], int shift, int in2[], int out[], int nfft, double outfft[]);
  int n_h, shift, prc;

  shift = (nfft >> 1) + 1;
  n_h = n / 2 + 1;
  if (n_h < n - shift) {
    n_h = n - shift;
  }
  /* ---- tmp1 = in * inout^2 (half to normal precision) ---- */
  mp_round(n, radix, shift, inout);
  mp_squh_save_infft(n, radix, inout, tmp1, nfft, tmp1fft, tmp2fft);
  mp_imul(n, radix, tmp1, in, tmp1);
  /* ---- tmp2 = 1 - tmp1 ---- */
  mp_load_1(n, radix, tmp2);
  mp_sub(n, radix, tmp2, tmp1, tmp2);
  /* ---- get precision ---- */
  prc = -tmp2[1];
  if (tmp2[0] == 0) {
    prc = nfft + 1;
  }
  /* ---- tmp2 *= inout / 2 (half precision) ---- */
  mp_mulh_use_in1fft(n_h, radix, tmp1fft, 0, tmp2, tmp2, nfft, tmp2fft);
  mp_idiv_2(n_h, radix, tmp2, tmp2);
  /* ---- inout += tmp2 ---- */
  mp_add(n, radix, inout, tmp2, inout);
  return prc;
}

/* -------- mp_io routines -------- */

void mp_sprintf(int n, int log10_radix, int in[], char out[]) {
  int j, k, x, y, outexp, shift;
  DGTINT *inr;

  inr = ((DGTINT *)&in[2]) - 2;
  if (in[0] < 0) {
    *out++ = '-';
  }
  x = inr[2];
  shift = log10_radix;
  for (k = log10_radix; k > 0; k--) {
    y = x % 10;
    x /= 10;
    out[k] = '0' + y;
    if (y != 0) {
      shift = k;
    }
  }
  out[0] = out[shift];
  out[1] = '.';
  for (k = 1; k <= log10_radix - shift; k++) {
    out[k + 1] = out[k + shift];
  }
  outexp = log10_radix - shift;
  out += outexp + 2;
  for (j = 3; j <= n + 1; j++) {
    x = inr[j];
    for (k = log10_radix - 1; k >= 0; k--) {
      y = x % 10;
      x /= 10;
      out[k] = '0' + y;
    }
    out += log10_radix;
  }
  *out++ = 'e';
  outexp += log10_radix * in[1];
  sprintf(out, "%d", outexp);
}

void mp_sscanf(int n, int log10_radix, char in[], int out[]) {
  char *s;
  int j, x, outexp, outexp_mod;
  DGTINT *outr;

  outr = ((DGTINT *)&out[2]) - 2;
  while (*in == ' ') {
    in++;
  }
  out[0] = 1;
  if (*in == '-') {
    out[0] = -1;
    in++;
  } else if (*in == '+') {
    in++;
  }
  while (*in == ' ' || *in == '0') {
    in++;
  }
  outexp = 0;
  for (s = in; *s != '\0'; s++) {
    if (*s == 'e' || *s == 'E' || *s == 'd' || *s == 'D') {
      if (sscanf(++s, "%d", &outexp) != 1) {
        outexp = 0;
      }
      break;
    }
  }
  if (*in == '.') {
    do {
      outexp--;
      while (*++in == ' ');
    } while (*in == '0' && *in != '\0');
  } else if (*in != '\0') {
    s = in;
    while (*++s == ' ');
    while (*s >= '0' && *s <= '9' && *s != '\0') {
      outexp++;
      while (*++s == ' ');
    }
  }
  x = outexp / log10_radix;
  outexp_mod = outexp - log10_radix * x;
  if (outexp_mod < 0) {
    x--;
    outexp_mod += log10_radix;
  }
  out[1] = x;
  x = 0;
  j = 2;
  for (s = in; *s != '\0'; s++) {
    if (*s == '.' || *s == ' ') {
      continue;
    }
    if (*s < '0' || *s > '9') {
      break;
    }
    x = 10 * x + (*s - '0');
    if (--outexp_mod < 0) {
      if (j > n + 1) {
        break;
      }
      outr[j++] = (DGTINT)x;
      x = 0;
      outexp_mod = log10_radix - 1;
    }
  }
  while (outexp_mod-- >= 0) {
    x *= 10;
  }
  while (j <= n + 1) {
    outr[j++] = (DGTINT)x;
    x = 0;
  }
  if (outr[2] == 0) {
    out[0] = 0;
    out[1] = 0;
  }
}
