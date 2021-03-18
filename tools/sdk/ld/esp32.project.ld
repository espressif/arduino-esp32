/* Automatically generated file; DO NOT EDIT */
/* Espressif IoT Development Framework Linker Script */
/* Generated from: /home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/esp32/ld/esp32.project.ld.in */

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

    *( .rtc.literal  .rtc.text  .rtc.text.*)

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

    *( .rtc.data  .rtc.data.*  .rtc.rodata  .rtc.rodata.*)

    *rtc_wake_stub*.*(.data .rodata .data.* .rodata.* .bss .bss.*)
    _rtc_data_end = ABSOLUTE(.);
  } > rtc_data_location

  /* RTC bss, from any source file named rtc_wake_stub*.c */
  .rtc.bss (NOLOAD) :
  {
    _rtc_bss_start = ABSOLUTE(.);
    *rtc_wake_stub*.*(.bss .bss.*)
    *rtc_wake_stub*.*(COMMON)

    *( .rtc.bss)

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

    *( .iram1  .iram1.*)
    *libapp_trace.a:( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:creat.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:isatty.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-abs.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-asctime.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-asctime_r.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-atoi.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-atol.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-bzero.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-close.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-creat.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-ctime.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-ctime_r.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-ctype_.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-div.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-environ.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-envlock.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-fclose.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-fflush.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-findfp.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-fputwc.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-fvwrite.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-fwalk.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-getenv_r.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-gettzinfo.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-gmtime.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-gmtime_r.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-impure.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-isalnum.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-isalpha.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-isascii.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-isblank.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-iscntrl.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-isdigit.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-isgraph.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-islower.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-isprint.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-ispunct.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-isspace.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-isupper.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-itoa.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-labs.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-lcltime.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-lcltime_r.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-ldiv.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-longjmp.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-makebuf.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-memccpy.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-memchr.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-memcmp.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-memcpy.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-memmove.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-memrchr.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-memset.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-mktime.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-month_lengths.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-open.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-quorem.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-raise.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-rand.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-rand_r.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-read.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-refill.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-rshift.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-s_fpclassify.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-sbrk.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-sccl.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-setjmp.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-sf_nan.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-srand.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-stdio.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strcasecmp.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strcasestr.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strcat.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strchr.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strcmp.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strcoll.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strcpy.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strcspn.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strdup.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strdup_r.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strftime.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strlcat.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strlcpy.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strlen.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strlwr.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strncasecmp.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strncat.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strncmp.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strncpy.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strndup.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strndup_r.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strnlen.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strptime.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strrchr.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strsep.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strspn.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strstr.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strtok_r.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strtol.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strtoul.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-strupr.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-sysclose.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-sysopen.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-sysread.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-syssbrk.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-system.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-systimes.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-syswrite.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-time.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-timelocal.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-toascii.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-tolower.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-toupper.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-tzcalc_limits.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-tzlock.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-tzset.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-tzset_r.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-tzvars.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-ungetc.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-utoa.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-wbuf.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-wcrtomb.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-wctomb_r.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lib_a-wsetup.*( .literal  .literal.*  .text  .text.*)
    *libc-psram-workaround.a:lock.*( .literal  .literal.*  .text  .text.*)
    *libesp32.a:panic.*( .literal  .literal.*  .text  .text.*)
    *libesp_ringbuf.a:( .literal  .literal.*  .text  .text.*)
    *libespcoredump.a:core_dump_common.*( .literal  .literal.*  .text  .text.*)
    *libespcoredump.a:core_dump_flash.*( .literal  .literal.*  .text  .text.*)
    *libespcoredump.a:core_dump_port.*( .literal  .literal.*  .text  .text.*)
    *libespcoredump.a:core_dump_uart.*( .literal  .literal.*  .text  .text.*)
    *libfreertos.a:( .literal  .literal.*  .text  .text.*)
    *libgcc.a:lib2funcs.*( .literal  .literal.*  .text  .text.*)
    *libgcov.a:( .literal  .literal.*  .text  .text.*)
    *libhal.a:( .literal  .literal.*  .text  .text.*)
    *libheap.a:multi_heap.*( .literal  .literal.*  .text  .text.*)
    *libheap.a:multi_heap_poisoning.*( .literal  .literal.*  .text  .text.*)
    *librtc.a:( .literal  .literal.*  .text  .text.*)
    *libsoc.a:cpu_util.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_clk.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_clk_init.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_init.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_periph.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_pm.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_sleep.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_time.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_wdt.*( .literal  .literal.*  .text  .text.*)
    *libspi_flash.a:spi_flash_rom_patch.*( .literal  .literal.*  .text  .text.*)
    *libxtensa-debug-module.a:eri.*( .literal  .literal.*  .text  .text.*)

    _iram_text_end = ABSOLUTE(.);
    _iram_end = ABSOLUTE(.);
  } > iram0_0_seg

  ASSERT(((_iram_text_end - ORIGIN(iram0_0_seg)) <= LENGTH(iram0_0_seg)),
          "IRAM0 segment data does not fit.")

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
    *(.gnu.linkonce.d.*)
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    *(.jcr)

    *( .data  .data.*  .dram1  .dram1.*)
    *libapp_trace.a:( .rodata  .rodata.*)
    *libc-psram-workaround.a:creat.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:isatty.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-abs.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-asctime.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-asctime_r.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-atoi.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-atol.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-bzero.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-close.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-creat.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-ctime.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-ctime_r.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-ctype_.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-div.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-environ.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-envlock.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-fclose.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-fflush.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-findfp.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-fputwc.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-fvwrite.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-fwalk.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-getenv_r.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-gettzinfo.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-gmtime.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-gmtime_r.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-impure.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-isalnum.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-isalpha.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-isascii.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-isblank.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-iscntrl.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-isdigit.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-isgraph.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-islower.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-isprint.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-ispunct.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-isspace.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-isupper.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-itoa.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-labs.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-lcltime.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-lcltime_r.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-ldiv.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-longjmp.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-makebuf.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-memccpy.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-memchr.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-memcmp.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-memcpy.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-memmove.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-memrchr.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-memset.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-mktime.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-month_lengths.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-open.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-quorem.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-raise.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-rand.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-rand_r.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-read.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-refill.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-rshift.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-s_fpclassify.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-sbrk.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-sccl.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-setjmp.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-sf_nan.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-srand.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-stdio.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strcasecmp.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strcasestr.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strcat.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strchr.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strcmp.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strcoll.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strcpy.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strcspn.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strdup.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strdup_r.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strftime.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strlcat.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strlcpy.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strlen.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strlwr.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strncasecmp.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strncat.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strncmp.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strncpy.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strndup.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strndup_r.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strnlen.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strptime.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strrchr.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strsep.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strspn.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strstr.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strtok_r.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strtol.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strtoul.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-strupr.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-sysclose.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-sysopen.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-sysread.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-syssbrk.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-system.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-systimes.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-syswrite.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-time.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-timelocal.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-toascii.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-tolower.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-toupper.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-tzcalc_limits.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-tzlock.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-tzset.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-tzset_r.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-tzvars.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-ungetc.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-utoa.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-wbuf.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-wcrtomb.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-wctomb_r.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lib_a-wsetup.*( .rodata  .rodata.*)
    *libc-psram-workaround.a:lock.*( .rodata  .rodata.*)
    *libesp32.a:panic.*( .rodata  .rodata.*)
    *libgcov.a:( .rodata  .rodata.*)
    *libheap.a:multi_heap.*( .rodata  .rodata.*)
    *libheap.a:multi_heap_poisoning.*( .rodata  .rodata.*)
    *libphy.a:( .rodata  .rodata.*)
    *libsoc.a:rtc_clk.*( .rodata  .rodata.*)

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

    *( .bss  .bss.*  COMMON)

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
    /* The heap starts right after end of this section */
    _heap_start = ABSOLUTE(.);
  } > dram0_0_seg

  ASSERT(((_bss_end - ORIGIN(dram0_0_seg)) <= LENGTH(dram0_0_seg)),
          "DRAM segment data does not fit.")
          
  .flash.rodata :
  {
    _rodata_start = ABSOLUTE(.);

    *(.rodata_desc .rodata_desc.*)               /* Should be the first.  App version info.        DO NOT PUT ANYTHING BEFORE IT! */
    *(.rodata_custom_desc .rodata_custom_desc.*) /* Should be the second. Custom app version info. DO NOT PUT ANYTHING BEFORE IT! */

    *(EXCLUDE_FILE(*libapp_trace.a *libesp32.a:panic.* *libphy.a *libgcov.a *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libc-psram-workaround.a:lib_a-tzset_r.* *libc-psram-workaround.a:lib_a-s_fpclassify.* *libc-psram-workaround.a:lib_a-refill.* *libc-psram-workaround.a:lib_a-isspace.* *libc-psram-workaround.a:lib_a-div.* *libc-psram-workaround.a:lib_a-strptime.* *libc-psram-workaround.a:lib_a-memchr.* *libc-psram-workaround.a:lib_a-wsetup.* *libc-psram-workaround.a:lib_a-strtoul.* *libc-psram-workaround.a:lib_a-itoa.* *libc-psram-workaround.a:lib_a-tolower.* *libc-psram-workaround.a:lib_a-rand_r.* *libc-psram-workaround.a:lib_a-strchr.* *libc-psram-workaround.a:lib_a-syswrite.* *libc-psram-workaround.a:lib_a-isupper.* *libc-psram-workaround.a:lib_a-tzset.* *libc-psram-workaround.a:lib_a-close.* *libc-psram-workaround.a:lib_a-strndup_r.* *libc-psram-workaround.a:lib_a-strrchr.* *libc-psram-workaround.a:lib_a-rshift.* *libc-psram-workaround.a:lib_a-sbrk.* *libc-psram-workaround.a:lib_a-ctype_.* *libc-psram-workaround.a:lib_a-sf_nan.* *libc-psram-workaround.a:lib_a-wbuf.* *libc-psram-workaround.a:lib_a-utoa.* *libc-psram-workaround.a:isatty.* *libc-psram-workaround.a:lib_a-memccpy.* *libc-psram-workaround.a:lib_a-strncpy.* *libc-psram-workaround.a:lib_a-ldiv.* *libc-psram-workaround.a:lib_a-strftime.* *libc-psram-workaround.a:lib_a-atoi.* *libc-psram-workaround.a:lib_a-bzero.* *libc-psram-workaround.a:lib_a-lcltime.* *libc-psram-workaround.a:lib_a-fwalk.* *libc-psram-workaround.a:lib_a-asctime_r.* *libc-psram-workaround.a:lib_a-open.* *libc-psram-workaround.a:lib_a-fflush.* *libc-psram-workaround.a:lib_a-quorem.* *libc-psram-workaround.a:lib_a-tzlock.* *libc-psram-workaround.a:lib_a-tzvars.* *libc-psram-workaround.a:lib_a-isalnum.* *libc-psram-workaround.a:lib_a-strncasecmp.* *libc-psram-workaround.a:lib_a-ctime.* *libc-psram-workaround.a:lib_a-strcspn.* *libc-psram-workaround.a:lib_a-impure.* *libc-psram-workaround.a:lib_a-environ.* *libc-psram-workaround.a:creat.* *libc-psram-workaround.a:lib_a-longjmp.* *libc-psram-workaround.a:lib_a-abs.* *libc-psram-workaround.a:lib_a-fputwc.* *libc-psram-workaround.a:lib_a-fvwrite.* *libc-psram-workaround.a:lib_a-isalpha.* *libc-psram-workaround.a:lib_a-memset.* *libc-psram-workaround.a:lib_a-syssbrk.* *libc-psram-workaround.a:lib_a-ispunct.* *libc-psram-workaround.a:lib_a-read.* *libc-psram-workaround.a:lib_a-systimes.* *libc-psram-workaround.a:lib_a-memcmp.* *libc-psram-workaround.a:lib_a-wctomb_r.* *libc-psram-workaround.a:lib_a-strlcat.* *libc-psram-workaround.a:lib_a-strnlen.* *libc-psram-workaround.a:lib_a-sysread.* *libc-psram-workaround.a:lib_a-month_lengths.* *libc-psram-workaround.a:lib_a-mktime.* *libc-psram-workaround.a:lib_a-strsep.* *libc-psram-workaround.a:lib_a-sysopen.* *libc-psram-workaround.a:lib_a-isdigit.* *libc-psram-workaround.a:lib_a-isblank.* *libc-psram-workaround.a:lib_a-strndup.* *libc-psram-workaround.a:lib_a-creat.* *libc-psram-workaround.a:lib_a-gmtime.* *libc-psram-workaround.a:lib_a-isprint.* *libc-psram-workaround.a:lib_a-ungetc.* *libc-psram-workaround.a:lib_a-makebuf.* *libc-psram-workaround.a:lib_a-gmtime_r.* *libc-psram-workaround.a:lib_a-strtok_r.* *libc-psram-workaround.a:lib_a-raise.* *libc-psram-workaround.a:lib_a-isascii.* *libc-psram-workaround.a:lib_a-strcpy.* *libc-psram-workaround.a:lib_a-strcmp.* *libc-psram-workaround.a:lib_a-toupper.* *libc-psram-workaround.a:lib_a-envlock.* *libc-psram-workaround.a:lib_a-strlwr.* *libc-psram-workaround.a:lib_a-srand.* *libc-psram-workaround.a:lib_a-memrchr.* *libc-psram-workaround.a:lib_a-strdup.* *libc-psram-workaround.a:lib_a-system.* *libc-psram-workaround.a:lib_a-toascii.* *libc-psram-workaround.a:lib_a-strdup_r.* *libc-psram-workaround.a:lib_a-memcpy.* *libc-psram-workaround.a:lib_a-getenv_r.* *libc-psram-workaround.a:lib_a-stdio.* *libc-psram-workaround.a:lib_a-time.* *libc-psram-workaround.a:lib_a-ctime_r.* *libc-psram-workaround.a:lib_a-isgraph.* *libc-psram-workaround.a:lib_a-sccl.* *libc-psram-workaround.a:lib_a-strlcpy.* *libc-psram-workaround.a:lib_a-atol.* *libc-psram-workaround.a:lib_a-strlen.* *libc-psram-workaround.a:lib_a-strcasecmp.* *libc-psram-workaround.a:lib_a-wcrtomb.* *libc-psram-workaround.a:lib_a-sysclose.* *libc-psram-workaround.a:lib_a-tzcalc_limits.* *libc-psram-workaround.a:lib_a-islower.* *libc-psram-workaround.a:lib_a-setjmp.* *libc-psram-workaround.a:lib_a-timelocal.* *libc-psram-workaround.a:lib_a-gettzinfo.* *libc-psram-workaround.a:lib_a-rand.* *libc-psram-workaround.a:lib_a-strncat.* *libc-psram-workaround.a:lib_a-strcasestr.* *libc-psram-workaround.a:lib_a-strcoll.* *libc-psram-workaround.a:lock.* *libc-psram-workaround.a:lib_a-iscntrl.* *libc-psram-workaround.a:lib_a-lcltime_r.* *libc-psram-workaround.a:lib_a-strtol.* *libc-psram-workaround.a:lib_a-labs.* *libc-psram-workaround.a:lib_a-strupr.* *libc-psram-workaround.a:lib_a-memmove.* *libc-psram-workaround.a:lib_a-strspn.* *libc-psram-workaround.a:lib_a-strcat.* *libc-psram-workaround.a:lib_a-findfp.* *libc-psram-workaround.a:lib_a-fclose.* *libc-psram-workaround.a:lib_a-strncmp.* *libc-psram-workaround.a:lib_a-strstr.* *libc-psram-workaround.a:lib_a-asctime.* *libsoc.a:rtc_clk.*) .rodata EXCLUDE_FILE(*libapp_trace.a *libesp32.a:panic.* *libphy.a *libgcov.a *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libc-psram-workaround.a:lib_a-tzset_r.* *libc-psram-workaround.a:lib_a-s_fpclassify.* *libc-psram-workaround.a:lib_a-refill.* *libc-psram-workaround.a:lib_a-isspace.* *libc-psram-workaround.a:lib_a-div.* *libc-psram-workaround.a:lib_a-strptime.* *libc-psram-workaround.a:lib_a-memchr.* *libc-psram-workaround.a:lib_a-wsetup.* *libc-psram-workaround.a:lib_a-strtoul.* *libc-psram-workaround.a:lib_a-itoa.* *libc-psram-workaround.a:lib_a-tolower.* *libc-psram-workaround.a:lib_a-rand_r.* *libc-psram-workaround.a:lib_a-strchr.* *libc-psram-workaround.a:lib_a-syswrite.* *libc-psram-workaround.a:lib_a-isupper.* *libc-psram-workaround.a:lib_a-tzset.* *libc-psram-workaround.a:lib_a-close.* *libc-psram-workaround.a:lib_a-strndup_r.* *libc-psram-workaround.a:lib_a-strrchr.* *libc-psram-workaround.a:lib_a-rshift.* *libc-psram-workaround.a:lib_a-sbrk.* *libc-psram-workaround.a:lib_a-ctype_.* *libc-psram-workaround.a:lib_a-sf_nan.* *libc-psram-workaround.a:lib_a-wbuf.* *libc-psram-workaround.a:lib_a-utoa.* *libc-psram-workaround.a:isatty.* *libc-psram-workaround.a:lib_a-memccpy.* *libc-psram-workaround.a:lib_a-strncpy.* *libc-psram-workaround.a:lib_a-ldiv.* *libc-psram-workaround.a:lib_a-strftime.* *libc-psram-workaround.a:lib_a-atoi.* *libc-psram-workaround.a:lib_a-bzero.* *libc-psram-workaround.a:lib_a-lcltime.* *libc-psram-workaround.a:lib_a-fwalk.* *libc-psram-workaround.a:lib_a-asctime_r.* *libc-psram-workaround.a:lib_a-open.* *libc-psram-workaround.a:lib_a-fflush.* *libc-psram-workaround.a:lib_a-quorem.* *libc-psram-workaround.a:lib_a-tzlock.* *libc-psram-workaround.a:lib_a-tzvars.* *libc-psram-workaround.a:lib_a-isalnum.* *libc-psram-workaround.a:lib_a-strncasecmp.* *libc-psram-workaround.a:lib_a-ctime.* *libc-psram-workaround.a:lib_a-strcspn.* *libc-psram-workaround.a:lib_a-impure.* *libc-psram-workaround.a:lib_a-environ.* *libc-psram-workaround.a:creat.* *libc-psram-workaround.a:lib_a-longjmp.* *libc-psram-workaround.a:lib_a-abs.* *libc-psram-workaround.a:lib_a-fputwc.* *libc-psram-workaround.a:lib_a-fvwrite.* *libc-psram-workaround.a:lib_a-isalpha.* *libc-psram-workaround.a:lib_a-memset.* *libc-psram-workaround.a:lib_a-syssbrk.* *libc-psram-workaround.a:lib_a-ispunct.* *libc-psram-workaround.a:lib_a-read.* *libc-psram-workaround.a:lib_a-systimes.* *libc-psram-workaround.a:lib_a-memcmp.* *libc-psram-workaround.a:lib_a-wctomb_r.* *libc-psram-workaround.a:lib_a-strlcat.* *libc-psram-workaround.a:lib_a-strnlen.* *libc-psram-workaround.a:lib_a-sysread.* *libc-psram-workaround.a:lib_a-month_lengths.* *libc-psram-workaround.a:lib_a-mktime.* *libc-psram-workaround.a:lib_a-strsep.* *libc-psram-workaround.a:lib_a-sysopen.* *libc-psram-workaround.a:lib_a-isdigit.* *libc-psram-workaround.a:lib_a-isblank.* *libc-psram-workaround.a:lib_a-strndup.* *libc-psram-workaround.a:lib_a-creat.* *libc-psram-workaround.a:lib_a-gmtime.* *libc-psram-workaround.a:lib_a-isprint.* *libc-psram-workaround.a:lib_a-ungetc.* *libc-psram-workaround.a:lib_a-makebuf.* *libc-psram-workaround.a:lib_a-gmtime_r.* *libc-psram-workaround.a:lib_a-strtok_r.* *libc-psram-workaround.a:lib_a-raise.* *libc-psram-workaround.a:lib_a-isascii.* *libc-psram-workaround.a:lib_a-strcpy.* *libc-psram-workaround.a:lib_a-strcmp.* *libc-psram-workaround.a:lib_a-toupper.* *libc-psram-workaround.a:lib_a-envlock.* *libc-psram-workaround.a:lib_a-strlwr.* *libc-psram-workaround.a:lib_a-srand.* *libc-psram-workaround.a:lib_a-memrchr.* *libc-psram-workaround.a:lib_a-strdup.* *libc-psram-workaround.a:lib_a-system.* *libc-psram-workaround.a:lib_a-toascii.* *libc-psram-workaround.a:lib_a-strdup_r.* *libc-psram-workaround.a:lib_a-memcpy.* *libc-psram-workaround.a:lib_a-getenv_r.* *libc-psram-workaround.a:lib_a-stdio.* *libc-psram-workaround.a:lib_a-time.* *libc-psram-workaround.a:lib_a-ctime_r.* *libc-psram-workaround.a:lib_a-isgraph.* *libc-psram-workaround.a:lib_a-sccl.* *libc-psram-workaround.a:lib_a-strlcpy.* *libc-psram-workaround.a:lib_a-atol.* *libc-psram-workaround.a:lib_a-strlen.* *libc-psram-workaround.a:lib_a-strcasecmp.* *libc-psram-workaround.a:lib_a-wcrtomb.* *libc-psram-workaround.a:lib_a-sysclose.* *libc-psram-workaround.a:lib_a-tzcalc_limits.* *libc-psram-workaround.a:lib_a-islower.* *libc-psram-workaround.a:lib_a-setjmp.* *libc-psram-workaround.a:lib_a-timelocal.* *libc-psram-workaround.a:lib_a-gettzinfo.* *libc-psram-workaround.a:lib_a-rand.* *libc-psram-workaround.a:lib_a-strncat.* *libc-psram-workaround.a:lib_a-strcasestr.* *libc-psram-workaround.a:lib_a-strcoll.* *libc-psram-workaround.a:lock.* *libc-psram-workaround.a:lib_a-iscntrl.* *libc-psram-workaround.a:lib_a-lcltime_r.* *libc-psram-workaround.a:lib_a-strtol.* *libc-psram-workaround.a:lib_a-labs.* *libc-psram-workaround.a:lib_a-strupr.* *libc-psram-workaround.a:lib_a-memmove.* *libc-psram-workaround.a:lib_a-strspn.* *libc-psram-workaround.a:lib_a-strcat.* *libc-psram-workaround.a:lib_a-findfp.* *libc-psram-workaround.a:lib_a-fclose.* *libc-psram-workaround.a:lib_a-strncmp.* *libc-psram-workaround.a:lib_a-strstr.* *libc-psram-workaround.a:lib_a-asctime.* *libsoc.a:rtc_clk.*) .rodata.*)

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
    /*  C++ constructor and destructor tables, properly ordered:  */
    __init_array_start = ABSOLUTE(.);
    KEEP (*crtbegin.*(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.*) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
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
  } >drom0_0_seg

  .flash.text :
  {
    _stext = .;
    _text_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libapp_trace.a *libesp32.a:panic.* *libhal.a *librtc.a *libgcc.a:lib2funcs.* *libgcov.a *libesp_ringbuf.a *libespcoredump.a:core_dump_port.* *libespcoredump.a:core_dump_flash.* *libespcoredump.a:core_dump_common.* *libespcoredump.a:core_dump_uart.* *libfreertos.a *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libc-psram-workaround.a:lib_a-tzset_r.* *libc-psram-workaround.a:lib_a-s_fpclassify.* *libc-psram-workaround.a:lib_a-refill.* *libc-psram-workaround.a:lib_a-isspace.* *libc-psram-workaround.a:lib_a-div.* *libc-psram-workaround.a:lib_a-strptime.* *libc-psram-workaround.a:lib_a-memchr.* *libc-psram-workaround.a:lib_a-wsetup.* *libc-psram-workaround.a:lib_a-strtoul.* *libc-psram-workaround.a:lib_a-itoa.* *libc-psram-workaround.a:lib_a-tolower.* *libc-psram-workaround.a:lib_a-rand_r.* *libc-psram-workaround.a:lib_a-strchr.* *libc-psram-workaround.a:lib_a-syswrite.* *libc-psram-workaround.a:lib_a-isupper.* *libc-psram-workaround.a:lib_a-tzset.* *libc-psram-workaround.a:lib_a-close.* *libc-psram-workaround.a:lib_a-strndup_r.* *libc-psram-workaround.a:lib_a-strrchr.* *libc-psram-workaround.a:lib_a-rshift.* *libc-psram-workaround.a:lib_a-sbrk.* *libc-psram-workaround.a:lib_a-ctype_.* *libc-psram-workaround.a:lib_a-sf_nan.* *libc-psram-workaround.a:lib_a-wbuf.* *libc-psram-workaround.a:lib_a-utoa.* *libc-psram-workaround.a:isatty.* *libc-psram-workaround.a:lib_a-memccpy.* *libc-psram-workaround.a:lib_a-strncpy.* *libc-psram-workaround.a:lib_a-ldiv.* *libc-psram-workaround.a:lib_a-strftime.* *libc-psram-workaround.a:lib_a-atoi.* *libc-psram-workaround.a:lib_a-bzero.* *libc-psram-workaround.a:lib_a-lcltime.* *libc-psram-workaround.a:lib_a-fwalk.* *libc-psram-workaround.a:lib_a-asctime_r.* *libc-psram-workaround.a:lib_a-open.* *libc-psram-workaround.a:lib_a-fflush.* *libc-psram-workaround.a:lib_a-quorem.* *libc-psram-workaround.a:lib_a-tzlock.* *libc-psram-workaround.a:lib_a-tzvars.* *libc-psram-workaround.a:lib_a-isalnum.* *libc-psram-workaround.a:lib_a-strncasecmp.* *libc-psram-workaround.a:lib_a-ctime.* *libc-psram-workaround.a:lib_a-strcspn.* *libc-psram-workaround.a:lib_a-impure.* *libc-psram-workaround.a:lib_a-environ.* *libc-psram-workaround.a:creat.* *libc-psram-workaround.a:lib_a-longjmp.* *libc-psram-workaround.a:lib_a-abs.* *libc-psram-workaround.a:lib_a-fputwc.* *libc-psram-workaround.a:lib_a-fvwrite.* *libc-psram-workaround.a:lib_a-isalpha.* *libc-psram-workaround.a:lib_a-memset.* *libc-psram-workaround.a:lib_a-syssbrk.* *libc-psram-workaround.a:lib_a-ispunct.* *libc-psram-workaround.a:lib_a-read.* *libc-psram-workaround.a:lib_a-systimes.* *libc-psram-workaround.a:lib_a-memcmp.* *libc-psram-workaround.a:lib_a-wctomb_r.* *libc-psram-workaround.a:lib_a-strlcat.* *libc-psram-workaround.a:lib_a-strnlen.* *libc-psram-workaround.a:lib_a-sysread.* *libc-psram-workaround.a:lib_a-month_lengths.* *libc-psram-workaround.a:lib_a-mktime.* *libc-psram-workaround.a:lib_a-strsep.* *libc-psram-workaround.a:lib_a-sysopen.* *libc-psram-workaround.a:lib_a-isdigit.* *libc-psram-workaround.a:lib_a-isblank.* *libc-psram-workaround.a:lib_a-strndup.* *libc-psram-workaround.a:lib_a-creat.* *libc-psram-workaround.a:lib_a-gmtime.* *libc-psram-workaround.a:lib_a-isprint.* *libc-psram-workaround.a:lib_a-ungetc.* *libc-psram-workaround.a:lib_a-makebuf.* *libc-psram-workaround.a:lib_a-gmtime_r.* *libc-psram-workaround.a:lib_a-strtok_r.* *libc-psram-workaround.a:lib_a-raise.* *libc-psram-workaround.a:lib_a-isascii.* *libc-psram-workaround.a:lib_a-strcpy.* *libc-psram-workaround.a:lib_a-strcmp.* *libc-psram-workaround.a:lib_a-toupper.* *libc-psram-workaround.a:lib_a-envlock.* *libc-psram-workaround.a:lib_a-strlwr.* *libc-psram-workaround.a:lib_a-srand.* *libc-psram-workaround.a:lib_a-memrchr.* *libc-psram-workaround.a:lib_a-strdup.* *libc-psram-workaround.a:lib_a-system.* *libc-psram-workaround.a:lib_a-toascii.* *libc-psram-workaround.a:lib_a-strdup_r.* *libc-psram-workaround.a:lib_a-memcpy.* *libc-psram-workaround.a:lib_a-getenv_r.* *libc-psram-workaround.a:lib_a-stdio.* *libc-psram-workaround.a:lib_a-time.* *libc-psram-workaround.a:lib_a-ctime_r.* *libc-psram-workaround.a:lib_a-isgraph.* *libc-psram-workaround.a:lib_a-sccl.* *libc-psram-workaround.a:lib_a-strlcpy.* *libc-psram-workaround.a:lib_a-atol.* *libc-psram-workaround.a:lib_a-strlen.* *libc-psram-workaround.a:lib_a-strcasecmp.* *libc-psram-workaround.a:lib_a-wcrtomb.* *libc-psram-workaround.a:lib_a-sysclose.* *libc-psram-workaround.a:lib_a-tzcalc_limits.* *libc-psram-workaround.a:lib_a-islower.* *libc-psram-workaround.a:lib_a-setjmp.* *libc-psram-workaround.a:lib_a-timelocal.* *libc-psram-workaround.a:lib_a-gettzinfo.* *libc-psram-workaround.a:lib_a-rand.* *libc-psram-workaround.a:lib_a-strncat.* *libc-psram-workaround.a:lib_a-strcasestr.* *libc-psram-workaround.a:lib_a-strcoll.* *libc-psram-workaround.a:lock.* *libc-psram-workaround.a:lib_a-iscntrl.* *libc-psram-workaround.a:lib_a-lcltime_r.* *libc-psram-workaround.a:lib_a-strtol.* *libc-psram-workaround.a:lib_a-labs.* *libc-psram-workaround.a:lib_a-strupr.* *libc-psram-workaround.a:lib_a-memmove.* *libc-psram-workaround.a:lib_a-strspn.* *libc-psram-workaround.a:lib_a-strcat.* *libc-psram-workaround.a:lib_a-findfp.* *libc-psram-workaround.a:lib_a-fclose.* *libc-psram-workaround.a:lib_a-strncmp.* *libc-psram-workaround.a:lib_a-strstr.* *libc-psram-workaround.a:lib_a-asctime.* *libsoc.a:rtc_pm.* *libsoc.a:rtc_clk.* *libsoc.a:rtc_init.* *libsoc.a:rtc_wdt.* *libsoc.a:cpu_util.* *libsoc.a:rtc_periph.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_clk_init.* *libsoc.a:rtc_time.* *libspi_flash.a:spi_flash_rom_patch.* *libxtensa-debug-module.a:eri.*) .literal EXCLUDE_FILE(*libapp_trace.a *libesp32.a:panic.* *libhal.a *librtc.a *libgcc.a:lib2funcs.* *libgcov.a *libesp_ringbuf.a *libespcoredump.a:core_dump_port.* *libespcoredump.a:core_dump_flash.* *libespcoredump.a:core_dump_common.* *libespcoredump.a:core_dump_uart.* *libfreertos.a *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libc-psram-workaround.a:lib_a-tzset_r.* *libc-psram-workaround.a:lib_a-s_fpclassify.* *libc-psram-workaround.a:lib_a-refill.* *libc-psram-workaround.a:lib_a-isspace.* *libc-psram-workaround.a:lib_a-div.* *libc-psram-workaround.a:lib_a-strptime.* *libc-psram-workaround.a:lib_a-memchr.* *libc-psram-workaround.a:lib_a-wsetup.* *libc-psram-workaround.a:lib_a-strtoul.* *libc-psram-workaround.a:lib_a-itoa.* *libc-psram-workaround.a:lib_a-tolower.* *libc-psram-workaround.a:lib_a-rand_r.* *libc-psram-workaround.a:lib_a-strchr.* *libc-psram-workaround.a:lib_a-syswrite.* *libc-psram-workaround.a:lib_a-isupper.* *libc-psram-workaround.a:lib_a-tzset.* *libc-psram-workaround.a:lib_a-close.* *libc-psram-workaround.a:lib_a-strndup_r.* *libc-psram-workaround.a:lib_a-strrchr.* *libc-psram-workaround.a:lib_a-rshift.* *libc-psram-workaround.a:lib_a-sbrk.* *libc-psram-workaround.a:lib_a-ctype_.* *libc-psram-workaround.a:lib_a-sf_nan.* *libc-psram-workaround.a:lib_a-wbuf.* *libc-psram-workaround.a:lib_a-utoa.* *libc-psram-workaround.a:isatty.* *libc-psram-workaround.a:lib_a-memccpy.* *libc-psram-workaround.a:lib_a-strncpy.* *libc-psram-workaround.a:lib_a-ldiv.* *libc-psram-workaround.a:lib_a-strftime.* *libc-psram-workaround.a:lib_a-atoi.* *libc-psram-workaround.a:lib_a-bzero.* *libc-psram-workaround.a:lib_a-lcltime.* *libc-psram-workaround.a:lib_a-fwalk.* *libc-psram-workaround.a:lib_a-asctime_r.* *libc-psram-workaround.a:lib_a-open.* *libc-psram-workaround.a:lib_a-fflush.* *libc-psram-workaround.a:lib_a-quorem.* *libc-psram-workaround.a:lib_a-tzlock.* *libc-psram-workaround.a:lib_a-tzvars.* *libc-psram-workaround.a:lib_a-isalnum.* *libc-psram-workaround.a:lib_a-strncasecmp.* *libc-psram-workaround.a:lib_a-ctime.* *libc-psram-workaround.a:lib_a-strcspn.* *libc-psram-workaround.a:lib_a-impure.* *libc-psram-workaround.a:lib_a-environ.* *libc-psram-workaround.a:creat.* *libc-psram-workaround.a:lib_a-longjmp.* *libc-psram-workaround.a:lib_a-abs.* *libc-psram-workaround.a:lib_a-fputwc.* *libc-psram-workaround.a:lib_a-fvwrite.* *libc-psram-workaround.a:lib_a-isalpha.* *libc-psram-workaround.a:lib_a-memset.* *libc-psram-workaround.a:lib_a-syssbrk.* *libc-psram-workaround.a:lib_a-ispunct.* *libc-psram-workaround.a:lib_a-read.* *libc-psram-workaround.a:lib_a-systimes.* *libc-psram-workaround.a:lib_a-memcmp.* *libc-psram-workaround.a:lib_a-wctomb_r.* *libc-psram-workaround.a:lib_a-strlcat.* *libc-psram-workaround.a:lib_a-strnlen.* *libc-psram-workaround.a:lib_a-sysread.* *libc-psram-workaround.a:lib_a-month_lengths.* *libc-psram-workaround.a:lib_a-mktime.* *libc-psram-workaround.a:lib_a-strsep.* *libc-psram-workaround.a:lib_a-sysopen.* *libc-psram-workaround.a:lib_a-isdigit.* *libc-psram-workaround.a:lib_a-isblank.* *libc-psram-workaround.a:lib_a-strndup.* *libc-psram-workaround.a:lib_a-creat.* *libc-psram-workaround.a:lib_a-gmtime.* *libc-psram-workaround.a:lib_a-isprint.* *libc-psram-workaround.a:lib_a-ungetc.* *libc-psram-workaround.a:lib_a-makebuf.* *libc-psram-workaround.a:lib_a-gmtime_r.* *libc-psram-workaround.a:lib_a-strtok_r.* *libc-psram-workaround.a:lib_a-raise.* *libc-psram-workaround.a:lib_a-isascii.* *libc-psram-workaround.a:lib_a-strcpy.* *libc-psram-workaround.a:lib_a-strcmp.* *libc-psram-workaround.a:lib_a-toupper.* *libc-psram-workaround.a:lib_a-envlock.* *libc-psram-workaround.a:lib_a-strlwr.* *libc-psram-workaround.a:lib_a-srand.* *libc-psram-workaround.a:lib_a-memrchr.* *libc-psram-workaround.a:lib_a-strdup.* *libc-psram-workaround.a:lib_a-system.* *libc-psram-workaround.a:lib_a-toascii.* *libc-psram-workaround.a:lib_a-strdup_r.* *libc-psram-workaround.a:lib_a-memcpy.* *libc-psram-workaround.a:lib_a-getenv_r.* *libc-psram-workaround.a:lib_a-stdio.* *libc-psram-workaround.a:lib_a-time.* *libc-psram-workaround.a:lib_a-ctime_r.* *libc-psram-workaround.a:lib_a-isgraph.* *libc-psram-workaround.a:lib_a-sccl.* *libc-psram-workaround.a:lib_a-strlcpy.* *libc-psram-workaround.a:lib_a-atol.* *libc-psram-workaround.a:lib_a-strlen.* *libc-psram-workaround.a:lib_a-strcasecmp.* *libc-psram-workaround.a:lib_a-wcrtomb.* *libc-psram-workaround.a:lib_a-sysclose.* *libc-psram-workaround.a:lib_a-tzcalc_limits.* *libc-psram-workaround.a:lib_a-islower.* *libc-psram-workaround.a:lib_a-setjmp.* *libc-psram-workaround.a:lib_a-timelocal.* *libc-psram-workaround.a:lib_a-gettzinfo.* *libc-psram-workaround.a:lib_a-rand.* *libc-psram-workaround.a:lib_a-strncat.* *libc-psram-workaround.a:lib_a-strcasestr.* *libc-psram-workaround.a:lib_a-strcoll.* *libc-psram-workaround.a:lock.* *libc-psram-workaround.a:lib_a-iscntrl.* *libc-psram-workaround.a:lib_a-lcltime_r.* *libc-psram-workaround.a:lib_a-strtol.* *libc-psram-workaround.a:lib_a-labs.* *libc-psram-workaround.a:lib_a-strupr.* *libc-psram-workaround.a:lib_a-memmove.* *libc-psram-workaround.a:lib_a-strspn.* *libc-psram-workaround.a:lib_a-strcat.* *libc-psram-workaround.a:lib_a-findfp.* *libc-psram-workaround.a:lib_a-fclose.* *libc-psram-workaround.a:lib_a-strncmp.* *libc-psram-workaround.a:lib_a-strstr.* *libc-psram-workaround.a:lib_a-asctime.* *libsoc.a:rtc_pm.* *libsoc.a:rtc_clk.* *libsoc.a:rtc_init.* *libsoc.a:rtc_wdt.* *libsoc.a:cpu_util.* *libsoc.a:rtc_periph.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_clk_init.* *libsoc.a:rtc_time.* *libspi_flash.a:spi_flash_rom_patch.* *libxtensa-debug-module.a:eri.*) .literal.*  .phyiram  .phyiram.* EXCLUDE_FILE(*libapp_trace.a *libesp32.a:panic.* *libhal.a *librtc.a *libgcc.a:lib2funcs.* *libgcov.a *libesp_ringbuf.a *libespcoredump.a:core_dump_port.* *libespcoredump.a:core_dump_flash.* *libespcoredump.a:core_dump_common.* *libespcoredump.a:core_dump_uart.* *libfreertos.a *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libc-psram-workaround.a:lib_a-tzset_r.* *libc-psram-workaround.a:lib_a-s_fpclassify.* *libc-psram-workaround.a:lib_a-refill.* *libc-psram-workaround.a:lib_a-isspace.* *libc-psram-workaround.a:lib_a-div.* *libc-psram-workaround.a:lib_a-strptime.* *libc-psram-workaround.a:lib_a-memchr.* *libc-psram-workaround.a:lib_a-wsetup.* *libc-psram-workaround.a:lib_a-strtoul.* *libc-psram-workaround.a:lib_a-itoa.* *libc-psram-workaround.a:lib_a-tolower.* *libc-psram-workaround.a:lib_a-rand_r.* *libc-psram-workaround.a:lib_a-strchr.* *libc-psram-workaround.a:lib_a-syswrite.* *libc-psram-workaround.a:lib_a-isupper.* *libc-psram-workaround.a:lib_a-tzset.* *libc-psram-workaround.a:lib_a-close.* *libc-psram-workaround.a:lib_a-strndup_r.* *libc-psram-workaround.a:lib_a-strrchr.* *libc-psram-workaround.a:lib_a-rshift.* *libc-psram-workaround.a:lib_a-sbrk.* *libc-psram-workaround.a:lib_a-ctype_.* *libc-psram-workaround.a:lib_a-sf_nan.* *libc-psram-workaround.a:lib_a-wbuf.* *libc-psram-workaround.a:lib_a-utoa.* *libc-psram-workaround.a:isatty.* *libc-psram-workaround.a:lib_a-memccpy.* *libc-psram-workaround.a:lib_a-strncpy.* *libc-psram-workaround.a:lib_a-ldiv.* *libc-psram-workaround.a:lib_a-strftime.* *libc-psram-workaround.a:lib_a-atoi.* *libc-psram-workaround.a:lib_a-bzero.* *libc-psram-workaround.a:lib_a-lcltime.* *libc-psram-workaround.a:lib_a-fwalk.* *libc-psram-workaround.a:lib_a-asctime_r.* *libc-psram-workaround.a:lib_a-open.* *libc-psram-workaround.a:lib_a-fflush.* *libc-psram-workaround.a:lib_a-quorem.* *libc-psram-workaround.a:lib_a-tzlock.* *libc-psram-workaround.a:lib_a-tzvars.* *libc-psram-workaround.a:lib_a-isalnum.* *libc-psram-workaround.a:lib_a-strncasecmp.* *libc-psram-workaround.a:lib_a-ctime.* *libc-psram-workaround.a:lib_a-strcspn.* *libc-psram-workaround.a:lib_a-impure.* *libc-psram-workaround.a:lib_a-environ.* *libc-psram-workaround.a:creat.* *libc-psram-workaround.a:lib_a-longjmp.* *libc-psram-workaround.a:lib_a-abs.* *libc-psram-workaround.a:lib_a-fputwc.* *libc-psram-workaround.a:lib_a-fvwrite.* *libc-psram-workaround.a:lib_a-isalpha.* *libc-psram-workaround.a:lib_a-memset.* *libc-psram-workaround.a:lib_a-syssbrk.* *libc-psram-workaround.a:lib_a-ispunct.* *libc-psram-workaround.a:lib_a-read.* *libc-psram-workaround.a:lib_a-systimes.* *libc-psram-workaround.a:lib_a-memcmp.* *libc-psram-workaround.a:lib_a-wctomb_r.* *libc-psram-workaround.a:lib_a-strlcat.* *libc-psram-workaround.a:lib_a-strnlen.* *libc-psram-workaround.a:lib_a-sysread.* *libc-psram-workaround.a:lib_a-month_lengths.* *libc-psram-workaround.a:lib_a-mktime.* *libc-psram-workaround.a:lib_a-strsep.* *libc-psram-workaround.a:lib_a-sysopen.* *libc-psram-workaround.a:lib_a-isdigit.* *libc-psram-workaround.a:lib_a-isblank.* *libc-psram-workaround.a:lib_a-strndup.* *libc-psram-workaround.a:lib_a-creat.* *libc-psram-workaround.a:lib_a-gmtime.* *libc-psram-workaround.a:lib_a-isprint.* *libc-psram-workaround.a:lib_a-ungetc.* *libc-psram-workaround.a:lib_a-makebuf.* *libc-psram-workaround.a:lib_a-gmtime_r.* *libc-psram-workaround.a:lib_a-strtok_r.* *libc-psram-workaround.a:lib_a-raise.* *libc-psram-workaround.a:lib_a-isascii.* *libc-psram-workaround.a:lib_a-strcpy.* *libc-psram-workaround.a:lib_a-strcmp.* *libc-psram-workaround.a:lib_a-toupper.* *libc-psram-workaround.a:lib_a-envlock.* *libc-psram-workaround.a:lib_a-strlwr.* *libc-psram-workaround.a:lib_a-srand.* *libc-psram-workaround.a:lib_a-memrchr.* *libc-psram-workaround.a:lib_a-strdup.* *libc-psram-workaround.a:lib_a-system.* *libc-psram-workaround.a:lib_a-toascii.* *libc-psram-workaround.a:lib_a-strdup_r.* *libc-psram-workaround.a:lib_a-memcpy.* *libc-psram-workaround.a:lib_a-getenv_r.* *libc-psram-workaround.a:lib_a-stdio.* *libc-psram-workaround.a:lib_a-time.* *libc-psram-workaround.a:lib_a-ctime_r.* *libc-psram-workaround.a:lib_a-isgraph.* *libc-psram-workaround.a:lib_a-sccl.* *libc-psram-workaround.a:lib_a-strlcpy.* *libc-psram-workaround.a:lib_a-atol.* *libc-psram-workaround.a:lib_a-strlen.* *libc-psram-workaround.a:lib_a-strcasecmp.* *libc-psram-workaround.a:lib_a-wcrtomb.* *libc-psram-workaround.a:lib_a-sysclose.* *libc-psram-workaround.a:lib_a-tzcalc_limits.* *libc-psram-workaround.a:lib_a-islower.* *libc-psram-workaround.a:lib_a-setjmp.* *libc-psram-workaround.a:lib_a-timelocal.* *libc-psram-workaround.a:lib_a-gettzinfo.* *libc-psram-workaround.a:lib_a-rand.* *libc-psram-workaround.a:lib_a-strncat.* *libc-psram-workaround.a:lib_a-strcasestr.* *libc-psram-workaround.a:lib_a-strcoll.* *libc-psram-workaround.a:lock.* *libc-psram-workaround.a:lib_a-iscntrl.* *libc-psram-workaround.a:lib_a-lcltime_r.* *libc-psram-workaround.a:lib_a-strtol.* *libc-psram-workaround.a:lib_a-labs.* *libc-psram-workaround.a:lib_a-strupr.* *libc-psram-workaround.a:lib_a-memmove.* *libc-psram-workaround.a:lib_a-strspn.* *libc-psram-workaround.a:lib_a-strcat.* *libc-psram-workaround.a:lib_a-findfp.* *libc-psram-workaround.a:lib_a-fclose.* *libc-psram-workaround.a:lib_a-strncmp.* *libc-psram-workaround.a:lib_a-strstr.* *libc-psram-workaround.a:lib_a-asctime.* *libsoc.a:rtc_pm.* *libsoc.a:rtc_clk.* *libsoc.a:rtc_init.* *libsoc.a:rtc_wdt.* *libsoc.a:cpu_util.* *libsoc.a:rtc_periph.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_clk_init.* *libsoc.a:rtc_time.* *libspi_flash.a:spi_flash_rom_patch.* *libxtensa-debug-module.a:eri.*) .text EXCLUDE_FILE(*libapp_trace.a *libesp32.a:panic.* *libhal.a *librtc.a *libgcc.a:lib2funcs.* *libgcov.a *libesp_ringbuf.a *libespcoredump.a:core_dump_port.* *libespcoredump.a:core_dump_flash.* *libespcoredump.a:core_dump_common.* *libespcoredump.a:core_dump_uart.* *libfreertos.a *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libc-psram-workaround.a:lib_a-tzset_r.* *libc-psram-workaround.a:lib_a-s_fpclassify.* *libc-psram-workaround.a:lib_a-refill.* *libc-psram-workaround.a:lib_a-isspace.* *libc-psram-workaround.a:lib_a-div.* *libc-psram-workaround.a:lib_a-strptime.* *libc-psram-workaround.a:lib_a-memchr.* *libc-psram-workaround.a:lib_a-wsetup.* *libc-psram-workaround.a:lib_a-strtoul.* *libc-psram-workaround.a:lib_a-itoa.* *libc-psram-workaround.a:lib_a-tolower.* *libc-psram-workaround.a:lib_a-rand_r.* *libc-psram-workaround.a:lib_a-strchr.* *libc-psram-workaround.a:lib_a-syswrite.* *libc-psram-workaround.a:lib_a-isupper.* *libc-psram-workaround.a:lib_a-tzset.* *libc-psram-workaround.a:lib_a-close.* *libc-psram-workaround.a:lib_a-strndup_r.* *libc-psram-workaround.a:lib_a-strrchr.* *libc-psram-workaround.a:lib_a-rshift.* *libc-psram-workaround.a:lib_a-sbrk.* *libc-psram-workaround.a:lib_a-ctype_.* *libc-psram-workaround.a:lib_a-sf_nan.* *libc-psram-workaround.a:lib_a-wbuf.* *libc-psram-workaround.a:lib_a-utoa.* *libc-psram-workaround.a:isatty.* *libc-psram-workaround.a:lib_a-memccpy.* *libc-psram-workaround.a:lib_a-strncpy.* *libc-psram-workaround.a:lib_a-ldiv.* *libc-psram-workaround.a:lib_a-strftime.* *libc-psram-workaround.a:lib_a-atoi.* *libc-psram-workaround.a:lib_a-bzero.* *libc-psram-workaround.a:lib_a-lcltime.* *libc-psram-workaround.a:lib_a-fwalk.* *libc-psram-workaround.a:lib_a-asctime_r.* *libc-psram-workaround.a:lib_a-open.* *libc-psram-workaround.a:lib_a-fflush.* *libc-psram-workaround.a:lib_a-quorem.* *libc-psram-workaround.a:lib_a-tzlock.* *libc-psram-workaround.a:lib_a-tzvars.* *libc-psram-workaround.a:lib_a-isalnum.* *libc-psram-workaround.a:lib_a-strncasecmp.* *libc-psram-workaround.a:lib_a-ctime.* *libc-psram-workaround.a:lib_a-strcspn.* *libc-psram-workaround.a:lib_a-impure.* *libc-psram-workaround.a:lib_a-environ.* *libc-psram-workaround.a:creat.* *libc-psram-workaround.a:lib_a-longjmp.* *libc-psram-workaround.a:lib_a-abs.* *libc-psram-workaround.a:lib_a-fputwc.* *libc-psram-workaround.a:lib_a-fvwrite.* *libc-psram-workaround.a:lib_a-isalpha.* *libc-psram-workaround.a:lib_a-memset.* *libc-psram-workaround.a:lib_a-syssbrk.* *libc-psram-workaround.a:lib_a-ispunct.* *libc-psram-workaround.a:lib_a-read.* *libc-psram-workaround.a:lib_a-systimes.* *libc-psram-workaround.a:lib_a-memcmp.* *libc-psram-workaround.a:lib_a-wctomb_r.* *libc-psram-workaround.a:lib_a-strlcat.* *libc-psram-workaround.a:lib_a-strnlen.* *libc-psram-workaround.a:lib_a-sysread.* *libc-psram-workaround.a:lib_a-month_lengths.* *libc-psram-workaround.a:lib_a-mktime.* *libc-psram-workaround.a:lib_a-strsep.* *libc-psram-workaround.a:lib_a-sysopen.* *libc-psram-workaround.a:lib_a-isdigit.* *libc-psram-workaround.a:lib_a-isblank.* *libc-psram-workaround.a:lib_a-strndup.* *libc-psram-workaround.a:lib_a-creat.* *libc-psram-workaround.a:lib_a-gmtime.* *libc-psram-workaround.a:lib_a-isprint.* *libc-psram-workaround.a:lib_a-ungetc.* *libc-psram-workaround.a:lib_a-makebuf.* *libc-psram-workaround.a:lib_a-gmtime_r.* *libc-psram-workaround.a:lib_a-strtok_r.* *libc-psram-workaround.a:lib_a-raise.* *libc-psram-workaround.a:lib_a-isascii.* *libc-psram-workaround.a:lib_a-strcpy.* *libc-psram-workaround.a:lib_a-strcmp.* *libc-psram-workaround.a:lib_a-toupper.* *libc-psram-workaround.a:lib_a-envlock.* *libc-psram-workaround.a:lib_a-strlwr.* *libc-psram-workaround.a:lib_a-srand.* *libc-psram-workaround.a:lib_a-memrchr.* *libc-psram-workaround.a:lib_a-strdup.* *libc-psram-workaround.a:lib_a-system.* *libc-psram-workaround.a:lib_a-toascii.* *libc-psram-workaround.a:lib_a-strdup_r.* *libc-psram-workaround.a:lib_a-memcpy.* *libc-psram-workaround.a:lib_a-getenv_r.* *libc-psram-workaround.a:lib_a-stdio.* *libc-psram-workaround.a:lib_a-time.* *libc-psram-workaround.a:lib_a-ctime_r.* *libc-psram-workaround.a:lib_a-isgraph.* *libc-psram-workaround.a:lib_a-sccl.* *libc-psram-workaround.a:lib_a-strlcpy.* *libc-psram-workaround.a:lib_a-atol.* *libc-psram-workaround.a:lib_a-strlen.* *libc-psram-workaround.a:lib_a-strcasecmp.* *libc-psram-workaround.a:lib_a-wcrtomb.* *libc-psram-workaround.a:lib_a-sysclose.* *libc-psram-workaround.a:lib_a-tzcalc_limits.* *libc-psram-workaround.a:lib_a-islower.* *libc-psram-workaround.a:lib_a-setjmp.* *libc-psram-workaround.a:lib_a-timelocal.* *libc-psram-workaround.a:lib_a-gettzinfo.* *libc-psram-workaround.a:lib_a-rand.* *libc-psram-workaround.a:lib_a-strncat.* *libc-psram-workaround.a:lib_a-strcasestr.* *libc-psram-workaround.a:lib_a-strcoll.* *libc-psram-workaround.a:lock.* *libc-psram-workaround.a:lib_a-iscntrl.* *libc-psram-workaround.a:lib_a-lcltime_r.* *libc-psram-workaround.a:lib_a-strtol.* *libc-psram-workaround.a:lib_a-labs.* *libc-psram-workaround.a:lib_a-strupr.* *libc-psram-workaround.a:lib_a-memmove.* *libc-psram-workaround.a:lib_a-strspn.* *libc-psram-workaround.a:lib_a-strcat.* *libc-psram-workaround.a:lib_a-findfp.* *libc-psram-workaround.a:lib_a-fclose.* *libc-psram-workaround.a:lib_a-strncmp.* *libc-psram-workaround.a:lib_a-strstr.* *libc-psram-workaround.a:lib_a-asctime.* *libsoc.a:rtc_pm.* *libsoc.a:rtc_clk.* *libsoc.a:rtc_init.* *libsoc.a:rtc_wdt.* *libsoc.a:cpu_util.* *libsoc.a:rtc_periph.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_clk_init.* *libsoc.a:rtc_time.* *libspi_flash.a:spi_flash_rom_patch.* *libxtensa-debug-module.a:eri.*) .text.*  .wifi0iram  .wifi0iram.*  .wifirxiram  .wifirxiram.*)

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
  } >iram0_2_seg
}
