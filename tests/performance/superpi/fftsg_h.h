/*
  Based on "Calculation of PI(= 3.14159...) using FFT and AGM" by T.Ooura, Nov. 1999.
  https://github.com/Fibonacci43/SuperPI
  Modified for Arduino by Lucas Saavedra Vaz, 2024.
*/

#pragma once

#include <math.h>

#ifndef M_PI_2
#define M_PI_2 1.570796326794896619231321691639751442098584699687
#endif
#ifndef WR5000 /* cos(M_PI_2*0.5000) */
#define WR5000 0.707106781186547524400844362104849039284835937688
#endif
#ifndef WR2500 /* cos(M_PI_2*0.2500) */
#define WR2500 0.923879532511286756128183189396788286822416625863
#endif
#ifndef WI2500 /* sin(M_PI_2*0.2500) */
#define WI2500 0.382683432365089771728459984030398866761344562485
#endif
#ifndef WR1250 /* cos(M_PI_2*0.1250) */
#define WR1250 0.980785280403230449126182236134239036973933730893
#endif
#ifndef WI1250 /* sin(M_PI_2*0.1250) */
#define WI1250 0.195090322016128267848284868477022240927691617751
#endif
#ifndef WR3750 /* cos(M_PI_2*0.3750) */
#define WR3750 0.831469612302545237078788377617905756738560811987
#endif
#ifndef WI3750 /* sin(M_PI_2*0.3750) */
#define WI3750 0.555570233019602224742830813948532874374937190754
#endif

#ifndef CDFT_RECURSIVE_N     /* length of the recursive FFT mode */
#define CDFT_RECURSIVE_N 512 /* <= (L1 cache size) / 16 */
#endif

#ifndef CDFT_LOOP_DIV /* control of the CDFT's speed & tolerance */
#define CDFT_LOOP_DIV 32
#endif

#ifndef RDFT_LOOP_DIV /* control of the RDFT's speed & tolerance */
#define RDFT_LOOP_DIV 64
#endif

#ifndef DCST_LOOP_DIV /* control of the DCT,DST's speed & tolerance */
#define DCST_LOOP_DIV 64
#endif

void bitrv1(int n, double *a);
void bitrv2(int n, double *a);
void bitrv208(double *a);
void bitrv208neg(double *a);
void bitrv216(double *a);
void bitrv216neg(double *a);
void bitrv2conj(int n, double *a);
void cdft(int n, int isgn, double *a);
void cftb040(double *a);
void cftb1st(int n, double *a);
void cftbsub(int n, double *a);
void cftexp1(int n, double *a);
void cftexp2(int n, double *a);
void cftf040(double *a);
void cftf081(double *a);
void cftf082(double *a);
void cftf161(double *a);
void cftf162(double *a);
void cftfsub(int n, double *a);
void cftfx41(int n, double *a);
void cftfx42(int n, double *a);
void cftmdl1(int n, double *a);
void cftmdl2(int n, double *a);
void cftrec1(int n, double *a);
void cftrec2(int n, double *a);
void cftx020(double *a);
void dctsub(int n, double *a);
void dctsub4(int n, double *a);
void ddct(int n, int isgn, double *a);
void ddst(int n, int isgn, double *a);
void dfct(int n, double *a);
void dfst(int n, double *a);
void dstsub(int n, double *a);
void dstsub4(int n, double *a);
void rdft(int n, int isgn, double *a);
void rftbsub(int n, double *a);
void rftfsub(int n, double *a);
