/* Automatically generated file; DO NOT EDIT */
/* Espressif IoT Development Framework Linker Script */
/* Generated from: /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp32/ld/esp32.project.ld.in */

/*  Default entry point:  */
ENTRY(call_start_cpu0);

SECTIONS
{
  /* RTC fast memory holds RTC wake stub code,
     including from any source file named rtc_wake_stub*.c
  */
  .rtc.text :
  {
    . = ALIGN(4);

    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .rtc.literal EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .rtc.text EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:queue.* *libfreertos.a:port.*) .rtc.text.*)
    *libesp_system.a:startup.*( .rtc.text.*)
    *libesp_system.a:startup.*(.rtc.text.do_core_init)
    *libesp_system.a:startup.*(.rtc.text.do_secondary_init)
    *libesp_system.a:startup.*(.rtc.text.start_cpu0_default)
    *libfreertos.a:port.*( .rtc.text.*)
    *libfreertos.a:queue.*( .rtc.text.*)
    *libfreertos.a:port.*(.rtc.text.esp_startup_start_app)
    *libfreertos.a:port.*(.rtc.text.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.rtc.text.main_task)
    *libfreertos.a:queue.*(.rtc.text.xQueueGenericCreateStatic)
    *libhal.a:twai_hal_iram.*( .rtc.literal  .rtc.text  .rtc.text.*)
    *libhal.a:uart_hal_iram.*( .rtc.literal  .rtc.text  .rtc.text.*)

    *rtc_wake_stub*.*(.literal .text .literal.* .text.*)
    _rtc_text_end = ABSOLUTE(.);
  } > rtc_iram_seg

  /*
    This section is required to skip rtc.text area because rtc_iram_seg and
    rtc_data_seg are reflect the same address space on different buses.
  */
  .rtc.dummy :
  {
    _rtc_dummy_start = ABSOLUTE(.);
    _rtc_fast_start = ABSOLUTE(.);
    . = SIZEOF(.rtc.text);
    _rtc_dummy_end = ABSOLUTE(.);
  } > rtc_data_seg

  /* This section located in RTC FAST Memory area.
     It holds data marked with RTC_FAST_ATTR attribute.
     See the file "esp_attr.h" for more information.
  */
  .rtc.force_fast :
  {
    . = ALIGN(4);
    _rtc_force_fast_start = ABSOLUTE(.);
    
    _coredump_rtc_fast_start = ABSOLUTE(.);
    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .rtc.fast.coredump EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:port.* *libfreertos.a:queue.*) .rtc.fast.coredump.*)
    *libesp_system.a:startup.*( .rtc.fast.coredump.*)
    *libesp_system.a:startup.*(.rtc.fast.coredump.do_core_init)
    *libesp_system.a:startup.*(.rtc.fast.coredump.do_secondary_init)
    *libesp_system.a:startup.*(.rtc.fast.coredump.start_cpu0_default)
    *libfreertos.a:port.*( .rtc.fast.coredump.*)
    *libfreertos.a:queue.*( .rtc.fast.coredump.*)
    *libfreertos.a:port.*(.rtc.fast.coredump.esp_startup_start_app)
    *libfreertos.a:port.*(.rtc.fast.coredump.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.rtc.fast.coredump.main_task)
    *libfreertos.a:queue.*(.rtc.fast.coredump.xQueueGenericCreateStatic)
    *libhal.a:twai_hal_iram.*( .rtc.fast.coredump  .rtc.fast.coredump.*)
    *libhal.a:uart_hal_iram.*( .rtc.fast.coredump  .rtc.fast.coredump.*)
    _coredump_rtc_fast_end = ABSOLUTE(.);
    
    *(.rtc.force_fast .rtc.force_fast.*)
    . = ALIGN(4) ;

    _rtc_force_fast_end = ABSOLUTE(.);
  } > rtc_data_seg

  /* RTC data section holds RTC wake stub
     data/rodata, including from any source file
     named rtc_wake_stub*.c and the data marked with
     RTC_DATA_ATTR, RTC_RODATA_ATTR attributes.
     The memory location of the data is dependent on
     CONFIG_ESP32_RTCDATA_IN_FAST_MEM option.
  */
  .rtc.data :
  {
    _rtc_data_start = ABSOLUTE(.);

    /* coredump mapping */
    _coredump_rtc_start = ABSOLUTE(.);
    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .rtc.coredump EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:queue.* *libfreertos.a:port.*) .rtc.coredump.*)
    *libesp_system.a:startup.*( .rtc.coredump.*)
    *libesp_system.a:startup.*(.rtc.coredump.do_core_init)
    *libesp_system.a:startup.*(.rtc.coredump.do_secondary_init)
    *libesp_system.a:startup.*(.rtc.coredump.start_cpu0_default)
    *libfreertos.a:port.*( .rtc.coredump.*)
    *libfreertos.a:queue.*( .rtc.coredump.*)
    *libfreertos.a:port.*(.rtc.coredump.esp_startup_start_app)
    *libfreertos.a:port.*(.rtc.coredump.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.rtc.coredump.main_task)
    *libfreertos.a:queue.*(.rtc.coredump.xQueueGenericCreateStatic)
    *libhal.a:twai_hal_iram.*( .rtc.coredump  .rtc.coredump.*)
    *libhal.a:uart_hal_iram.*( .rtc.coredump  .rtc.coredump.*)
    _coredump_rtc_end = ABSOLUTE(.);

    /* should be placed after coredump mapping */
    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .rtc.data EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:queue.* *libfreertos.a:port.*) .rtc.data.* EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .rtc.rodata EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:queue.* *libfreertos.a:port.*) .rtc.rodata.*)
    *libesp_system.a:startup.*( .rtc.data.*  .rtc.rodata.*)
    *libesp_system.a:startup.*(.rtc.data.do_core_init .rtc.rodata.do_core_init)
    *libesp_system.a:startup.*(.rtc.data.do_secondary_init .rtc.rodata.do_secondary_init)
    *libesp_system.a:startup.*(.rtc.data.start_cpu0_default .rtc.rodata.start_cpu0_default)
    *libfreertos.a:port.*( .rtc.data.*  .rtc.rodata.*)
    *libfreertos.a:queue.*( .rtc.data.*  .rtc.rodata.*)
    *libfreertos.a:port.*(.rtc.data.esp_startup_start_app .rtc.rodata.esp_startup_start_app)
    *libfreertos.a:port.*(.rtc.data.esp_startup_start_app_other_cores .rtc.rodata.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.rtc.data.main_task .rtc.rodata.main_task)
    *libfreertos.a:queue.*(.rtc.data.xQueueGenericCreateStatic .rtc.rodata.xQueueGenericCreateStatic)
    *libhal.a:twai_hal_iram.*( .rtc.data  .rtc.data.*  .rtc.rodata  .rtc.rodata.*)
    *libhal.a:uart_hal_iram.*( .rtc.data  .rtc.data.*  .rtc.rodata  .rtc.rodata.*)
    
    *rtc_wake_stub*.*(.data .rodata .data.* .rodata.* .bss .bss.*)
    _rtc_data_end = ABSOLUTE(.);
    
  } > rtc_data_location

  /* RTC bss, from any source file named rtc_wake_stub*.c */
  .rtc.bss (NOLOAD) :
  {
    _rtc_bss_start = ABSOLUTE(.);
    *rtc_wake_stub*.*(.bss .bss.*)
    *rtc_wake_stub*.*(COMMON)

    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .rtc.bss)
    *libhal.a:twai_hal_iram.*( .rtc.bss)
    *libhal.a:uart_hal_iram.*( .rtc.bss)

    _rtc_bss_end = ABSOLUTE(.);
  } > rtc_data_location

  /* This section holds data that should not be initialized at power up
     and will be retained during deep sleep.
     User data marked with RTC_NOINIT_ATTR will be placed
     into this section. See the file "esp_attr.h" for more information.
	 The memory location of the data is dependent on
     CONFIG_ESP32_RTCDATA_IN_FAST_MEM option.
  */
  .rtc_noinit (NOLOAD):
  {
    . = ALIGN(4);
    _rtc_noinit_start = ABSOLUTE(.);
    *(.rtc_noinit .rtc_noinit.*)
    . = ALIGN(4) ;
    _rtc_noinit_end = ABSOLUTE(.);
  } > rtc_data_location

  /* This section located in RTC SLOW Memory area.
     It holds data marked with RTC_SLOW_ATTR attribute.
     See the file "esp_attr.h" for more information.
  */
  .rtc.force_slow :
  {
    . = ALIGN(4);
    _rtc_force_slow_start = ABSOLUTE(.);
    *(.rtc.force_slow .rtc.force_slow.*)
    . = ALIGN(4) ;
    _rtc_force_slow_end = ABSOLUTE(.);
  } > rtc_slow_seg

  /* Get size of rtc slow data based on rtc_data_location alias */
  _rtc_slow_length = (ORIGIN(rtc_slow_seg) == ORIGIN(rtc_data_location))
                        ? (_rtc_force_slow_end - _rtc_data_start)
                        : (_rtc_force_slow_end - _rtc_force_slow_start);

  _rtc_fast_length = (ORIGIN(rtc_slow_seg) == ORIGIN(rtc_data_location))
                        ? (_rtc_force_fast_end - _rtc_fast_start)
                        : (_rtc_noinit_end - _rtc_fast_start);

  ASSERT((_rtc_slow_length <= LENGTH(rtc_slow_seg)),
          "RTC_SLOW segment data does not fit.")

  ASSERT((_rtc_fast_length <= LENGTH(rtc_data_seg)),
          "RTC_FAST segment data does not fit.")

  /* Send .iram0 code to iram */
  .iram0.vectors :
  {
    _iram_start = ABSOLUTE(.);
    /* Vectors go to IRAM */
    _init_start = ABSOLUTE(.);
    /* Vectors according to builds/RF-2015.2-win32/esp108_v1_2_s5_512int_2/config.html */
    . = 0x0;
    KEEP(*(.WindowVectors.text));
    . = 0x180;
    KEEP(*(.Level2InterruptVector.text));
    . = 0x1c0;
    KEEP(*(.Level3InterruptVector.text));
    . = 0x200;
    KEEP(*(.Level4InterruptVector.text));
    . = 0x240;
    KEEP(*(.Level5InterruptVector.text));
    . = 0x280;
    KEEP(*(.DebugExceptionVector.text));
    . = 0x2c0;
    KEEP(*(.NMIExceptionVector.text));
    . = 0x300;
    KEEP(*(.KernelExceptionVector.text));
    . = 0x340;
    KEEP(*(.UserExceptionVector.text));
    . = 0x3C0;
    KEEP(*(.DoubleExceptionVector.text));
    . = 0x400;
    _invalid_pc_placeholder = ABSOLUTE(.);
    *(.*Vector.literal)

    *(.UserEnter.literal);
    *(.UserEnter.text);
    . = ALIGN (16);
    *(.entry.text)
    *(.init.literal)
    *(.init)

    _init_end = ABSOLUTE(.);
  } > iram0_0_seg

  .iram0.text :
  {
    /* Code marked as runnning out of IRAM */
    _iram_text_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .iram1 EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:port.* *libfreertos.a:queue.*) .iram1.*)
    *libapp_trace.a:SEGGER_RTT_esp32.*( .literal  .literal.*  .text  .text.*)
    *libapp_trace.a:SEGGER_SYSVIEW.*( .literal  .literal.*  .text  .text.*)
    *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.*( .literal  .literal.*  .text  .text.*)
    *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.*( .literal  .literal.*  .text  .text.*)
    *libapp_trace.a:app_trace.*( .literal  .literal.*  .text  .text.*)
    *libapp_trace.a:app_trace_util.*( .literal  .literal.*  .text  .text.*)
    *libc.a:creat.*( .literal  .literal.*  .text  .text.*)
    *libc.a:isatty.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-abs.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-asctime.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-asctime_r.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-atoi.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-atol.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-bzero.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-close.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-creat.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-ctime.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-ctime_r.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-ctype_.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-div.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-environ.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-envlock.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-fclose.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-fflush.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-findfp.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-fputwc.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-fvwrite.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-fwalk.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-getenv_r.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-gettzinfo.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-gmtime.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-gmtime_r.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-impure.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-isalnum.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-isalpha.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-isascii.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-isblank.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-iscntrl.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-isdigit.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-isgraph.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-islower.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-isprint.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-ispunct.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-isspace.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-isupper.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-itoa.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-labs.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-lcltime.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-lcltime_r.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-ldiv.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-longjmp.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-makebuf.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-memccpy.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-memchr.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-memcmp.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-memcpy.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-memmove.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-memrchr.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-memset.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-mktime.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-month_lengths.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-open.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-quorem.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-raise.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-rand.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-rand_r.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-read.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-refill.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-rshift.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-s_fpclassify.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-sbrk.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-sccl.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-setjmp.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-sf_nan.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-srand.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-stdio.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strcasecmp.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strcasestr.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strcat.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strchr.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strcmp.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strcoll.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strcpy.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strcspn.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strdup.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strdup_r.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strftime.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strlcat.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strlcpy.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strlen.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strlwr.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strncasecmp.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strncat.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strncmp.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strncpy.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strndup.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strndup_r.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strnlen.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strptime.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strrchr.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strsep.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strspn.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strstr.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strtok_r.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strtol.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strtoul.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-strupr.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-sysclose.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-sysopen.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-sysread.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-syssbrk.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-system.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-systimes.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-syswrite.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-time.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-timelocal.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-toascii.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-tolower.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-toupper.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-tzcalc_limits.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-tzlock.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-tzset.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-tzset_r.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-tzvars.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-ungetc.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-utoa.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-wbuf.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-wcrtomb.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-wctomb_r.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lib_a-wsetup.*( .literal  .literal.*  .text  .text.*)
    *libc.a:lock.*( .literal  .literal.*  .text  .text.*)
    *libesp_common.a:esp_err.*( .literal  .literal.*  .text  .text.*)
    *libesp_event.a:default_event_loop.*(.literal.esp_event_isr_post .text.esp_event_isr_post)
    *libesp_event.a:esp_event.*(.literal.esp_event_isr_post_to .text.esp_event_isr_post_to)
    *libesp_ringbuf.a:( .literal  .literal.*  .text  .text.*)
    *libesp_system.a:panic.*( .literal  .literal.*  .text  .text.*)
    *libesp_system.a:panic_handler.*( .literal  .literal.*  .text  .text.*)
    *libesp_system.a:reset_reason.*( .literal  .literal.*  .text  .text.*)
    *libesp_system.a:startup.*(.iram1.27.literal .iram1.26.literal .iram1.25 .iram1.27 .iram1.26)
    *libesp_system.a:startup.*(.iram1.do_core_init)
    *libesp_system.a:startup.*(.iram1.do_secondary_init)
    *libesp_system.a:startup.*(.iram1.start_cpu0_default)
    *libesp_system.a:system_api.*(.literal.esp_system_abort .text.esp_system_abort)
    *libfreertos.a:( .literal EXCLUDE_FILE(*libfreertos.a:port.* *libfreertos.a:queue.*) .literal.*  .text EXCLUDE_FILE(*libfreertos.a:port.* *libfreertos.a:queue.*) .text.*)
    *libfreertos.a:port.*(.iram1.27.literal .iram1.28.literal .iram1.27 .iram1.28 .literal.pxPortInitialiseStack .literal.unlikely.vPortEndScheduler .literal.xPortStartScheduler .literal.vPortYieldOtherCore .literal.vPortReleaseTaskMPUSettings .literal.xPortInIsrContext .literal.xPortSysTickHandler .literal.vPortAssertIfInISR .literal.vPortSetStackWatchpoint .literal.vPortEnterCritical .literal.vPortExitCritical .literal.vApplicationStackOverflowHook .text.pxPortInitialiseStack .text.unlikely.vPortEndScheduler .text.xPortStartScheduler .text.vPortYieldOtherCore .text.vPortStoreTaskMPUSettings .text.vPortReleaseTaskMPUSettings .text.xPortInIsrContext .text.xPortSysTickHandler .text.vPortAssertIfInISR .text.vPortSetStackWatchpoint .text.xPortGetTickRateHz .text.vPortEnterCritical .text.vPortExitCritical .text.vApplicationStackOverflowHook)
    *libfreertos.a:queue.*( .iram1.* .literal.prvCopyDataToQueue .literal.prvNotifyQueueSetContainer .literal.prvCopyDataFromQueue .literal.prvUnlockQueue .literal.xQueueGenericReset .literal.xQueueGenericCreate .literal.xQueueGetMutexHolder .literal.xQueueGetMutexHolderFromISR .literal.xQueueCreateCountingSemaphoreStatic .literal.xQueueCreateCountingSemaphore .literal.xQueueGenericSend .literal.xQueueCreateMutexStatic .literal.xQueueGiveMutexRecursive .literal.xQueueCreateMutex .literal.xQueueGenericSendFromISR .literal.xQueueGiveFromISR .literal.xQueueReceive .literal.xQueueSemaphoreTake .literal.xQueueTakeMutexRecursive .literal.xQueuePeek .literal.xQueueReceiveFromISR .literal.xQueuePeekFromISR .literal.uxQueueMessagesWaiting .literal.uxQueueSpacesAvailable .literal.uxQueueMessagesWaitingFromISR .literal.vQueueDelete .literal.xQueueIsQueueEmptyFromISR .literal.xQueueIsQueueFullFromISR .literal.vQueueWaitForMessageRestricted .literal.xQueueCreateSet .literal.xQueueAddToSet .literal.xQueueRemoveFromSet .literal.xQueueSelectFromSet .literal.xQueueSelectFromSetFromISR .text.prvCopyDataToQueue .text.prvNotifyQueueSetContainer .text.prvCopyDataFromQueue .text.prvUnlockQueue .text.xQueueGenericReset .text.xQueueGenericCreate .text.xQueueGetMutexHolder .text.xQueueGetMutexHolderFromISR .text.xQueueCreateCountingSemaphoreStatic .text.xQueueCreateCountingSemaphore .text.xQueueGenericSend .text.xQueueCreateMutexStatic .text.xQueueGiveMutexRecursive .text.xQueueCreateMutex .text.xQueueGenericSendFromISR .text.xQueueGiveFromISR .text.xQueueReceive .text.xQueueSemaphoreTake .text.xQueueTakeMutexRecursive .text.xQueuePeek .text.xQueueReceiveFromISR .text.xQueuePeekFromISR .text.uxQueueMessagesWaiting .text.uxQueueSpacesAvailable .text.uxQueueMessagesWaitingFromISR .text.vQueueDelete .text.xQueueIsQueueEmptyFromISR .text.xQueueIsQueueFullFromISR .text.vQueueWaitForMessageRestricted .text.xQueueCreateSet .text.xQueueAddToSet .text.xQueueRemoveFromSet .text.xQueueSelectFromSet .text.xQueueSelectFromSetFromISR)
    *libfreertos.a:port.*(.iram1.esp_startup_start_app)
    *libfreertos.a:port.*(.iram1.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.iram1.main_task)
    *libfreertos.a:queue.*(.iram1.xQueueGenericCreateStatic)
    *libgcc.a:_divsf3.*( .literal  .literal.*  .text  .text.*)
    *libgcc.a:lib2funcs.*( .literal  .literal.*  .text  .text.*)
    *libgcov.a:( .literal  .literal.*  .text  .text.*)
    *libhal.a:cpu_hal.*( .literal  .literal.*  .text  .text.*)
    *libhal.a:i2c_hal_iram.*( .literal  .literal.*  .text  .text.*)
    *libhal.a:ledc_hal_iram.*( .literal  .literal.*  .text  .text.*)
    *libhal.a:soc_hal.*( .literal  .literal.*  .text  .text.*)
    *libhal.a:spi_flash_hal_gpspi.*( .literal  .literal.*  .text  .text.*)
    *libhal.a:spi_flash_hal_iram.*( .literal  .literal.*  .text  .text.*)
    *libhal.a:spi_hal_iram.*( .literal  .literal.*  .text  .text.*)
    *libhal.a:spi_slave_hal_iram.*( .literal  .literal.*  .text  .text.*)
    *libhal.a:systimer_hal.*( .literal  .literal.*  .text  .text.*)
    *libhal.a:twai_hal_iram.*( .iram1  .iram1.*)
    *libhal.a:uart_hal_iram.*( .iram1  .iram1.*)
    *libhal.a:wdt_hal_iram.*( .literal  .literal.*  .text  .text.*)
    *libheap.a:heap_tlsf.*( .literal  .literal.*  .text  .text.*)
    *libheap.a:multi_heap.*( .literal  .literal.*  .text  .text.*)
    *libheap.a:multi_heap_poisoning.*( .literal  .literal.*  .text  .text.*)
    *liblog.a:log.*(.literal.esp_log_write .text.esp_log_write)
    *liblog.a:log_freertos.*(.literal.esp_log_early_timestamp .text.esp_log_early_timestamp)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_lock .text.esp_log_impl_lock)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_lock_timeout .text.esp_log_impl_lock_timeout)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_unlock .text.esp_log_impl_unlock)
    *liblog.a:log_freertos.*(.literal.esp_log_timestamp .text.esp_log_timestamp)
    *libnet80211.a:( .wifi0iram  .wifi0iram.*)
    *libnewlib.a:abort.*( .literal  .literal.*  .text  .text.*)
    *libnewlib.a:heap.*( .literal  .literal.*  .text  .text.*)
    *libpp.a:( .wifi0iram  .wifi0iram.*)
    *librtc.a:( .literal  .literal.*  .text  .text.*)
    *libsoc.a:cpu_util.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:lldesc.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_clk.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_periph.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_pm.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_sleep.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_time.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_wdt.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_init.*(.literal.rtc_vddsdio_set_config .text.rtc_vddsdio_set_config)
    *libspi_flash.a:memspi_host_driver.*( .literal  .literal.*  .text  .text.*)
    *libspi_flash.a:spi_flash_chip_gd.*( .literal  .literal.*  .text  .text.*)
    *libspi_flash.a:spi_flash_chip_generic.*( .literal  .literal.*  .text  .text.*)
    *libspi_flash.a:spi_flash_chip_issi.*( .literal  .literal.*  .text  .text.*)
    *libspi_flash.a:spi_flash_chip_mxic.*( .literal  .literal.*  .text  .text.*)
    *libspi_flash.a:spi_flash_rom_patch.*( .literal  .literal.*  .text  .text.*)
    *libxt_hal.a:( .literal  .literal.*  .text  .text.*)
    *libxtensa.a:eri.*( .literal  .literal.*  .text  .text.*)
    *libxtensa.a:stdatomic.*( .literal  .literal.*  .text  .text.*)

  } > iram0_0_seg

  .dram0.data :
  {
    _data_start = ABSOLUTE(.);
    _bt_data_start = ABSOLUTE(.);
    *libbt.a:(.data .data.*)
    . = ALIGN (4);
    _bt_data_end = ABSOLUTE(.);
    _btdm_data_start = ABSOLUTE(.);
    *libbtdm_app.a:(.data .data.*)
    . = ALIGN (4);
    _btdm_data_end = ABSOLUTE(.);
    _nimble_data_start = ABSOLUTE(.);
    *libnimble.a:(.data .data.*)
    . = ALIGN (4);
    _nimble_data_end = ABSOLUTE(.);
    *(.gnu.linkonce.d.*)
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    *(.jcr)

    /* coredump mapping */
    _coredump_dram_start = ABSOLUTE(.);
    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .dram1.coredump EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:port.* *libfreertos.a:queue.*) .dram1.coredump.*)
    *libesp_system.a:startup.*( .dram1.coredump.*)
    *libesp_system.a:startup.*(.dram1.coredump.do_core_init)
    *libesp_system.a:startup.*(.dram1.coredump.do_secondary_init)
    *libesp_system.a:startup.*(.dram1.coredump.start_cpu0_default)
    *libfreertos.a:port.*( .dram1.coredump.*)
    *libfreertos.a:queue.*( .dram1.coredump.*)
    *libfreertos.a:port.*(.dram1.coredump.esp_startup_start_app)
    *libfreertos.a:port.*(.dram1.coredump.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.dram1.coredump.main_task)
    *libfreertos.a:queue.*(.dram1.coredump.xQueueGenericCreateStatic)
    *libhal.a:twai_hal_iram.*( .dram1.coredump  .dram1.coredump.*)
    *libhal.a:uart_hal_iram.*( .dram1.coredump  .dram1.coredump.*)
    _coredump_dram_end = ABSOLUTE(.);
    
    /* should be placed after coredump mapping */
    _esp_system_init_fn_array_start = ABSOLUTE(.);
    KEEP (*(SORT(.esp_system_init_fn) SORT(.esp_system_init_fn.*)))
    _esp_system_init_fn_array_end = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .data EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:queue.* *libfreertos.a:port.*) .data.* EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .dram1 EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:queue.* *libfreertos.a:port.*) .dram1.*)
    *libapp_trace.a:SEGGER_RTT_esp32.*( .rodata  .rodata.*)
    *libapp_trace.a:SEGGER_SYSVIEW.*( .rodata  .rodata.*)
    *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.*( .rodata  .rodata.*)
    *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.*( .rodata  .rodata.*)
    *libapp_trace.a:app_trace.*( .rodata  .rodata.*)
    *libapp_trace.a:app_trace_util.*( .rodata  .rodata.*)
    *libc.a:creat.*( .rodata  .rodata.*)
    *libc.a:isatty.*( .rodata  .rodata.*)
    *libc.a:lib_a-abs.*( .rodata  .rodata.*)
    *libc.a:lib_a-asctime.*( .rodata  .rodata.*)
    *libc.a:lib_a-asctime_r.*( .rodata  .rodata.*)
    *libc.a:lib_a-atoi.*( .rodata  .rodata.*)
    *libc.a:lib_a-atol.*( .rodata  .rodata.*)
    *libc.a:lib_a-bzero.*( .rodata  .rodata.*)
    *libc.a:lib_a-close.*( .rodata  .rodata.*)
    *libc.a:lib_a-creat.*( .rodata  .rodata.*)
    *libc.a:lib_a-ctime.*( .rodata  .rodata.*)
    *libc.a:lib_a-ctime_r.*( .rodata  .rodata.*)
    *libc.a:lib_a-ctype_.*( .rodata  .rodata.*)
    *libc.a:lib_a-div.*( .rodata  .rodata.*)
    *libc.a:lib_a-environ.*( .rodata  .rodata.*)
    *libc.a:lib_a-envlock.*( .rodata  .rodata.*)
    *libc.a:lib_a-fclose.*( .rodata  .rodata.*)
    *libc.a:lib_a-fflush.*( .rodata  .rodata.*)
    *libc.a:lib_a-findfp.*( .rodata  .rodata.*)
    *libc.a:lib_a-fputwc.*( .rodata  .rodata.*)
    *libc.a:lib_a-fvwrite.*( .rodata  .rodata.*)
    *libc.a:lib_a-fwalk.*( .rodata  .rodata.*)
    *libc.a:lib_a-getenv_r.*( .rodata  .rodata.*)
    *libc.a:lib_a-gettzinfo.*( .rodata  .rodata.*)
    *libc.a:lib_a-gmtime.*( .rodata  .rodata.*)
    *libc.a:lib_a-gmtime_r.*( .rodata  .rodata.*)
    *libc.a:lib_a-impure.*( .rodata  .rodata.*)
    *libc.a:lib_a-isalnum.*( .rodata  .rodata.*)
    *libc.a:lib_a-isalpha.*( .rodata  .rodata.*)
    *libc.a:lib_a-isascii.*( .rodata  .rodata.*)
    *libc.a:lib_a-isblank.*( .rodata  .rodata.*)
    *libc.a:lib_a-iscntrl.*( .rodata  .rodata.*)
    *libc.a:lib_a-isdigit.*( .rodata  .rodata.*)
    *libc.a:lib_a-isgraph.*( .rodata  .rodata.*)
    *libc.a:lib_a-islower.*( .rodata  .rodata.*)
    *libc.a:lib_a-isprint.*( .rodata  .rodata.*)
    *libc.a:lib_a-ispunct.*( .rodata  .rodata.*)
    *libc.a:lib_a-isspace.*( .rodata  .rodata.*)
    *libc.a:lib_a-isupper.*( .rodata  .rodata.*)
    *libc.a:lib_a-itoa.*( .rodata  .rodata.*)
    *libc.a:lib_a-labs.*( .rodata  .rodata.*)
    *libc.a:lib_a-lcltime.*( .rodata  .rodata.*)
    *libc.a:lib_a-lcltime_r.*( .rodata  .rodata.*)
    *libc.a:lib_a-ldiv.*( .rodata  .rodata.*)
    *libc.a:lib_a-longjmp.*( .rodata  .rodata.*)
    *libc.a:lib_a-makebuf.*( .rodata  .rodata.*)
    *libc.a:lib_a-memccpy.*( .rodata  .rodata.*)
    *libc.a:lib_a-memchr.*( .rodata  .rodata.*)
    *libc.a:lib_a-memcmp.*( .rodata  .rodata.*)
    *libc.a:lib_a-memcpy.*( .rodata  .rodata.*)
    *libc.a:lib_a-memmove.*( .rodata  .rodata.*)
    *libc.a:lib_a-memrchr.*( .rodata  .rodata.*)
    *libc.a:lib_a-memset.*( .rodata  .rodata.*)
    *libc.a:lib_a-mktime.*( .rodata  .rodata.*)
    *libc.a:lib_a-month_lengths.*( .rodata  .rodata.*)
    *libc.a:lib_a-open.*( .rodata  .rodata.*)
    *libc.a:lib_a-quorem.*( .rodata  .rodata.*)
    *libc.a:lib_a-raise.*( .rodata  .rodata.*)
    *libc.a:lib_a-rand.*( .rodata  .rodata.*)
    *libc.a:lib_a-rand_r.*( .rodata  .rodata.*)
    *libc.a:lib_a-read.*( .rodata  .rodata.*)
    *libc.a:lib_a-refill.*( .rodata  .rodata.*)
    *libc.a:lib_a-rshift.*( .rodata  .rodata.*)
    *libc.a:lib_a-s_fpclassify.*( .rodata  .rodata.*)
    *libc.a:lib_a-sbrk.*( .rodata  .rodata.*)
    *libc.a:lib_a-sccl.*( .rodata  .rodata.*)
    *libc.a:lib_a-setjmp.*( .rodata  .rodata.*)
    *libc.a:lib_a-sf_nan.*( .rodata  .rodata.*)
    *libc.a:lib_a-srand.*( .rodata  .rodata.*)
    *libc.a:lib_a-stdio.*( .rodata  .rodata.*)
    *libc.a:lib_a-strcasecmp.*( .rodata  .rodata.*)
    *libc.a:lib_a-strcasestr.*( .rodata  .rodata.*)
    *libc.a:lib_a-strcat.*( .rodata  .rodata.*)
    *libc.a:lib_a-strchr.*( .rodata  .rodata.*)
    *libc.a:lib_a-strcmp.*( .rodata  .rodata.*)
    *libc.a:lib_a-strcoll.*( .rodata  .rodata.*)
    *libc.a:lib_a-strcpy.*( .rodata  .rodata.*)
    *libc.a:lib_a-strcspn.*( .rodata  .rodata.*)
    *libc.a:lib_a-strdup.*( .rodata  .rodata.*)
    *libc.a:lib_a-strdup_r.*( .rodata  .rodata.*)
    *libc.a:lib_a-strftime.*( .rodata  .rodata.*)
    *libc.a:lib_a-strlcat.*( .rodata  .rodata.*)
    *libc.a:lib_a-strlcpy.*( .rodata  .rodata.*)
    *libc.a:lib_a-strlen.*( .rodata  .rodata.*)
    *libc.a:lib_a-strlwr.*( .rodata  .rodata.*)
    *libc.a:lib_a-strncasecmp.*( .rodata  .rodata.*)
    *libc.a:lib_a-strncat.*( .rodata  .rodata.*)
    *libc.a:lib_a-strncmp.*( .rodata  .rodata.*)
    *libc.a:lib_a-strncpy.*( .rodata  .rodata.*)
    *libc.a:lib_a-strndup.*( .rodata  .rodata.*)
    *libc.a:lib_a-strndup_r.*( .rodata  .rodata.*)
    *libc.a:lib_a-strnlen.*( .rodata  .rodata.*)
    *libc.a:lib_a-strptime.*( .rodata  .rodata.*)
    *libc.a:lib_a-strrchr.*( .rodata  .rodata.*)
    *libc.a:lib_a-strsep.*( .rodata  .rodata.*)
    *libc.a:lib_a-strspn.*( .rodata  .rodata.*)
    *libc.a:lib_a-strstr.*( .rodata  .rodata.*)
    *libc.a:lib_a-strtok_r.*( .rodata  .rodata.*)
    *libc.a:lib_a-strtol.*( .rodata  .rodata.*)
    *libc.a:lib_a-strtoul.*( .rodata  .rodata.*)
    *libc.a:lib_a-strupr.*( .rodata  .rodata.*)
    *libc.a:lib_a-sysclose.*( .rodata  .rodata.*)
    *libc.a:lib_a-sysopen.*( .rodata  .rodata.*)
    *libc.a:lib_a-sysread.*( .rodata  .rodata.*)
    *libc.a:lib_a-syssbrk.*( .rodata  .rodata.*)
    *libc.a:lib_a-system.*( .rodata  .rodata.*)
    *libc.a:lib_a-systimes.*( .rodata  .rodata.*)
    *libc.a:lib_a-syswrite.*( .rodata  .rodata.*)
    *libc.a:lib_a-time.*( .rodata  .rodata.*)
    *libc.a:lib_a-timelocal.*( .rodata  .rodata.*)
    *libc.a:lib_a-toascii.*( .rodata  .rodata.*)
    *libc.a:lib_a-tolower.*( .rodata  .rodata.*)
    *libc.a:lib_a-toupper.*( .rodata  .rodata.*)
    *libc.a:lib_a-tzcalc_limits.*( .rodata  .rodata.*)
    *libc.a:lib_a-tzlock.*( .rodata  .rodata.*)
    *libc.a:lib_a-tzset.*( .rodata  .rodata.*)
    *libc.a:lib_a-tzset_r.*( .rodata  .rodata.*)
    *libc.a:lib_a-tzvars.*( .rodata  .rodata.*)
    *libc.a:lib_a-ungetc.*( .rodata  .rodata.*)
    *libc.a:lib_a-utoa.*( .rodata  .rodata.*)
    *libc.a:lib_a-wbuf.*( .rodata  .rodata.*)
    *libc.a:lib_a-wcrtomb.*( .rodata  .rodata.*)
    *libc.a:lib_a-wctomb_r.*( .rodata  .rodata.*)
    *libc.a:lib_a-wsetup.*( .rodata  .rodata.*)
    *libc.a:lock.*( .rodata  .rodata.*)
    *libesp_common.a:esp_err.*( .rodata  .rodata.*)
    *libesp_event.a:default_event_loop.*(.rodata.esp_event_isr_post)
    *libesp_event.a:esp_event.*(.rodata.esp_event_isr_post_to)
    *libesp_system.a:panic.*( .rodata  .rodata.*)
    *libesp_system.a:panic_handler.*( .rodata  .rodata.*)
    *libesp_system.a:reset_reason.*( .rodata  .rodata.*)
    *libesp_system.a:startup.*(.data.g_startup_fn  .dram1.*)
    *libesp_system.a:startup.*(.data.do_core_init .dram1.do_core_init)
    *libesp_system.a:startup.*(.data.do_secondary_init .dram1.do_secondary_init)
    *libesp_system.a:startup.*(.data.start_cpu0_default .dram1.start_cpu0_default)
    *libesp_system.a:system_api.*(.rodata.esp_system_abort)
    *libfreertos.a:port.*( .data.*  .dram1.*)
    *libfreertos.a:queue.*( .data.*  .dram1.*)
    *libfreertos.a:port.*(.data.esp_startup_start_app .dram1.esp_startup_start_app)
    *libfreertos.a:port.*(.data.esp_startup_start_app_other_cores .dram1.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.data.main_task .dram1.main_task)
    *libfreertos.a:queue.*(.data.xQueueGenericCreateStatic .dram1.xQueueGenericCreateStatic)
    *libgcc.a:_divsf3.*( .rodata  .rodata.*)
    *libgcov.a:( .rodata  .rodata.*)
    *libhal.a:cpu_hal.*( .rodata  .rodata.*)
    *libhal.a:i2c_hal_iram.*( .rodata  .rodata.*)
    *libhal.a:ledc_hal_iram.*( .rodata  .rodata.*)
    *libhal.a:soc_hal.*( .rodata  .rodata.*)
    *libhal.a:spi_flash_hal_gpspi.*( .rodata  .rodata.*)
    *libhal.a:spi_flash_hal_iram.*( .rodata  .rodata.*)
    *libhal.a:spi_hal_iram.*( .rodata  .rodata.*)
    *libhal.a:spi_slave_hal_iram.*( .rodata  .rodata.*)
    *libhal.a:systimer_hal.*( .rodata  .rodata.*)
    *libhal.a:twai_hal_iram.*( .data  .data.*  .dram1  .dram1.*)
    *libhal.a:uart_hal_iram.*( .data  .data.*  .dram1  .dram1.*)
    *libhal.a:wdt_hal_iram.*( .rodata  .rodata.*)
    *libheap.a:heap_tlsf.*( .rodata  .rodata.*)
    *libheap.a:multi_heap.*( .rodata  .rodata.*)
    *libheap.a:multi_heap_poisoning.*( .rodata  .rodata.*)
    *liblog.a:log.*(.rodata.esp_log_write)
    *liblog.a:log_freertos.*(.rodata.esp_log_early_timestamp)
    *liblog.a:log_freertos.*(.rodata.esp_log_impl_lock)
    *liblog.a:log_freertos.*(.rodata.esp_log_impl_lock_timeout)
    *liblog.a:log_freertos.*(.rodata.esp_log_impl_unlock)
    *liblog.a:log_freertos.*(.rodata.esp_log_timestamp)
    *libnewlib.a:abort.*( .rodata  .rodata.*)
    *libnewlib.a:heap.*( .rodata  .rodata.*)
    *libphy.a:( .rodata  .rodata.*)
    *libsoc.a:lldesc.*( .rodata  .rodata.*)
    *libsoc.a:rtc_clk.*( .rodata  .rodata.*)
    *libsoc.a:rtc_init.*(.rodata.rtc_vddsdio_set_config)
    *libspi_flash.a:memspi_host_driver.*( .rodata  .rodata.*)
    *libspi_flash.a:spi_flash_chip_gd.*( .rodata  .rodata.*)
    *libspi_flash.a:spi_flash_chip_generic.*( .rodata  .rodata.*)
    *libspi_flash.a:spi_flash_chip_issi.*( .rodata  .rodata.*)
    *libspi_flash.a:spi_flash_chip_mxic.*( .rodata  .rodata.*)
    *libspi_flash.a:spi_flash_rom_patch.*( .rodata  .rodata.*)
    *libxtensa.a:stdatomic.*( .rodata  .rodata.*)

    _data_end = ABSOLUTE(.);
    . = ALIGN(4);
  } > dram0_0_seg

  /*This section holds data that should not be initialized at power up.
    The section located in Internal SRAM memory region. The macro _NOINIT
    can be used as attribute to place data into this section.
    See the esp_attr.h file for more information.
  */
  .noinit (NOLOAD):
  {
    . = ALIGN(4);
    _noinit_start = ABSOLUTE(.);
    *(.noinit .noinit.*)
    . = ALIGN(4) ;
    _noinit_end = ABSOLUTE(.);
  } > dram0_0_seg

  /* Shared RAM */
  .dram0.bss (NOLOAD) :
  {
    . = ALIGN (8);
    _bss_start = ABSOLUTE(.);
    *(.ext_ram.bss*)
    _bt_bss_start = ABSOLUTE(.);
    *libbt.a:(.bss .bss.* COMMON)
    . = ALIGN (4);
    _bt_bss_end = ABSOLUTE(.);
    _btdm_bss_start = ABSOLUTE(.);
    *libbtdm_app.a:(.bss .bss.* COMMON)
    . = ALIGN (4);
    _btdm_bss_end = ABSOLUTE(.);
    _nimble_bss_start = ABSOLUTE(.);
    *libnimble.a:(.bss .bss.* COMMON)
    . = ALIGN (4);
    _nimble_bss_end = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .bss EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:port.* *libfreertos.a:queue.*) .bss.* EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) COMMON)
    *libesp_system.a:startup.*(.bss.ob$11551 .bss.s_system_full_inited .bss.s_system_inited .bss.g_startup_time)
    *libesp_system.a:startup.*(.bss.do_core_init)
    *libesp_system.a:startup.*(.bss.do_secondary_init)
    *libesp_system.a:startup.*(.bss.start_cpu0_default)
    *libfreertos.a:port.*(.bss.port_uxOldInterruptState .bss.port_uxCriticalNesting .bss.port_interruptNesting .bss.port_xSchedulerRunning)
    *libfreertos.a:queue.*( .bss.*)
    *libfreertos.a:port.*(.bss.esp_startup_start_app)
    *libfreertos.a:port.*(.bss.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.bss.main_task)
    *libfreertos.a:queue.*(.bss.xQueueGenericCreateStatic)
    *libhal.a:twai_hal_iram.*( .bss  .bss.*  COMMON)
    *libhal.a:uart_hal_iram.*( .bss  .bss.*  COMMON)

    *(.dynsbss)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    *(.share.mem)
    *(.gnu.linkonce.b.*)

    . = ALIGN (8);
    _bss_end = ABSOLUTE(.);
  } > dram0_0_seg

  ASSERT(((_bss_end - ORIGIN(dram0_0_seg)) <= LENGTH(dram0_0_seg)),
          "DRAM segment data does not fit.")

  /* When modifying the alignment, update tls_section_alignment in pxPortInitialiseStack */
  .flash.rodata : ALIGN(0x10)
  {
    _rodata_start = ABSOLUTE(.);

    *(.rodata_desc .rodata_desc.*)               /* Should be the first.  App version info.        DO NOT PUT ANYTHING BEFORE IT! */
    *(.rodata_custom_desc .rodata_custom_desc.*) /* Should be the second. Custom app version info. DO NOT PUT ANYTHING BEFORE IT! */

    *(EXCLUDE_FILE(*libgcov.a *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libgcc.a:_divsf3.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-isblank.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-time.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-tzset.* *libc.a:lib_a-raise.* *libc.a:lib_a-sysread.* *libc.a:lib_a-systimes.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strftime.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-close.* *libc.a:lib_a-strupr.* *libc.a:lib_a-bzero.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-memchr.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isupper.* *libc.a:lock.* *libc.a:lib_a-itoa.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-fclose.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-open.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-syswrite.* *libc.a:creat.* *libc.a:lib_a-tolower.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-abs.* *libc.a:lib_a-system.* *libc.a:lib_a-strcspn.* *libc.a:isatty.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-refill.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-toascii.* *libc.a:lib_a-ctime.* *libc.a:lib_a-strspn.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strtol.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-isprint.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strdup.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-rand.* *libc.a:lib_a-strncat.* *libc.a:lib_a-creat.* *libc.a:lib_a-read.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-fflush.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-toupper.* *libc.a:lib_a-quorem.* *libc.a:lib_a-div.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-labs.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-mktime.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-stdio.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-wsetup.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-islower.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-environ.* *libc.a:lib_a-sccl.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-memmove.* *libc.a:lib_a-rshift.* *libc.a:lib_a-envlock.* *libc.a:lib_a-strlen.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-strptime.* *libc.a:lib_a-findfp.* *libc.a:lib_a-impure.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-utoa.* *libc.a:lib_a-srand.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-asctime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-atoi.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-memset.* *libc.a:lib_a-atol.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libheap.a:heap_tlsf.* *libesp_system.a:reset_reason.* *libesp_system.a:panic_handler.* *libesp_system.a:panic.* *libesp_common.a:esp_err.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_rom_patch.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_generic.* *libsoc.a:rtc_clk.* *libsoc.a:lldesc.* *libxtensa.a:stdatomic.* *libnewlib.a:heap.* *libnewlib.a:abort.* *libhal.a:spi_flash_hal_iram.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libhal.a:i2c_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_hal_gpspi.* *libhal.a:cpu_hal.* *libhal.a:ledc_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:systimer_hal.* *libhal.a:wdt_hal_iram.* *libhal.a:spi_hal_iram.* *libphy.a) .rodata EXCLUDE_FILE(*libgcov.a *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *liblog.a:log_freertos.* *liblog.a:log.* *libgcc.a:_divsf3.* *libesp_event.a:esp_event.* *libesp_event.a:default_event_loop.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-isblank.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-time.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-tzset.* *libc.a:lib_a-raise.* *libc.a:lib_a-sysread.* *libc.a:lib_a-systimes.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strftime.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-close.* *libc.a:lib_a-strupr.* *libc.a:lib_a-bzero.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-memchr.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isupper.* *libc.a:lock.* *libc.a:lib_a-itoa.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-fclose.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-open.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-syswrite.* *libc.a:creat.* *libc.a:lib_a-tolower.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-abs.* *libc.a:lib_a-system.* *libc.a:lib_a-strcspn.* *libc.a:isatty.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-refill.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-toascii.* *libc.a:lib_a-ctime.* *libc.a:lib_a-strspn.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strtol.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-isprint.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strdup.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-rand.* *libc.a:lib_a-strncat.* *libc.a:lib_a-creat.* *libc.a:lib_a-read.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-fflush.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-toupper.* *libc.a:lib_a-quorem.* *libc.a:lib_a-div.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-labs.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-mktime.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-stdio.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-wsetup.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-islower.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-environ.* *libc.a:lib_a-sccl.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-memmove.* *libc.a:lib_a-rshift.* *libc.a:lib_a-envlock.* *libc.a:lib_a-strlen.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-strptime.* *libc.a:lib_a-findfp.* *libc.a:lib_a-impure.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-utoa.* *libc.a:lib_a-srand.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-asctime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-atoi.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-memset.* *libc.a:lib_a-atol.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libheap.a:heap_tlsf.* *libesp_system.a:startup.* *libesp_system.a:system_api.* *libesp_system.a:reset_reason.* *libesp_system.a:panic_handler.* *libesp_system.a:panic.* *libesp_common.a:esp_err.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_rom_patch.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_generic.* *libsoc.a:rtc_init.* *libsoc.a:rtc_clk.* *libsoc.a:lldesc.* *libxtensa.a:stdatomic.* *libnewlib.a:heap.* *libnewlib.a:abort.* *libhal.a:spi_flash_hal_iram.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libhal.a:i2c_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_hal_gpspi.* *libhal.a:cpu_hal.* *libhal.a:ledc_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:systimer_hal.* *libhal.a:wdt_hal_iram.* *libhal.a:spi_hal_iram.* *libfreertos.a:queue.* *libfreertos.a:port.* *libphy.a) .rodata.*)
    *libesp_event.a:default_event_loop.*(.rodata.esp_event_loop_create_default.str1.4 .rodata.esp_event_send_to_default_loop)
    *libesp_event.a:esp_event.*(.rodata.base_node_add_handler.str1.4 .rodata.loop_node_add_handler.str1.4 .rodata.esp_event_loop_create.str1.4 .rodata.esp_event_loop_run.str1.4 .rodata.esp_event_loop_run_task.str1.4 .rodata.esp_event_handler_register_with_internal.str1.4 .rodata.esp_event_handler_unregister_with_internal.str1.4 .rodata.__func__$9170 .rodata.__func__$9157 .rodata.__func__$9124 .rodata.__func__$9092 .rodata.__func__$9067 .rodata.__func__$9026 .rodata.__func__$9017)
    *libesp_system.a:startup.*(.rodata.start_cpu0_default.str1.4 .rodata.__func__$11587)
    *libesp_system.a:system_api.*(.rodata.esp_get_idf_version.str1.4)
    *libesp_system.a:startup.*(.rodata.do_core_init)
    *libesp_system.a:startup.*(.rodata.do_secondary_init)
    *libesp_system.a:startup.*(.rodata.start_cpu0_default)
    *libfreertos.a:port.*(.rodata.main_task.str1.4 .rodata.vPortAssertIfInISR.str1.4 .rodata.vPortEnterCritical.str1.4 .rodata.vPortExitCritical.str1.4 .rodata.vApplicationStackOverflowHook.str1.4 .rodata.esp_startup_start_app.str1.4 .rodata.__func__$5604 .rodata.__func__$5617 .rodata.__func__$4432 .rodata.__func__$4419 .rodata.__FUNCTION__$5566)
    *libfreertos.a:queue.*(.rodata.prvNotifyQueueSetContainer.str1.4 .rodata.__FUNCTION__$5560 .rodata.__FUNCTION__$5550 .rodata.__FUNCTION__$5513 .rodata.__FUNCTION__$5508 .rodata.__FUNCTION__$5502 .rodata.__FUNCTION__$5496 .rodata.__FUNCTION__$5490 .rodata.__FUNCTION__$5479 .rodata.__FUNCTION__$5468 .rodata.__FUNCTION__$5455 .rodata.__FUNCTION__$5444 .rodata.__FUNCTION__$5433 .rodata.__FUNCTION__$5424 .rodata.__FUNCTION__$5597 .rodata.__FUNCTION__$5412 .rodata.__FUNCTION__$5401 .rodata.__FUNCTION__$5395 .rodata.__FUNCTION__$5388 .rodata.__FUNCTION__$5381 .rodata.__FUNCTION__$5375 .rodata.__FUNCTION__$5342 .rodata.__FUNCTION__$5332 .rodata.__FUNCTION__$5323)
    *libfreertos.a:port.*(.rodata.esp_startup_start_app)
    *libfreertos.a:port.*(.rodata.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.rodata.main_task)
    *libfreertos.a:queue.*(.rodata.xQueueGenericCreateStatic)
    *libhal.a:twai_hal_iram.*( .rodata  .rodata.*)
    *libhal.a:uart_hal_iram.*( .rodata  .rodata.*)
    *liblog.a:log.*(.rodata.esp_log_level_set.str1.4 .rodata.__func__$3544 .rodata.__func__$3515)
    *liblog.a:log_freertos.*(.rodata.esp_log_system_timestamp.str1.4)
    *libsoc.a:rtc_init.*( .rodata.*)


    *(.irom1.text) /* catch stray ICACHE_RODATA_ATTR */
    *(.gnu.linkonce.r.*)
    *(.rodata1)
    __XT_EXCEPTION_TABLE_ = ABSOLUTE(.);
    *(.xt_except_table)
    *(.gcc_except_table .gcc_except_table.*)
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    . = (. + 3) & ~ 3;
    __eh_frame = ABSOLUTE(.);
    KEEP(*(.eh_frame))
    . = (. + 7) & ~ 3;
    /*  C++ constructor and destructor tables

        Make a point of not including anything from crtbegin.o or crtend.o, as IDF doesn't use toolchain crt
      */
    __init_array_start = ABSOLUTE(.);
    KEEP (*(EXCLUDE_FILE (*crtend.* *crtbegin.*) .ctors SORT(.ctors.*)))
    __init_array_end = ABSOLUTE(.);

    KEEP (*crtbegin.*(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.*) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    /*  C++ exception handlers table:  */
    __XT_EXCEPTION_DESCS_ = ABSOLUTE(.);
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
    *(.xt_except_desc_end)
    *(.dynamic)
    *(.gnu.version_d)
    /* Addresses of memory regions reserved via
       SOC_RESERVE_MEMORY_REGION() */
    soc_reserved_memory_region_start = ABSOLUTE(.);
    KEEP (*(.reserved_memory_address))
    soc_reserved_memory_region_end = ABSOLUTE(.);
    _rodata_end = ABSOLUTE(.);
    /* Literals are also RO data. */
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
    . = ALIGN(4);
    _thread_local_start = ABSOLUTE(.);
    *(.tdata)
    *(.tdata.*)
    *(.tbss)
    *(.tbss.*)
    _thread_local_end = ABSOLUTE(.);
    . = ALIGN(4);
  } >default_rodata_seg

  .flash.text :
  {
    _stext = .;
    _text_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libesp_ringbuf.a *libgcov.a *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libgcc.a:lib2funcs.* *libgcc.a:_divsf3.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-isblank.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-time.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-tzset.* *libc.a:lib_a-raise.* *libc.a:lib_a-sysread.* *libc.a:lib_a-systimes.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strftime.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-close.* *libc.a:lib_a-strupr.* *libc.a:lib_a-bzero.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-memchr.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isupper.* *libc.a:lock.* *libc.a:lib_a-itoa.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-fclose.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-open.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-syswrite.* *libc.a:creat.* *libc.a:lib_a-tolower.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-abs.* *libc.a:lib_a-system.* *libc.a:lib_a-strcspn.* *libc.a:isatty.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-refill.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-toascii.* *libc.a:lib_a-ctime.* *libc.a:lib_a-strspn.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strtol.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-isprint.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strdup.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-rand.* *libc.a:lib_a-strncat.* *libc.a:lib_a-creat.* *libc.a:lib_a-read.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-fflush.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-toupper.* *libc.a:lib_a-quorem.* *libc.a:lib_a-div.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-labs.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-mktime.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-stdio.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-wsetup.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-islower.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-environ.* *libc.a:lib_a-sccl.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-memmove.* *libc.a:lib_a-rshift.* *libc.a:lib_a-envlock.* *libc.a:lib_a-strlen.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-strptime.* *libc.a:lib_a-findfp.* *libc.a:lib_a-impure.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-utoa.* *libc.a:lib_a-srand.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-asctime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-atoi.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-memset.* *libc.a:lib_a-atol.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libheap.a:heap_tlsf.* *libxt_hal.a *libesp_system.a:reset_reason.* *libesp_system.a:panic_handler.* *libesp_system.a:panic.* *libesp_common.a:esp_err.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_rom_patch.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_generic.* *librtc.a *libsoc.a:cpu_util.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_pm.* *libsoc.a:rtc_clk.* *libsoc.a:lldesc.* *libsoc.a:rtc_wdt.* *libsoc.a:rtc_time.* *libsoc.a:rtc_periph.* *libxtensa.a:stdatomic.* *libxtensa.a:eri.* *libnewlib.a:heap.* *libnewlib.a:abort.* *libhal.a:spi_flash_hal_iram.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libhal.a:i2c_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_hal_gpspi.* *libhal.a:cpu_hal.* *libhal.a:ledc_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:systimer_hal.* *libhal.a:wdt_hal_iram.* *libhal.a:spi_hal_iram.* *libfreertos.a) .literal EXCLUDE_FILE(*libesp_ringbuf.a *libgcov.a *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *liblog.a:log.* *liblog.a:log_freertos.* *libgcc.a:lib2funcs.* *libgcc.a:_divsf3.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-isblank.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-time.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-tzset.* *libc.a:lib_a-raise.* *libc.a:lib_a-sysread.* *libc.a:lib_a-systimes.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strftime.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-close.* *libc.a:lib_a-strupr.* *libc.a:lib_a-bzero.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-memchr.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isupper.* *libc.a:lock.* *libc.a:lib_a-itoa.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-fclose.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-open.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-syswrite.* *libc.a:creat.* *libc.a:lib_a-tolower.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-abs.* *libc.a:lib_a-system.* *libc.a:lib_a-strcspn.* *libc.a:isatty.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-refill.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-toascii.* *libc.a:lib_a-ctime.* *libc.a:lib_a-strspn.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strtol.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-isprint.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strdup.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-rand.* *libc.a:lib_a-strncat.* *libc.a:lib_a-creat.* *libc.a:lib_a-read.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-fflush.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-toupper.* *libc.a:lib_a-quorem.* *libc.a:lib_a-div.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-labs.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-mktime.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-stdio.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-wsetup.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-islower.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-environ.* *libc.a:lib_a-sccl.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-memmove.* *libc.a:lib_a-rshift.* *libc.a:lib_a-envlock.* *libc.a:lib_a-strlen.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-strptime.* *libc.a:lib_a-findfp.* *libc.a:lib_a-impure.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-utoa.* *libc.a:lib_a-srand.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-asctime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-atoi.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-memset.* *libc.a:lib_a-atol.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libheap.a:heap_tlsf.* *libxt_hal.a *libesp_system.a:startup.* *libesp_system.a:system_api.* *libesp_system.a:reset_reason.* *libesp_system.a:panic_handler.* *libesp_system.a:panic.* *libesp_common.a:esp_err.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_rom_patch.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_generic.* *librtc.a *libsoc.a:rtc_init.* *libsoc.a:cpu_util.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_pm.* *libsoc.a:rtc_clk.* *libsoc.a:lldesc.* *libsoc.a:rtc_wdt.* *libsoc.a:rtc_time.* *libsoc.a:rtc_periph.* *libxtensa.a:stdatomic.* *libxtensa.a:eri.* *libnewlib.a:heap.* *libnewlib.a:abort.* *libhal.a:spi_flash_hal_iram.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libhal.a:i2c_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_hal_gpspi.* *libhal.a:cpu_hal.* *libhal.a:ledc_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:systimer_hal.* *libhal.a:wdt_hal_iram.* *libhal.a:spi_hal_iram.* *libfreertos.a) .literal.* EXCLUDE_FILE(*libesp_ringbuf.a *libgcov.a *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libgcc.a:lib2funcs.* *libgcc.a:_divsf3.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-isblank.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-time.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-tzset.* *libc.a:lib_a-raise.* *libc.a:lib_a-sysread.* *libc.a:lib_a-systimes.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strftime.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-close.* *libc.a:lib_a-strupr.* *libc.a:lib_a-bzero.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-memchr.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isupper.* *libc.a:lock.* *libc.a:lib_a-itoa.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-fclose.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-open.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-syswrite.* *libc.a:creat.* *libc.a:lib_a-tolower.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-abs.* *libc.a:lib_a-system.* *libc.a:lib_a-strcspn.* *libc.a:isatty.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-refill.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-toascii.* *libc.a:lib_a-ctime.* *libc.a:lib_a-strspn.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strtol.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-isprint.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strdup.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-rand.* *libc.a:lib_a-strncat.* *libc.a:lib_a-creat.* *libc.a:lib_a-read.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-fflush.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-toupper.* *libc.a:lib_a-quorem.* *libc.a:lib_a-div.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-labs.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-mktime.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-stdio.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-wsetup.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-islower.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-environ.* *libc.a:lib_a-sccl.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-memmove.* *libc.a:lib_a-rshift.* *libc.a:lib_a-envlock.* *libc.a:lib_a-strlen.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-strptime.* *libc.a:lib_a-findfp.* *libc.a:lib_a-impure.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-utoa.* *libc.a:lib_a-srand.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-asctime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-atoi.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-memset.* *libc.a:lib_a-atol.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libheap.a:heap_tlsf.* *libxt_hal.a *libesp_system.a:reset_reason.* *libesp_system.a:panic_handler.* *libesp_system.a:panic.* *libesp_common.a:esp_err.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_rom_patch.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_generic.* *librtc.a *libsoc.a:cpu_util.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_pm.* *libsoc.a:rtc_clk.* *libsoc.a:lldesc.* *libsoc.a:rtc_wdt.* *libsoc.a:rtc_time.* *libsoc.a:rtc_periph.* *libxtensa.a:stdatomic.* *libxtensa.a:eri.* *libnewlib.a:heap.* *libnewlib.a:abort.* *libhal.a:spi_flash_hal_iram.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libhal.a:i2c_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_hal_gpspi.* *libhal.a:cpu_hal.* *libhal.a:ledc_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:systimer_hal.* *libhal.a:wdt_hal_iram.* *libhal.a:spi_hal_iram.* *libfreertos.a) .text EXCLUDE_FILE(*libesp_ringbuf.a *libgcov.a *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *liblog.a:log.* *liblog.a:log_freertos.* *libgcc.a:lib2funcs.* *libgcc.a:_divsf3.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-isblank.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-time.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-tzset.* *libc.a:lib_a-raise.* *libc.a:lib_a-sysread.* *libc.a:lib_a-systimes.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strftime.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-close.* *libc.a:lib_a-strupr.* *libc.a:lib_a-bzero.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-memchr.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isupper.* *libc.a:lock.* *libc.a:lib_a-itoa.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-fclose.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-open.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-syswrite.* *libc.a:creat.* *libc.a:lib_a-tolower.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-abs.* *libc.a:lib_a-system.* *libc.a:lib_a-strcspn.* *libc.a:isatty.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-refill.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-toascii.* *libc.a:lib_a-ctime.* *libc.a:lib_a-strspn.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strtol.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-isprint.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strdup.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-rand.* *libc.a:lib_a-strncat.* *libc.a:lib_a-creat.* *libc.a:lib_a-read.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-fflush.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-toupper.* *libc.a:lib_a-quorem.* *libc.a:lib_a-div.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-labs.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-mktime.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-stdio.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-wsetup.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-islower.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-environ.* *libc.a:lib_a-sccl.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-memmove.* *libc.a:lib_a-rshift.* *libc.a:lib_a-envlock.* *libc.a:lib_a-strlen.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-strptime.* *libc.a:lib_a-findfp.* *libc.a:lib_a-impure.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-utoa.* *libc.a:lib_a-srand.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-asctime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-atoi.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-memset.* *libc.a:lib_a-atol.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libheap.a:heap_tlsf.* *libxt_hal.a *libesp_system.a:startup.* *libesp_system.a:system_api.* *libesp_system.a:reset_reason.* *libesp_system.a:panic_handler.* *libesp_system.a:panic.* *libesp_common.a:esp_err.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_rom_patch.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_generic.* *librtc.a *libsoc.a:rtc_init.* *libsoc.a:cpu_util.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_pm.* *libsoc.a:rtc_clk.* *libsoc.a:lldesc.* *libsoc.a:rtc_wdt.* *libsoc.a:rtc_time.* *libsoc.a:rtc_periph.* *libxtensa.a:stdatomic.* *libxtensa.a:eri.* *libnewlib.a:heap.* *libnewlib.a:abort.* *libhal.a:spi_flash_hal_iram.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libhal.a:i2c_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_hal_gpspi.* *libhal.a:cpu_hal.* *libhal.a:ledc_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:systimer_hal.* *libhal.a:wdt_hal_iram.* *libhal.a:spi_hal_iram.* *libfreertos.a) .text.* EXCLUDE_FILE(*libpp.a *libnet80211.a *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .wifi0iram EXCLUDE_FILE(*libpp.a *libnet80211.a *libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:port.* *libfreertos.a:queue.*) .wifi0iram.* EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .wifirxiram EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:port.* *libfreertos.a:queue.*) .wifirxiram.*)
    *libesp_event.a:default_event_loop.*(.literal.esp_event_handler_register .literal.esp_event_handler_instance_register .literal.esp_event_handler_unregister .literal.esp_event_handler_instance_unregister .literal.esp_event_post .literal.esp_event_loop_create_default .literal.esp_event_loop_delete_default .literal.esp_event_send_to_default_loop .text.esp_event_handler_register .text.esp_event_handler_instance_register .text.esp_event_handler_unregister .text.esp_event_handler_instance_unregister .text.esp_event_post .text.esp_event_loop_create_default .text.esp_event_loop_delete_default .text.esp_event_send_to_default_loop)
    *libesp_event.a:esp_event.*(.literal.handler_instances_add .literal.base_node_add_handler .literal.loop_node_add_handler .literal.handler_instances_remove .literal.handler_instances_remove_all$isra$1 .literal.esp_event_loop_create .literal.esp_event_loop_run .literal.esp_event_loop_run_task .literal.esp_event_loop_delete .literal.esp_event_handler_register_with_internal .literal.esp_event_handler_register_with .literal.esp_event_handler_instance_register_with .literal.esp_event_handler_unregister_with_internal .literal.esp_event_handler_unregister_with .literal.esp_event_handler_instance_unregister_with .literal.esp_event_post_to .text.handler_instances_add .text.base_node_add_handler .text.loop_node_add_handler .text.handler_instances_remove .text.handler_instances_remove_all$isra$1 .text.esp_event_loop_create .text.esp_event_loop_run .text.esp_event_loop_run_task .text.esp_event_loop_delete .text.esp_event_handler_register_with_internal .text.esp_event_handler_register_with .text.esp_event_handler_instance_register_with .text.esp_event_handler_unregister_with_internal .text.esp_event_handler_unregister_with .text.esp_event_handler_instance_unregister_with .text.esp_event_post_to .text.esp_event_dump)
    *libesp_system.a:startup.*(.literal.do_system_init_fn .literal.esp_startup_start_app_other_cores_default .text.do_system_init_fn .text.esp_startup_start_app_other_cores_default  .wifi0iram.*  .wifirxiram.*)
    *libesp_system.a:system_api.*(.literal.esp_register_shutdown_handler .literal.esp_unregister_shutdown_handler .literal.esp_get_free_heap_size .literal.esp_get_free_internal_heap_size .literal.esp_get_minimum_free_heap_size .literal.esp_get_idf_version .text.esp_register_shutdown_handler .text.esp_unregister_shutdown_handler .text.esp_get_free_heap_size .text.esp_get_free_internal_heap_size .text.esp_get_minimum_free_heap_size .text.esp_get_idf_version)
    *libesp_system.a:startup.*(.literal.do_core_init .text.do_core_init .wifi0iram.do_core_init .wifirxiram.do_core_init)
    *libesp_system.a:startup.*(.literal.do_secondary_init .text.do_secondary_init .wifi0iram.do_secondary_init .wifirxiram.do_secondary_init)
    *libesp_system.a:startup.*(.literal.start_cpu0_default .text.start_cpu0_default .wifi0iram.start_cpu0_default .wifirxiram.start_cpu0_default)
    *libfreertos.a:port.*( .wifi0iram.*  .wifirxiram.*)
    *libfreertos.a:queue.*( .wifi0iram.*  .wifirxiram.*)
    *libfreertos.a:port.*(.literal.esp_startup_start_app .text.esp_startup_start_app .wifi0iram.esp_startup_start_app .wifirxiram.esp_startup_start_app)
    *libfreertos.a:port.*(.literal.esp_startup_start_app_other_cores .text.esp_startup_start_app_other_cores .wifi0iram.esp_startup_start_app_other_cores .wifirxiram.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.literal.main_task .text.main_task .wifi0iram.main_task .wifirxiram.main_task)
    *libfreertos.a:queue.*(.literal.xQueueGenericCreateStatic .text.xQueueGenericCreateStatic .wifi0iram.xQueueGenericCreateStatic .wifirxiram.xQueueGenericCreateStatic)
    *libhal.a:twai_hal_iram.*( .literal  .literal.*  .text  .text.*  .wifi0iram  .wifi0iram.*  .wifirxiram  .wifirxiram.*)
    *libhal.a:uart_hal_iram.*( .literal  .literal.*  .text  .text.*  .wifi0iram  .wifi0iram.*  .wifirxiram  .wifirxiram.*)
    *liblog.a:log.*(.literal.heap_bubble_down .literal.esp_log_set_vprintf .literal.esp_log_level_set .literal.esp_log_writev .text.heap_bubble_down .text.esp_log_set_vprintf .text.esp_log_level_set .text.esp_log_writev)
    *liblog.a:log_freertos.*(.literal.esp_log_system_timestamp .text.esp_log_system_timestamp)
    *libsoc.a:rtc_init.*(.literal.rtc_init .literal.rtc_vddsdio_get_config .text.rtc_init .text.rtc_vddsdio_get_config)

    *(.stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.irom0.text) /* catch stray ICACHE_RODATA_ATTR */
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _etext = .;

    /* Similar to _iram_start, this symbol goes here so it is
       resolved by addr2line in preference to the first symbol in
       the flash.text segment.
    */
    _flash_cache_start = ABSOLUTE(0);
  } >default_code_seg

  /* Marks the end of IRAM code segment */
  .iram0.text_end (NOLOAD) :
  {
    . = ALIGN (4);
    _iram_text_end = ABSOLUTE(.);
  } > iram0_0_seg

  .iram0.data :
  {
    . = ALIGN(4);
    _iram_data_start = ABSOLUTE(.);

    /* coredump mapping */
    _coredump_iram_start = ABSOLUTE(.);
    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .iram.data.coredump EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:port.* *libfreertos.a:queue.*) .iram.data.coredump.*)
    *libesp_system.a:startup.*( .iram.data.coredump.*)
    *libesp_system.a:startup.*(.iram.data.coredump.do_core_init)
    *libesp_system.a:startup.*(.iram.data.coredump.do_secondary_init)
    *libesp_system.a:startup.*(.iram.data.coredump.start_cpu0_default)
    *libfreertos.a:port.*( .iram.data.coredump.*)
    *libfreertos.a:queue.*( .iram.data.coredump.*)
    *libfreertos.a:port.*(.iram.data.coredump.esp_startup_start_app)
    *libfreertos.a:port.*(.iram.data.coredump.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.iram.data.coredump.main_task)
    *libfreertos.a:queue.*(.iram.data.coredump.xQueueGenericCreateStatic)
    *libhal.a:twai_hal_iram.*( .iram.data.coredump  .iram.data.coredump.*)
    *libhal.a:uart_hal_iram.*( .iram.data.coredump  .iram.data.coredump.*)
    _coredump_iram_end = ABSOLUTE(.);
    
    /* should be placed after coredump mapping */
    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .iram.data EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:queue.* *libfreertos.a:port.*) .iram.data.*)
    *libesp_system.a:startup.*( .iram.data.*)
    *libesp_system.a:startup.*(.iram.data.do_core_init)
    *libesp_system.a:startup.*(.iram.data.do_secondary_init)
    *libesp_system.a:startup.*(.iram.data.start_cpu0_default)
    *libfreertos.a:port.*( .iram.data.*)
    *libfreertos.a:queue.*( .iram.data.*)
    *libfreertos.a:port.*(.iram.data.esp_startup_start_app)
    *libfreertos.a:port.*(.iram.data.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.iram.data.main_task)
    *libfreertos.a:queue.*(.iram.data.xQueueGenericCreateStatic)
    *libhal.a:twai_hal_iram.*( .iram.data  .iram.data.*)
    *libhal.a:uart_hal_iram.*( .iram.data  .iram.data.*)

    _iram_data_end = ABSOLUTE(.);
  } > iram0_0_seg

  .iram0.bss (NOLOAD) :
  {
    . = ALIGN(4);
    _iram_bss_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.*) .iram.bss EXCLUDE_FILE(*libesp_system.a:startup.* *libhal.a:uart_hal_iram.* *libhal.a:twai_hal_iram.* *libfreertos.a:port.* *libfreertos.a:queue.*) .iram.bss.*)
    *libesp_system.a:startup.*( .iram.bss.*)
    *libesp_system.a:startup.*(.iram.bss.do_core_init)
    *libesp_system.a:startup.*(.iram.bss.do_secondary_init)
    *libesp_system.a:startup.*(.iram.bss.start_cpu0_default)
    *libfreertos.a:port.*( .iram.bss.*)
    *libfreertos.a:queue.*( .iram.bss.*)
    *libfreertos.a:port.*(.iram.bss.esp_startup_start_app)
    *libfreertos.a:port.*(.iram.bss.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.iram.bss.main_task)
    *libfreertos.a:queue.*(.iram.bss.xQueueGenericCreateStatic)
    *libhal.a:twai_hal_iram.*( .iram.bss  .iram.bss.*)
    *libhal.a:uart_hal_iram.*( .iram.bss  .iram.bss.*)

    _iram_bss_end = ABSOLUTE(.);
    . = ALIGN(4);
    _iram_end = ABSOLUTE(.);
   } > iram0_0_seg

  /* Marks the end of data, bss and possibly rodata  */
  .dram0.heap_start (NOLOAD) :
  {
    . = ALIGN (8);
    _heap_start = ABSOLUTE(.);
  } > dram0_0_seg
}

ASSERT(((_iram_end - ORIGIN(iram0_0_seg)) <= LENGTH(iram0_0_seg)),
          "IRAM0 segment data does not fit.")

ASSERT(((_heap_start - ORIGIN(dram0_0_seg)) <= LENGTH(dram0_0_seg)),
          "DRAM segment data does not fit.")
