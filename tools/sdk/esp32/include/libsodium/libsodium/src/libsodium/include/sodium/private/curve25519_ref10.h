#ifndef curve25519_ref10_H
#define curve25519_ref10_H

#include <stddef.h>
#include <stdint.h>

#define fe crypto_core_curve25519_ref10_fe
typedef int32_t fe[10];

/*
 fe means field element.
 Here the field is \Z/(2^255-19).
 An element t, entries t[0]...t[9], represents the integer
 t[0]+2^26 t[1]+2^51 t[2]+2^77 t[3]+2^102 t[4]+...+2^230 t[9].
 Bounds on each t[i] vary depending on context.
 */

#define fe_frombytes crypto_core_curve25519_ref10_fe_frombytes
#define fe_tobytes crypto_core_curve25519_ref10_fe_tobytes
#define fe_copy crypto_core_curve25519_ref10_fe_copy
#define fe_isnonzero crypto_core_curve25519_ref10_fe_isnonzero
#define fe_isnegative crypto_core_curve25519_ref10_fe_isnegative
#define fe_0 crypto_core_curve25519_ref10_fe_0
#define fe_1 crypto_core_curve25519_ref10_fe_1
#define fe_cmov crypto_core_curve25519_ref10_fe_cmov
#define fe_add crypto_core_curve25519_ref10_fe_add
#define fe_sub crypto_core_curve25519_ref10_fe_sub
#define fe_neg crypto_core_curve25519_ref10_fe_neg
#define fe_mul crypto_core_curve25519_ref10_fe_mul
#define fe_sq crypto_core_curve25519_ref10_fe_sq
#define fe_sq2 crypto_core_curve25519_ref10_fe_sq2
#define fe_invert crypto_core_curve25519_ref10_fe_invert
#define fe_pow22523 crypto_core_curve25519_ref10_fe_pow22523

extern void fe_frombytes(fe,const unsigned char *);
extern void fe_tobytes(unsigned char *,const fe);

extern void fe_copy(fe,const fe);
extern int fe_isnonzero(const fe);
extern int fe_isnegative(const fe);
extern void fe_0(fe);
extern void fe_1(fe);
extern void fe_cmov(fe,const fe,unsigned int);
extern void fe_add(fe,const fe,const fe);
extern void fe_sub(fe,const fe,const fe);
extern void fe_neg(fe,const fe);
extern void fe_mul(fe,const fe,const fe);
extern void fe_sq(fe,const fe);
extern void fe_sq2(fe,const fe);
extern void fe_invert(fe,const fe);
extern void fe_pow22523(fe,const fe);

/*
 ge means group element.
 *
 Here the group is the set of pairs (x,y) of field elements (see fe.h)
 satisfying -x^2 + y^2 = 1 + d x^2y^2
 where d = -121665/121666.
 *
 Representations:
 ge_p2 (projective): (X:Y:Z) satisfying x=X/Z, y=Y/Z
 ge_p3 (extended): (X:Y:Z:T) satisfying x=X/Z, y=Y/Z, XY=ZT
 ge_p1p1 (completed): ((X:Z),(Y:T)) satisfying x=X/Z, y=Y/T
 ge_precomp (Duif): (y+x,y-x,2dxy)
 */

#define ge_p2 crypto_core_curve25519_ref10_ge_p2
typedef struct {
    fe X;
    fe Y;
    fe Z;
} ge_p2;

#define ge_p3 crypto_core_curve25519_ref10_ge_p3
typedef struct {
    fe X;
    fe Y;
    fe Z;
    fe T;
} ge_p3;

#define ge_p1p1 crypto_core_curve25519_ref10_ge_p1p1
typedef struct {
    fe X;
    fe Y;
    fe Z;
    fe T;
} ge_p1p1;

#define ge_precomp crypto_core_curve25519_ref10_ge_precomp
typedef struct {
    fe yplusx;
    fe yminusx;
    fe xy2d;
} ge_precomp;

#define ge_cached crypto_core_curve25519_ref10_ge_cached
typedef struct {
    fe YplusX;
    fe YminusX;
    fe Z;
    fe T2d;
} ge_cached;

#define ge_frombytes_negate_vartime crypto_core_curve25519_ref10_ge_frombytes_negate_vartime
#define ge_tobytes crypto_core_curve25519_ref10_ge_tobytes
#define ge_p3_tobytes crypto_core_curve25519_ref10_ge_p3_tobytes

#define ge_p2_0 crypto_core_curve25519_ref10_ge_p2_0
#define ge_p3_0 crypto_core_curve25519_ref10_ge_p3_0
#define ge_precomp_0 crypto_core_curve25519_ref10_ge_precomp_0
#define ge_p3_to_p2 crypto_core_curve25519_ref10_ge_p3_to_p2
#define ge_p3_to_cached crypto_core_curve25519_ref10_ge_p3_to_cached
#define ge_p1p1_to_p2 crypto_core_curve25519_ref10_ge_p1p1_to_p2
#define ge_p1p1_to_p3 crypto_core_curve25519_ref10_ge_p1p1_to_p3
#define ge_p2_dbl crypto_core_curve25519_ref10_ge_p2_dbl
#define ge_p3_dbl crypto_core_curve25519_ref10_ge_p3_dbl

#define ge_madd crypto_core_curve25519_ref10_ge_madd
#define ge_msub crypto_core_curve25519_ref10_ge_msub
#define ge_add crypto_core_curve25519_ref10_ge_add
#define ge_sub crypto_core_curve25519_ref10_ge_sub
#define ge_scalarmult_base crypto_core_curve25519_ref10_ge_scalarmult_base
#define ge_double_scalarmult_vartime crypto_core_curve25519_ref10_ge_double_scalarmult_vartime
#define ge_scalarmult_vartime crypto_core_curve25519_ref10_ge_scalarmult_vartime

extern void ge_tobytes(unsigned char *,const ge_p2 *);
extern void ge_p3_tobytes(unsigned char *,const ge_p3 *);
extern int ge_frombytes_negate_vartime(ge_p3 *,const unsigned char *);

extern void ge_p2_0(ge_p2 *);
extern void ge_p3_0(ge_p3 *);
extern void ge_precomp_0(ge_precomp *);
extern void ge_p3_to_p2(ge_p2 *,const ge_p3 *);
extern void ge_p3_to_cached(ge_cached *,const ge_p3 *);
extern void ge_p1p1_to_p2(ge_p2 *,const ge_p1p1 *);
extern void ge_p1p1_to_p3(ge_p3 *,const ge_p1p1 *);
extern void ge_p2_dbl(ge_p1p1 *,const ge_p2 *);
extern void ge_p3_dbl(ge_p1p1 *,const ge_p3 *);

extern void ge_madd(ge_p1p1 *,const ge_p3 *,const ge_precomp *);
extern void ge_msub(ge_p1p1 *,const ge_p3 *,const ge_precomp *);
extern void ge_add(ge_p1p1 *,const ge_p3 *,const ge_cached *);
extern void ge_sub(ge_p1p1 *,const ge_p3 *,const ge_cached *);
extern void ge_scalarmult_base(ge_p3 *,const unsigned char *);
extern void ge_double_scalarmult_vartime(ge_p2 *,const unsigned char *,const ge_p3 *,const unsigned char *);
extern void ge_scalarmult_vartime(ge_p3 *,const unsigned char *,const ge_p3 *);

/*
 The set of scalars is \Z/l
 where l = 2^252 + 27742317777372353535851937790883648493.
 */

#define sc_reduce crypto_core_curve25519_ref10_sc_reduce
#define sc_muladd crypto_core_curve25519_ref10_sc_muladd

extern void sc_reduce(unsigned char *);
extern void sc_muladd(unsigned char *,const unsigned char *,const unsigned char *,const unsigned char *);

#endif
