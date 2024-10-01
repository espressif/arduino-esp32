/*
  Linpack test for Arduino and ESP32.
  Based on https://github.com/VioletGiraffe/EmbeddedLinpack
  Created by Violet Giraffe, 2018
  Adapted by Lucas Saavedra Vaz, 2024
*/

#include <Arduino.h>
#include <math.h>

// Number of runs to average
#define N_RUNS 1000

using floating_point_t = float;
bool type_float;

floating_point_t benchmark(void);
floating_point_t cpu_time(void);
void daxpy(int n, floating_point_t da, floating_point_t dx[], int incx, floating_point_t dy[], int incy);
floating_point_t ddot(int n, floating_point_t dx[], int incx, floating_point_t dy[], int incy);
int dgefa(floating_point_t a[], int lda, int n, int ipvt[]);
void dgesl(floating_point_t a[], int lda, int n, int ipvt[], floating_point_t b[], int job);
void dscal(int n, floating_point_t sa, floating_point_t x[], int incx);
int idamax(int n, floating_point_t dx[], int incx);
floating_point_t r8_abs(floating_point_t x);
floating_point_t r8_epsilon(void);
floating_point_t r8_max(floating_point_t x, floating_point_t y);
floating_point_t r8_random(int iseed[4]);
void r8mat_gen(int lda, int n, floating_point_t *a);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  String data_type;

  if (sizeof(floating_point_t) == sizeof(float)) {
    data_type = "float";
    type_float = true;
  } else if (sizeof(floating_point_t) == sizeof(double)) {
    data_type = "double";
    type_float = false;
  } else {
    data_type = "unknown";
    log_e("Unknown data type size. Aborting.");
    while (1);
  }

  log_d("Starting Linpack %s test", data_type.c_str());
  Serial.printf("Runs: %d\n", N_RUNS);
  Serial.printf("Type: %s\n", data_type.c_str());
  Serial.flush();
  int i = 0;

  floating_point_t minMflops = 1000000000.0, maxMflops = 0.0, avgMflops = 0.0;
  for (i = 0; i < N_RUNS; ++i) {
    Serial.printf("Run %d\n", i);
    const auto mflops = benchmark();
    avgMflops += mflops;
    minMflops = fmin(mflops, minMflops);
    maxMflops = fmax(mflops, maxMflops);
    Serial.flush();
  }

  avgMflops /= N_RUNS;
  Serial.println(String("Runs completed: ") + i);
  Serial.println(String("Average MFLOPS: ") + avgMflops);
  Serial.println(String("Min MFLOPS: ") + minMflops);
  Serial.println(String("Max MFLOPS: ") + maxMflops);
  Serial.flush();
}

void loop() {
  vTaskDelete(NULL);
}

/******************************************************************************/

floating_point_t benchmark(void)

/******************************************************************************/
/*
  Purpose:

    MAIN is the main program for LINPACK_BENCH.

  Discussion:

    LINPACK_BENCH drives the floating_point_t precision LINPACK benchmark program.

  Modified:

    25 July 2008

  Parameters:

    N is the problem size.
*/
{
#define N   8
#define LDA (N + 1)

  static floating_point_t a[N * LDA];
  static floating_point_t a_max;
  static floating_point_t b[N];
  static floating_point_t b_max;
  const floating_point_t cray = 0.056;
  static floating_point_t eps;
  int i;
  int info;
  static int ipvt[N];
  int j;
  int job;
  floating_point_t ops;
  static floating_point_t resid[N];
  floating_point_t resid_max;
  [[maybe_unused]]
  floating_point_t residn;
  static floating_point_t rhs[N];
  floating_point_t t1;
  floating_point_t t2;
  static floating_point_t time[6];
  floating_point_t total;
  floating_point_t x[N];

  log_d("LINPACK_BENCH");
  log_d("  C version");
  log_d("  The LINPACK benchmark.");
  log_d("  Language: C");
  if (!type_float) {
    log_d("  Datatype: Double precision real");
  } else if (type_float) {
    log_d("  Datatype: Single precision real");
  } else {
    log_d("  Datatype: unknown");
  }
  log_d("  Matrix order N               = %d", N);
  log_d("  Leading matrix dimension LDA = %d", LDA);

  ops = (floating_point_t)(2L * N * N * N) / 3.0 + 2.0 * (floating_point_t)((long)N * N);

  /*
  Allocate space for arrays.
*/
  r8mat_gen(LDA, N, a);

  a_max = 0.0;
  for (j = 0; j < N; j++) {
    for (i = 0; i < N; i++) {
      a_max = r8_max(a_max, a[i + j * LDA]);
    }
  }

  for (i = 0; i < N; i++) {
    x[i] = 1.0;
  }

  for (i = 0; i < N; i++) {
    b[i] = 0.0;
    for (j = 0; j < N; j++) {
      b[i] = b[i] + a[i + j * LDA] * x[j];
    }
  }
  t1 = cpu_time();

  info = dgefa(a, LDA, N, ipvt);

  if (info != 0) {
    log_d("LINPACK_BENCH - Fatal error!");
    log_d("  The matrix A is apparently singular.");
    log_d("  Abnormal end of execution.");
    return 1;
  }

  t2 = cpu_time();
  time[0] = t2 - t1;

  t1 = cpu_time();

  job = 0;
  dgesl(a, LDA, N, ipvt, b, job);

  t2 = cpu_time();
  time[1] = t2 - t1;

  total = time[0] + time[1];

  /*
  Compute a residual to verify results.
*/
  r8mat_gen(LDA, N, a);

  for (i = 0; i < N; i++) {
    x[i] = 1.0;
  }

  for (i = 0; i < N; i++) {
    rhs[i] = 0.0;
    for (j = 0; j < N; j++) {
      rhs[i] = rhs[i] + a[i + j * LDA] * x[j];
    }
  }

  for (i = 0; i < N; i++) {
    resid[i] = -rhs[i];
    for (j = 0; j < N; j++) {
      resid[i] = resid[i] + a[i + j * LDA] * b[j];
    }
  }

  resid_max = 0.0;
  for (i = 0; i < N; i++) {
    resid_max = r8_max(resid_max, r8_abs(resid[i]));
  }

  b_max = 0.0;
  for (i = 0; i < N; i++) {
    b_max = r8_max(b_max, r8_abs(b[i]));
  }

  eps = r8_epsilon();

  residn = resid_max / (floating_point_t)N / a_max / b_max / eps;

  time[2] = total;
  if (0.0 < total) {
    time[3] = ops / (1.0E+06 * total);
  } else {
    time[3] = -1.0;
  }
  time[4] = 2.0 / time[3];
  time[5] = total / cray;

  log_d("");
  log_d("     Norm. Resid      Resid           MACHEP         X[1]          X[N]");
  log_d("  %14f  %14f  %14e  %14f  %14f", residn, resid_max, eps, b[0], b[N - 1]);
  log_d("");
  log_d("      Factor     Solve      Total     MFLOPS       Unit      Cray-Ratio");
  log_d("  %9f  %9f  %9f  %9f  %9f  %9f", time[0], time[1], time[2], time[3], time[4], time[5]);

  /*
  Terminate.
*/
  log_d("");
  log_d("LINPACK_BENCH");
  log_d("  Normal end of execution.");
  log_d("");

  return time[3];
#undef LDA
#undef N
}
/******************************************************************************/

floating_point_t cpu_time(void)

/******************************************************************************/
/*
  Purpose:

    CPU_TIME returns the current reading on the CPU clock.

  Discussion:

    The CPU time measurements available through this routine are often
    not very accurate.  In some cases, the accuracy is no better than
    a hundredth of a second.

  Licensing:

    This code is distributed under the GNU LGPL license.

  Modified:

    06 June 2005

  Author:

    John Burkardt

  Parameters:

    Output, floating_point_t CPU_TIME, the current reading of the CPU clock, in seconds.
*/
{
  floating_point_t value;

  value = (floating_point_t)micros() / (floating_point_t)1000000;

  return value;
}
/******************************************************************************/

void daxpy(int n, floating_point_t da, floating_point_t dx[], int incx, floating_point_t dy[], int incy)

/******************************************************************************/
/*
  Purpose:

    DAXPY computes constant times a vector plus a vector.

  Discussion:

    This routine uses unrolled loops for increments equal to one.

  Modified:

    30 March 2007

  Author:

    FORTRAN77 original by Jack Dongarra, Cleve Moler, Jim Bunch, Pete Stewart.
    C version by John Burkardt

  Reference:

    Jack Dongarra, Cleve Moler, Jim Bunch, Pete Stewart,
    LINPACK User's Guide,
    SIAM, 1979.

    Charles Lawson, Richard Hanson, David Kincaid, Fred Krogh,
    Basic Linear Algebra Subprograms for Fortran Usage,
    Algorithm 539,
    ACM Transactions on Mathematical Software,
    Volume 5, Number 3, September 1979, pages 308-323.

  Parameters:

    Input, int N, the number of elements in DX and DY.

    Input, floating_point_t DA, the multiplier of DX.

    Input, floating_point_t DX[*], the first vector.

    Input, int INCX, the increment between successive entries of DX.

    Input/output, floating_point_t DY[*], the second vector.
    On output, DY[*] has been replaced by DY[*] + DA * DX[*].

    Input, int INCY, the increment between successive entries of DY.
*/
{
  int i;
  int ix;
  int iy;
  int m;

  if (n <= 0) {
    return;
  }

  if (da == 0.0) {
    return;
  }
  /*
  Code for unequal increments or equal increments
  not equal to 1.
*/
  if (incx != 1 || incy != 1) {
    if (0 <= incx) {
      ix = 0;
    } else {
      ix = (-n + 1) * incx;
    }

    if (0 <= incy) {
      iy = 0;
    } else {
      iy = (-n + 1) * incy;
    }

    for (i = 0; i < n; i++) {
      dy[iy] = dy[iy] + da * dx[ix];
      ix = ix + incx;
      iy = iy + incy;
    }
  }
  /*
  Code for both increments equal to 1.
*/
  else {
    m = n % 4;

    for (i = 0; i < m; i++) {
      dy[i] = dy[i] + da * dx[i];
    }

    for (i = m; i < n; i = i + 4) {
      dy[i] = dy[i] + da * dx[i];
      dy[i + 1] = dy[i + 1] + da * dx[i + 1];
      dy[i + 2] = dy[i + 2] + da * dx[i + 2];
      dy[i + 3] = dy[i + 3] + da * dx[i + 3];
    }
  }
  return;
}
/******************************************************************************/

floating_point_t ddot(int n, floating_point_t dx[], int incx, floating_point_t dy[], int incy)

/******************************************************************************/
/*
  Purpose:

    DDOT forms the dot product of two vectors.

  Discussion:

    This routine uses unrolled loops for increments equal to one.

  Modified:

    30 March 2007

  Author:

    FORTRAN77 original by Jack Dongarra, Cleve Moler, Jim Bunch, Pete Stewart.
    C version by John Burkardt

  Reference:

    Jack Dongarra, Cleve Moler, Jim Bunch, Pete Stewart,
    LINPACK User's Guide,
    SIAM, 1979.

    Charles Lawson, Richard Hanson, David Kincaid, Fred Krogh,
    Basic Linear Algebra Subprograms for Fortran Usage,
    Algorithm 539,
    ACM Transactions on Mathematical Software,
    Volume 5, Number 3, September 1979, pages 308-323.

  Parameters:

    Input, int N, the number of entries in the vectors.

    Input, floating_point_t DX[*], the first vector.

    Input, int INCX, the increment between successive entries in DX.

    Input, floating_point_t DY[*], the second vector.

    Input, int INCY, the increment between successive entries in DY.

    Output, floating_point_t DDOT, the sum of the product of the corresponding
    entries of DX and DY.
*/
{
  floating_point_t dtemp;
  int i;
  int ix;
  int iy;
  int m;

  dtemp = 0.0;

  if (n <= 0) {
    return dtemp;
  }
  /*
  Code for unequal increments or equal increments
  not equal to 1.
*/
  if (incx != 1 || incy != 1) {
    if (0 <= incx) {
      ix = 0;
    } else {
      ix = (-n + 1) * incx;
    }

    if (0 <= incy) {
      iy = 0;
    } else {
      iy = (-n + 1) * incy;
    }

    for (i = 0; i < n; i++) {
      dtemp = dtemp + dx[ix] * dy[iy];
      ix = ix + incx;
      iy = iy + incy;
    }
  }
  /*
  Code for both increments equal to 1.
*/
  else {
    m = n % 5;

    for (i = 0; i < m; i++) {
      dtemp = dtemp + dx[i] * dy[i];
    }

    for (i = m; i < n; i = i + 5) {
      dtemp = dtemp + dx[i] * dy[i] + dx[i + 1] * dy[i + 1] + dx[i + 2] * dy[i + 2] + dx[i + 3] * dy[i + 3] + dx[i + 4] * dy[i + 4];
    }
  }
  return dtemp;
}
/******************************************************************************/

int dgefa(floating_point_t a[], int lda, int n, int ipvt[])

/******************************************************************************/
/*
  Purpose:

    DGEFA factors a real general matrix.

  Modified:

    16 May 2005

  Author:

    C version by John Burkardt.

  Reference:

    Jack Dongarra, Cleve Moler, Jim Bunch and Pete Stewart,
    LINPACK User's Guide,
    SIAM, (Society for Industrial and Applied Mathematics),
    3600 University City Science Center,
    Philadelphia, PA, 19104-2688.
    ISBN 0-89871-172-X

  Parameters:

    Input/output, floating_point_t A[LDA*N].
    On input, the matrix to be factored.
    On output, an upper triangular matrix and the multipliers used to obtain
    it.  The factorization can be written A=L*U, where L is a product of
    permutation and unit lower triangular matrices, and U is upper triangular.

    Input, int LDA, the leading dimension of A.

    Input, int N, the order of the matrix A.

    Output, int IPVT[N], the pivot indices.

    Output, int DGEFA, singularity indicator.
    0, normal value.
    K, if U(K,K) == 0.  This is not an error condition for this subroutine,
    but it does indicate that DGESL or DGEDI will divide by zero if called.
    Use RCOND in DGECO for a reliable indication of singularity.
*/
{
  int info;
  int j;
  int k;
  int l;
  floating_point_t t;
  /*
  Gaussian elimination with partial pivoting.
*/
  info = 0;

  for (k = 1; k <= n - 1; k++) {
    /*
  Find L = pivot index.
*/
    l = idamax(n - k + 1, a + (k - 1) + (k - 1) * lda, 1) + k - 1;
    ipvt[k - 1] = l;
    /*
  Zero pivot implies this column already triangularized.
*/
    if (a[l - 1 + (k - 1) * lda] == 0.0) {
      info = k;
      continue;
    }
    /*
  Interchange if necessary.
*/
    if (l != k) {
      t = a[l - 1 + (k - 1) * lda];
      a[l - 1 + (k - 1) * lda] = a[k - 1 + (k - 1) * lda];
      a[k - 1 + (k - 1) * lda] = t;
    }
    /*
  Compute multipliers.
*/
    t = -1.0 / a[k - 1 + (k - 1) * lda];

    dscal(n - k, t, a + k + (k - 1) * lda, 1);
    /*
  Row elimination with column indexing.
*/
    for (j = k + 1; j <= n; j++) {
      t = a[l - 1 + (j - 1) * lda];
      if (l != k) {
        a[l - 1 + (j - 1) * lda] = a[k - 1 + (j - 1) * lda];
        a[k - 1 + (j - 1) * lda] = t;
      }
      daxpy(n - k, t, a + k + (k - 1) * lda, 1, a + k + (j - 1) * lda, 1);
    }
  }

  ipvt[n - 1] = n;

  if (a[n - 1 + (n - 1) * lda] == 0.0) {
    info = n;
  }

  return info;
}
/******************************************************************************/

void dgesl(floating_point_t a[], int lda, int n, int ipvt[], floating_point_t b[], int job)

/******************************************************************************/
/*
  Purpose:

    DGESL solves a real general linear system A * X = B.

  Discussion:

    DGESL can solve either of the systems A * X = B or A' * X = B.

    The system matrix must have been factored by DGECO or DGEFA.

    A division by zero will occur if the input factor contains a
    zero on the diagonal.  Technically this indicates singularity
    but it is often caused by improper arguments or improper
    setting of LDA.  It will not occur if the subroutines are
    called correctly and if DGECO has set 0.0 < RCOND
    or DGEFA has set INFO == 0.

  Modified:

    16 May 2005

  Author:

    C version by John Burkardt.

  Reference:

    Jack Dongarra, Cleve Moler, Jim Bunch and Pete Stewart,
    LINPACK User's Guide,
    SIAM, (Society for Industrial and Applied Mathematics),
    3600 University City Science Center,
    Philadelphia, PA, 19104-2688.
    ISBN 0-89871-172-X

  Parameters:

    Input, floating_point_t A[LDA*N], the output from DGECO or DGEFA.

    Input, int LDA, the leading dimension of A.

    Input, int N, the order of the matrix A.

    Input, int IPVT[N], the pivot vector from DGECO or DGEFA.

    Input/output, floating_point_t B[N].
    On input, the right hand side vector.
    On output, the solution vector.

    Input, int JOB.
    0, solve A * X = B;
    nonzero, solve A' * X = B.
*/
{
  int k;
  int l;
  floating_point_t t;
  /*
  Solve A * X = B.
*/
  if (job == 0) {
    for (k = 1; k <= n - 1; k++) {
      l = ipvt[k - 1];
      t = b[l - 1];

      if (l != k) {
        b[l - 1] = b[k - 1];
        b[k - 1] = t;
      }

      daxpy(n - k, t, a + k + (k - 1) * lda, 1, b + k, 1);
    }

    for (k = n; 1 <= k; k--) {
      b[k - 1] = b[k - 1] / a[k - 1 + (k - 1) * lda];
      t = -b[k - 1];
      daxpy(k - 1, t, a + 0 + (k - 1) * lda, 1, b, 1);
    }
  }
  /*
  Solve A' * X = B.
*/
  else {
    for (k = 1; k <= n; k++) {
      t = ddot(k - 1, a + 0 + (k - 1) * lda, 1, b, 1);
      b[k - 1] = (b[k - 1] - t) / a[k - 1 + (k - 1) * lda];
    }

    for (k = n - 1; 1 <= k; k--) {
      b[k - 1] = b[k - 1] + ddot(n - k, a + k + (k - 1) * lda, 1, b + k, 1);
      l = ipvt[k - 1];

      if (l != k) {
        t = b[l - 1];
        b[l - 1] = b[k - 1];
        b[k - 1] = t;
      }
    }
  }
  return;
}
/******************************************************************************/

void dscal(int n, floating_point_t sa, floating_point_t x[], int incx)

/******************************************************************************/
/*
  Purpose:

    DSCAL scales a vector by a constant.

  Modified:

    30 March 2007

  Author:

    FORTRAN77 original by Jack Dongarra, Cleve Moler, Jim Bunch, Pete Stewart.
    C version by John Burkardt

  Reference:

    Jack Dongarra, Cleve Moler, Jim Bunch, Pete Stewart,
    LINPACK User's Guide,
    SIAM, 1979.

    Charles Lawson, Richard Hanson, David Kincaid, Fred Krogh,
    Basic Linear Algebra Subprograms for Fortran Usage,
    Algorithm 539,
    ACM Transactions on Mathematical Software,
    Volume 5, Number 3, September 1979, pages 308-323.

  Parameters:

    Input, int N, the number of entries in the vector.

    Input, floating_point_t SA, the multiplier.

    Input/output, floating_point_t X[*], the vector to be scaled.

    Input, int INCX, the increment between successive entries of X.
*/
{
  int i;
  int ix;
  int m;

  if (n <= 0) {
  } else if (incx == 1) {
    m = n % 5;

    for (i = 0; i < m; i++) {
      x[i] = sa * x[i];
    }

    for (i = m; i < n; i = i + 5) {
      x[i] = sa * x[i];
      x[i + 1] = sa * x[i + 1];
      x[i + 2] = sa * x[i + 2];
      x[i + 3] = sa * x[i + 3];
      x[i + 4] = sa * x[i + 4];
    }
  } else {
    if (0 <= incx) {
      ix = 0;
    } else {
      ix = (-n + 1) * incx;
    }

    for (i = 0; i < n; i++) {
      x[ix] = sa * x[ix];
      ix = ix + incx;
    }
  }
  return;
}
/******************************************************************************/

int idamax(int n, floating_point_t dx[], int incx)

/******************************************************************************/
/*
  Purpose:

    IDAMAX finds the index of the vector element of maximum absolute value.

  Discussion:

    WARNING: This index is a 1-based index, not a 0-based index!

  Modified:

    30 March 2007

  Author:

    FORTRAN77 original by Jack Dongarra, Cleve Moler, Jim Bunch, Pete Stewart.
    C version by John Burkardt

  Reference:

    Jack Dongarra, Cleve Moler, Jim Bunch, Pete Stewart,
    LINPACK User's Guide,
    SIAM, 1979.

    Charles Lawson, Richard Hanson, David Kincaid, Fred Krogh,
    Basic Linear Algebra Subprograms for Fortran Usage,
    Algorithm 539,
    ACM Transactions on Mathematical Software,
    Volume 5, Number 3, September 1979, pages 308-323.

  Parameters:

    Input, int N, the number of entries in the vector.

    Input, floating_point_t X[*], the vector to be examined.

    Input, int INCX, the increment between successive entries of SX.

    Output, int IDAMAX, the index of the element of maximum
    absolute value.
*/
{
  floating_point_t dmax;
  int i;
  int ix;
  int value;

  value = 0;

  if (n < 1 || incx <= 0) {
    return value;
  }

  value = 1;

  if (n == 1) {
    return value;
  }

  if (incx == 1) {
    dmax = r8_abs(dx[0]);

    for (i = 1; i < n; i++) {
      if (dmax < r8_abs(dx[i])) {
        value = i + 1;
        dmax = r8_abs(dx[i]);
      }
    }
  } else {
    ix = 0;
    dmax = r8_abs(dx[0]);
    ix = ix + incx;

    for (i = 1; i < n; i++) {
      if (dmax < r8_abs(dx[ix])) {
        value = i + 1;
        dmax = r8_abs(dx[ix]);
      }
      ix = ix + incx;
    }
  }

  return value;
}
/******************************************************************************/

floating_point_t r8_abs(floating_point_t x)

/******************************************************************************/
/*
  Purpose:

    R8_ABS returns the absolute value of a R8.

  Modified:

    02 April 2005

  Author:

    John Burkardt

  Parameters:

    Input, floating_point_t X, the quantity whose absolute value is desired.

    Output, floating_point_t R8_ABS, the absolute value of X.
*/
{
  floating_point_t value;

  if (0.0 <= x) {
    value = x;
  } else {
    value = -x;
  }
  return value;
}
/******************************************************************************/

floating_point_t r8_epsilon(void)

/******************************************************************************/
/*
  Purpose:

    R8_EPSILON returns the R8 round off unit.

  Discussion:

    R8_EPSILON is a number R which is a power of 2 with the property that,
    to the precision of the computer's arithmetic,
      1 < 1 + R
    but
      1 = ( 1 + R / 2 )

  Licensing:

    This code is distributed under the GNU LGPL license.

  Modified:

    08 May 2006

  Author:

    John Burkardt

  Parameters:

    Output, floating_point_t R8_EPSILON, the floating_point_t precision round-off unit.
*/
{
  floating_point_t r;

  r = 1.0;

  while (1.0 < (floating_point_t)(1.0 + r)) {
    r = r / 2.0;
  }
  r = 2.0 * r;

  return r;
}
/******************************************************************************/

floating_point_t r8_max(floating_point_t x, floating_point_t y)

/******************************************************************************/
/*
  Purpose:

    R8_MAX returns the maximum of two R8's.

  Modified:

    18 August 2004

  Author:

    John Burkardt

  Parameters:

    Input, floating_point_t X, Y, the quantities to compare.

    Output, floating_point_t R8_MAX, the maximum of X and Y.
*/
{
  floating_point_t value;

  if (y < x) {
    value = x;
  } else {
    value = y;
  }
  return value;
}
/******************************************************************************/

floating_point_t r8_random(int iseed[4])

/******************************************************************************/
/*
  Purpose:

    R8_RANDOM returns a uniformly distributed random number between 0 and 1.

  Discussion:

    This routine uses a multiplicative congruential method with modulus
    2**48 and multiplier 33952834046453 (see G.S.Fishman,
    'Multiplicative congruential random number generators with modulus
    2**b: an exhaustive analysis for b = 32 and a partial analysis for
    b = 48', Math. Comp. 189, pp 331-344, 1990).

    48-bit integers are stored in 4 integer array elements with 12 bits
    per element. Hence the routine is portable across machines with
    integers of 32 bits or more.

  Parameters:

    Input/output, integer ISEED(4).
    On entry, the seed of the random number generator; the array
    elements must be between 0 and 4095, and ISEED(4) must be odd.
    On exit, the seed is updated.

    Output, floating_point_t R8_RANDOM, the next pseudorandom number.
*/
{
  int ipw2 = 4096;
  int it1;
  int it2;
  int it3;
  int it4;
  int m1 = 494;
  int m2 = 322;
  int m3 = 2508;
  int m4 = 2549;
  floating_point_t r = 1.0 / 4096.0;
  floating_point_t value;
  /*
  Multiply the seed by the multiplier modulo 2**48.
*/
  it4 = iseed[3] * m4;
  it3 = it4 / ipw2;
  it4 = it4 - ipw2 * it3;
  it3 = it3 + iseed[2] * m4 + iseed[3] * m3;
  it2 = it3 / ipw2;
  it3 = it3 - ipw2 * it2;
  it2 = it2 + iseed[1] * m4 + iseed[2] * m3 + iseed[3] * m2;
  it1 = it2 / ipw2;
  it2 = it2 - ipw2 * it1;
  it1 = it1 + iseed[0] * m4 + iseed[1] * m3 + iseed[2] * m2 + iseed[3] * m1;
  it1 = (it1 % ipw2);
  /*
  Return updated seed
*/
  iseed[0] = it1;
  iseed[1] = it2;
  iseed[2] = it3;
  iseed[3] = it4;
  /*
  Convert 48-bit integer to a real number in the interval (0,1)
*/
  value = r * ((floating_point_t)(it1) + r * ((floating_point_t)(it2) + r * ((floating_point_t)(it3) + r * ((floating_point_t)(it4)))));

  return value;
}
/******************************************************************************/

void r8mat_gen(int lda, int n, floating_point_t *a)

/******************************************************************************/
/*
  Purpose:

    R8MAT_GEN generates a random R8MAT.

  Modified:

    06 June 2005

  Parameters:

    Input, integer LDA, the leading dimension of the matrix.

    Input, integer N, the order of the matrix.

    Output, floating_point_t R8MAT_GEN[LDA*N], the N by N matrix.
*/
{
  int i;
  int init[4] = {1, 2, 3, 1325};
  int j;

  for (j = 1; j <= n; j++) {
    for (i = 1; i <= n; i++) {
      a[i - 1 + (j - 1) * lda] = r8_random(init) - 0.5;
    }
  }
}
/******************************************************************************/
