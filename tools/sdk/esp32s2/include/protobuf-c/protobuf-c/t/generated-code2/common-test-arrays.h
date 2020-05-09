
/* data included from the c++ packed-data generator,
   and from the c test code. */

#define THOUSAND     1000
#define MILLION      1000000
#define BILLION      1000000000LL
#define TRILLION     1000000000000LL
#define QUADRILLION  1000000000000000LL
#define QUINTILLION  1000000000000000000LL

int32_t int32_arr0[2] = { -1, 1 };
int32_t int32_arr1[5] = { 42, 666, -1123123, 0, 47 };
int32_t int32_arr_min_max[2] = { INT32_MIN, INT32_MAX };

uint32_t uint32_roundnumbers[4] = { BILLION, MILLION, 1, 0 };
uint32_t uint32_0_max[2] = { 0, UINT32_MAX };

int64_t int64_roundnumbers[15] = { -QUINTILLION, -QUADRILLION, -TRILLION,
                                   -BILLION, -MILLION, -THOUSAND,
                                   1,
                                   THOUSAND, MILLION, BILLION,
                                   TRILLION, QUADRILLION, QUINTILLION };
int64_t int64_min_max[2] = { INT64_MIN, INT64_MAX };

uint64_t uint64_roundnumbers[9] = { 1,
                                   THOUSAND, MILLION, BILLION,
                                   TRILLION, QUADRILLION, QUINTILLION };
uint64_t uint64_0_1_max[3] = { 0, 1, UINT64_MAX };
uint64_t uint64_random[] = {0,
                          666,
                          4200000000ULL,
                          16ULL * (uint64_t) QUINTILLION + 33 };

#define FLOATING_POINT_RANDOM \
-1000.0, -100.0, -42.0, 0, 666, 131313
float float_random[] = { FLOATING_POINT_RANDOM };
double double_random[] = { FLOATING_POINT_RANDOM };

protobuf_c_boolean boolean_0[]  = {0 };
protobuf_c_boolean boolean_1[]  = {1 };
protobuf_c_boolean boolean_random[] = {0,1,1,0,0,1,1,1,0,0,0,0,0,1,1,1,1,1,1,0,1,1,0,1,1,0 };

TEST_ENUM_SMALL_TYPE_NAME enum_small_0[] = { TEST_ENUM_SMALL(VALUE) };
TEST_ENUM_SMALL_TYPE_NAME enum_small_1[] = { TEST_ENUM_SMALL(OTHER_VALUE) };
#define T(v) (TEST_ENUM_SMALL_TYPE_NAME)(v)
TEST_ENUM_SMALL_TYPE_NAME enum_small_random[] = {T(0),T(1),T(1),T(0),T(0),T(1),T(1),T(1),T(0),T(0),T(0),T(0),T(0),T(1),T(1),T(1),T(1),T(1),T(1),T(0),T(1),T(1),T(0),T(1),T(1),T(0) };
#undef T

#define T(v) (TEST_ENUM_TYPE_NAME)(v)
TEST_ENUM_TYPE_NAME enum_0[] = { T(0) };
TEST_ENUM_TYPE_NAME enum_1[] = { T(1) };
TEST_ENUM_TYPE_NAME enum_random[] = { 
   T(0), T(268435455), T(127), T(16384), T(16383),
   T(2097152), T(2097151), T(128), T(268435456),
   T(0), T(2097152), T(268435455), T(127), T(16383), T(16384) };
#undef T

char *repeated_strings_0[] = { (char*)"onestring" };
char *repeated_strings_1[] = { (char*)"two", (char*)"string" };
char *repeated_strings_2[] = { (char*)"many", (char*)"tiny", (char*)"little", (char*)"strings", (char*)"should", (char*)"be", (char*)"handled" };
char *repeated_strings_3[] = { (char*)"one very long strings XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" };
