#include "Arduino.h"
#include <stdint.h>
#include <stdio.h>

// a minor hack to rename the main function, so we can call it from C++
#define main(ignore) coremark_main(void)

#define FLAGS_STR "(flags unknown)"

#define PERFORMANCE_RUN 1

// 0 means auto-detect number of iterations for 10 second test
#define ITERATIONS 0

/*
Copyright 2018 Embedded Microprocessor Benchmark Consortium (EEMBC)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Original Author: Shay Gal-on
*/
/* Topic : Description
	This file contains configuration constants required to execute on different platforms
*/
#ifndef CORE_PORTME_H
#define CORE_PORTME_H
/************************/
/* Data types and settings */
/************************/
/* Configuration : HAS_FLOAT
	Define to 1 if the platform supports floating point.
*/
#ifndef HAS_FLOAT
#define HAS_FLOAT 1
#endif
/* Configuration : HAS_TIME_H
	Define to 1 if platform has the time.h header file,
	and implementation of functions thereof.
*/
#ifndef HAS_TIME_H
#define HAS_TIME_H 0
#endif
/* Configuration : USE_CLOCK
	Define to 1 if platform has the time.h header file,
	and implementation of functions thereof.
*/
#ifndef USE_CLOCK
#define USE_CLOCK 0
#endif
/* Configuration : HAS_STDIO
	Define to 1 if the platform has stdio.h.
*/
#ifndef HAS_STDIO
#define HAS_STDIO 1
#endif
/* Configuration : HAS_PRINTF
	Define to 1 if the platform has stdio.h and implements the printf function.
*/
#ifndef HAS_PRINTF
#define HAS_PRINTF 0
#endif


/* Definitions : COMPILER_VERSION, COMPILER_FLAGS, MEM_LOCATION
	Initialize these strings per platform
*/
#ifndef COMPILER_VERSION
 #ifdef __GNUC__
 #define COMPILER_VERSION "GCC"__VERSION__
 #else
 #define COMPILER_VERSION "Please put compiler version here (e.g. gcc 4.1)"
 #endif
#endif
#ifndef COMPILER_FLAGS
 #define COMPILER_FLAGS FLAGS_STR /* "Please put compiler flags here (e.g. -o3)" */
#endif
#ifndef MEM_LOCATION
 #define MEM_LOCATION "STACK"
#endif

/* Data Types :
	To avoid compiler issues, define the data types that need ot be used for 8b, 16b and 32b in <core_portme.h>.

	*Imprtant* :
	ee_ptr_int needs to be the data type used to hold pointers, otherwise coremark may fail!!!
*/
typedef int16_t  ee_s16;
typedef uint16_t ee_u16;
typedef int32_t  ee_s32;
typedef double   ee_f32;
typedef uint8_t  ee_u8;
typedef uint32_t ee_u32;
typedef uintptr_t ee_ptr_int;
typedef size_t ee_size_t;
#define NULL ((void *)0)
/* align_mem :
	This macro is used to align an offset to point to a 32b value. It is used in the Matrix algorithm to initialize the input memory blocks.
*/
#define align_mem(x) (void *)(4 + (((ee_ptr_int)(x) - 1) & ~3))

/* Configuration : CORE_TICKS
	Define type of return from the timing functions.
 */
#define CORETIMETYPE ee_u32
typedef ee_u32 CORE_TICKS;

/* Configuration : SEED_METHOD
	Defines method to get seed values that cannot be computed at compile time.

	Valid values :
	SEED_ARG - from command line.
	SEED_FUNC - from a system function.
	SEED_VOLATILE - from volatile variables.
*/
#ifndef SEED_METHOD
#define SEED_METHOD SEED_VOLATILE
#endif

/* Configuration : MEM_METHOD
	Defines method to get a block of memry.

	Valid values :
	MEM_MALLOC - for platforms that implement malloc and have malloc.h.
	MEM_STATIC - to use a static memory array.
	MEM_STACK - to allocate the data block on the stack (NYI).
*/
#ifndef MEM_METHOD
#define MEM_METHOD MEM_STACK
#endif

/* Configuration : MULTITHREAD
	Define for parallel execution

	Valid values :
	1 - only one context (default).
	N>1 - will execute N copies in parallel.

	Note :
	If this flag is defined to more then 1, an implementation for launching parallel contexts must be defined.

	Two sample implementations are provided. Use <USE_PTHREAD> or <USE_FORK> to enable them.

	It is valid to have a different implementation of <core_start_parallel> and <core_end_parallel> in <core_portme.c>,
	to fit a particular architecture.
*/
#ifndef MULTITHREAD
#define MULTITHREAD CONFIG_SOC_CPU_CORES_NUM
#define PARALLEL_METHOD "FreeRTOS"
#define USE_PTHREAD 0
#define USE_FORK 0
#define USE_SOCKET 0
#endif

/* Configuration : MAIN_HAS_NOARGC
	Needed if platform does not support getting arguments to main.

	Valid values :
	0 - argc/argv to main is supported
	1 - argc/argv to main is not supported

	Note :
	This flag only matters if MULTITHREAD has been defined to a value greater then 1.
*/
#ifndef MAIN_HAS_NOARGC
#define MAIN_HAS_NOARGC 1
#endif

/* Configuration : MAIN_HAS_NORETURN
	Needed if platform does not support returning a value from main.

	Valid values :
	0 - main returns an int, and return value will be 0.
	1 - platform does not support returning a value from main
*/
#ifndef MAIN_HAS_NORETURN
#define MAIN_HAS_NORETURN 0
#endif

/* Variable : default_num_contexts
	Not used for this simple port, must cintain the value 1.
*/
extern ee_u32 default_num_contexts;

typedef struct CORE_PORTABLE_S {
#if (MULTITHREAD > 1)
  TaskHandle_t task;
#endif
	ee_u8	portable_id;
} core_portable;

/* target specific init/fini */
void portable_init(core_portable *p, int *argc, char *argv[]);
void portable_fini(core_portable *p);

#if !defined(PROFILE_RUN) && !defined(PERFORMANCE_RUN) && !defined(VALIDATION_RUN)
#if (TOTAL_DATA_SIZE==1200)
#define PROFILE_RUN 1
#elif (TOTAL_DATA_SIZE==2000)
#define PERFORMANCE_RUN 1
#else
#define VALIDATION_RUN 1
#endif
#endif

int ee_printf(const char *fmt, ...);

#endif /* CORE_PORTME_H */
