/* Automatically generated file; DO NOT EDIT */
/* Espressif IoT Development Framework Linker Script */
/* Generated from: /home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/esp_system/ld/esp32/sections.ld.in */

/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
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

    *(.rtc.literal .rtc.text .rtc.text.*)

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
    *(.rtc.fast.coredump .rtc.fast.coredump.*)
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

    _coredump_rtc_start = ABSOLUTE(.);
    *(.rtc.coredump .rtc.coredump.*)
    _coredump_rtc_end = ABSOLUTE(.);
    *(.rtc.data .rtc.data.*)
    *(.rtc.rodata .rtc.rodata.*)

    *rtc_wake_stub*.*(.data .rodata .data.* .rodata.* .bss .bss.*)
    _rtc_data_end = ABSOLUTE(.);

  } > rtc_data_location

  /* RTC bss, from any source file named rtc_wake_stub*.c */
  .rtc.bss (NOLOAD) :
  {
    _rtc_bss_start = ABSOLUTE(.);
    *rtc_wake_stub*.*(.bss .bss.*)
    *rtc_wake_stub*.*(COMMON)

    *(.rtc.bss)

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
    _vector_table = ABSOLUTE(.);
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

    *(.iram1 .iram1.*)
    *libapp_trace.a:app_trace.*(.literal .literal.* .text .text.*)
    *libapp_trace.a:app_trace_util.*(.literal .literal.* .text .text.*)
    *libc.a:creat.*(.literal .literal.* .text .text.*)
    *libc.a:isatty.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-abs.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-asctime.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-asctime_r.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-atoi.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-atol.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-bzero.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-close.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-creat.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-ctime.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-ctime_r.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-ctype_.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-div.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-environ.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-envlock.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-fclose.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-fflush.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-findfp.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-fputwc.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-fvwrite.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-fwalk.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-getenv_r.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-gettzinfo.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-gmtime.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-gmtime_r.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-impure.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-isalnum.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-isalpha.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-isascii.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-isblank.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-iscntrl.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-isdigit.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-isgraph.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-islower.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-isprint.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-ispunct.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-isspace.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-isupper.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-itoa.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-labs.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-lcltime.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-lcltime_r.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-ldiv.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-longjmp.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-makebuf.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-memccpy.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-memchr.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-memcmp.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-memcpy.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-memmove.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-memrchr.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-memset.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-mktime.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-month_lengths.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-open.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-quorem.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-raise.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-rand.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-rand_r.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-read.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-refill.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-rshift.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-s_fpclassify.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-sbrk.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-sccl.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-setjmp.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-sf_nan.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-srand.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-stdio.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strcasecmp.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strcasestr.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strcat.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strchr.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strcmp.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strcoll.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strcpy.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strcspn.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strdup.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strdup_r.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strftime.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strlcat.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strlcpy.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strlen.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strlwr.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strncasecmp.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strncat.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strncmp.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strncpy.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strndup.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strndup_r.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strnlen.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strptime.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strrchr.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strsep.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strspn.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strstr.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strtok_r.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strtol.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strtoul.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-strupr.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-sysclose.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-sysopen.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-sysread.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-syssbrk.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-system.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-systimes.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-syswrite.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-time.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-timelocal.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-toascii.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-tolower.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-toupper.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-tzcalc_limits.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-tzlock.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-tzset.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-tzset_r.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-tzvars.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-ungetc.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-utoa.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-wbuf.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-wcrtomb.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-wctomb_r.*(.literal .literal.* .text .text.*)
    *libc.a:lib_a-wsetup.*(.literal .literal.* .text .text.*)
    *libc.a:lock.*(.literal .literal.* .text .text.*)
    *libesp_event.a:default_event_loop.*(.literal.esp_event_isr_post .text.esp_event_isr_post)
    *libesp_event.a:esp_event.*(.literal.esp_event_isr_post_to .text.esp_event_isr_post_to)
    *libesp_hw_support.a:cpu_util.*(.literal .literal.* .text .text.*)
    *libesp_hw_support.a:rtc_clk.*(.literal .literal.* .text .text.*)
    *libesp_hw_support.a:rtc_init.*(.literal.rtc_vddsdio_set_config .text.rtc_vddsdio_set_config)
    *libesp_hw_support.a:rtc_pm.*(.literal .literal.* .text .text.*)
    *libesp_hw_support.a:rtc_sleep.*(.literal .literal.* .text .text.*)
    *libesp_hw_support.a:rtc_time.*(.literal .literal.* .text .text.*)
    *libesp_hw_support.a:rtc_wdt.*(.literal .literal.* .text .text.*)
    *libesp_ringbuf.a:(.literal .literal.* .text .text.*)
    *libesp_system.a:esp_err.*(.literal .literal.* .text .text.*)
    *libesp_system.a:esp_system.*(.literal.esp_system_abort .text.esp_system_abort)
    *libesp_system.a:ubsan.*(.literal .literal.* .text .text.*)
    *libfreertos.a:(EXCLUDE_FILE(*libfreertos.a:port.* *libfreertos.a:port_common.*) .literal EXCLUDE_FILE(*libfreertos.a:port.* *libfreertos.a:port_common.*) .literal.* EXCLUDE_FILE(*libfreertos.a:port.* *libfreertos.a:port_common.*) .text EXCLUDE_FILE(*libfreertos.a:port.* *libfreertos.a:port_common.*) .text.*)
    *libfreertos.a:port.*(.literal.pxPortInitialiseStack .literal.unlikely.vPortEndScheduler .literal.vApplicationStackOverflowHook .literal.vPortAssertIfInISR .literal.vPortExitCritical .literal.vPortExitCriticalCompliance .literal.vPortReleaseTaskMPUSettings .literal.vPortSetStackWatchpoint .literal.vPortYieldOtherCore .literal.xPortEnterCriticalTimeout .literal.xPortEnterCriticalTimeoutCompliance .literal.xPortInIsrContext .literal.xPortStartScheduler .text .text.pxPortInitialiseStack .text.unlikely.vPortEndScheduler .text.vApplicationStackOverflowHook .text.vPortAssertIfInISR .text.vPortExitCritical .text.vPortExitCriticalCompliance .text.vPortReleaseTaskMPUSettings .text.vPortSetStackWatchpoint .text.vPortStoreTaskMPUSettings .text.vPortYieldOtherCore .text.xPortEnterCriticalTimeout .text.xPortEnterCriticalTimeoutCompliance .text.xPortGetTickRateHz .text.xPortInIsrContext .text.xPortStartScheduler)
    *libfreertos.a:port_common.*(.literal.esp_startup_start_app_common .literal.vApplicationGetIdleTaskMemory .literal.vApplicationGetTimerTaskMemory .literal.xPortCheckValidTCBMem .literal.xPortcheckValidStackMem .text .text.esp_startup_start_app_common .text.vApplicationGetIdleTaskMemory .text.vApplicationGetTimerTaskMemory .text.xPortCheckValidTCBMem .text.xPortcheckValidStackMem)
    *libgcc.a:lib2funcs.*(.literal .literal.* .text .text.*)
    *libgcov.a:(.literal .literal.* .text .text.*)
    *libhal.a:cpu_hal.*(.literal .literal.* .text .text.*)
    *libhal.a:i2c_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:ledc_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:soc_hal.*(.literal .literal.* .text .text.*)
    *libhal.a:spi_flash_encrypt_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:spi_flash_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:spi_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:spi_slave_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:wdt_hal_iram.*(.literal .literal.* .text .text.*)
    *libheap.a:heap_tlsf.*(.literal .literal.* .text .text.*)
    *libheap.a:multi_heap.*(.literal .literal.* .text .text.*)
    *libheap.a:multi_heap_poisoning.*(.literal .literal.* .text .text.*)
    *liblog.a:log.*(.literal.esp_log_write .text.esp_log_write)
    *liblog.a:log_freertos.*(.literal.esp_log_early_timestamp .text.esp_log_early_timestamp)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_lock .text.esp_log_impl_lock)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_lock_timeout .text.esp_log_impl_lock_timeout)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_unlock .text.esp_log_impl_unlock)
    *liblog.a:log_freertos.*(.literal.esp_log_timestamp .text.esp_log_timestamp)
    *libnewlib.a:abort.*(.literal .literal.* .text .text.*)
    *libnewlib.a:assert.*(.literal .literal.* .text .text.*)
    *libnewlib.a:heap.*(.literal .literal.* .text .text.*)
    *libnewlib.a:stdatomic.*(.literal .literal.* .text .text.*)
    *librtc.a:(.literal .literal.* .text .text.*)
    *libsoc.a:lldesc.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:memspi_host_driver.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_boya.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_gd.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_generic.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_issi.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_mxic.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_th.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_winbond.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_rom_patch.*(.literal .literal.* .text .text.*)
    *libxt_hal.a:(.literal .literal.* .text .text.*)
    *libxtensa.a:eri.*(.literal .literal.* .text .text.*)
    *libxtensa.a:xtensa_intr_asm.*(.literal .literal.* .text .text.*)

  } > iram0_0_seg

  .dram0.data :
  {
    _data_start = ABSOLUTE(.);
    *(.gnu.linkonce.d.*)
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    *(.jcr)

    _esp_system_init_fn_array_start = ABSOLUTE(.);
    KEEP (*(SORT(.esp_system_init_fn) SORT(.esp_system_init_fn.*)))
    _esp_system_init_fn_array_end = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libbt.a *libbtdm_app.a *libnimble.a) .data EXCLUDE_FILE(*libbt.a *libbtdm_app.a *libnimble.a) .data.*)
    *(.dram1 .dram1.*)
    _coredump_dram_start = ABSOLUTE(.);
    *(.dram2.coredump .dram2.coredump.*)
    _coredump_dram_end = ABSOLUTE(.);
    *libapp_trace.a:app_trace.*(.rodata .rodata.*)
    *libapp_trace.a:app_trace_util.*(.rodata .rodata.*)
    _bt_data_start = ABSOLUTE(.);
    *libbt.a:(.data .data.*)
    . = ALIGN(4);
    _bt_data_end = ABSOLUTE(.);
    _btdm_data_start = ABSOLUTE(.);
    *libbtdm_app.a:(.data .data.*)
    . = ALIGN(4);
    _btdm_data_end = ABSOLUTE(.);
    *libc.a:creat.*(.rodata .rodata.*)
    *libc.a:isatty.*(.rodata .rodata.*)
    *libc.a:lib_a-abs.*(.rodata .rodata.*)
    *libc.a:lib_a-asctime.*(.rodata .rodata.*)
    *libc.a:lib_a-asctime_r.*(.rodata .rodata.*)
    *libc.a:lib_a-atoi.*(.rodata .rodata.*)
    *libc.a:lib_a-atol.*(.rodata .rodata.*)
    *libc.a:lib_a-bzero.*(.rodata .rodata.*)
    *libc.a:lib_a-close.*(.rodata .rodata.*)
    *libc.a:lib_a-creat.*(.rodata .rodata.*)
    *libc.a:lib_a-ctime.*(.rodata .rodata.*)
    *libc.a:lib_a-ctime_r.*(.rodata .rodata.*)
    *libc.a:lib_a-ctype_.*(.rodata .rodata.*)
    *libc.a:lib_a-div.*(.rodata .rodata.*)
    *libc.a:lib_a-environ.*(.rodata .rodata.*)
    *libc.a:lib_a-envlock.*(.rodata .rodata.*)
    *libc.a:lib_a-fclose.*(.rodata .rodata.*)
    *libc.a:lib_a-fflush.*(.rodata .rodata.*)
    *libc.a:lib_a-findfp.*(.rodata .rodata.*)
    *libc.a:lib_a-fputwc.*(.rodata .rodata.*)
    *libc.a:lib_a-fvwrite.*(.rodata .rodata.*)
    *libc.a:lib_a-fwalk.*(.rodata .rodata.*)
    *libc.a:lib_a-getenv_r.*(.rodata .rodata.*)
    *libc.a:lib_a-gettzinfo.*(.rodata .rodata.*)
    *libc.a:lib_a-gmtime.*(.rodata .rodata.*)
    *libc.a:lib_a-gmtime_r.*(.rodata .rodata.*)
    *libc.a:lib_a-impure.*(.rodata .rodata.*)
    *libc.a:lib_a-isalnum.*(.rodata .rodata.*)
    *libc.a:lib_a-isalpha.*(.rodata .rodata.*)
    *libc.a:lib_a-isascii.*(.rodata .rodata.*)
    *libc.a:lib_a-isblank.*(.rodata .rodata.*)
    *libc.a:lib_a-iscntrl.*(.rodata .rodata.*)
    *libc.a:lib_a-isdigit.*(.rodata .rodata.*)
    *libc.a:lib_a-isgraph.*(.rodata .rodata.*)
    *libc.a:lib_a-islower.*(.rodata .rodata.*)
    *libc.a:lib_a-isprint.*(.rodata .rodata.*)
    *libc.a:lib_a-ispunct.*(.rodata .rodata.*)
    *libc.a:lib_a-isspace.*(.rodata .rodata.*)
    *libc.a:lib_a-isupper.*(.rodata .rodata.*)
    *libc.a:lib_a-itoa.*(.rodata .rodata.*)
    *libc.a:lib_a-labs.*(.rodata .rodata.*)
    *libc.a:lib_a-lcltime.*(.rodata .rodata.*)
    *libc.a:lib_a-lcltime_r.*(.rodata .rodata.*)
    *libc.a:lib_a-ldiv.*(.rodata .rodata.*)
    *libc.a:lib_a-longjmp.*(.rodata .rodata.*)
    *libc.a:lib_a-makebuf.*(.rodata .rodata.*)
    *libc.a:lib_a-memccpy.*(.rodata .rodata.*)
    *libc.a:lib_a-memchr.*(.rodata .rodata.*)
    *libc.a:lib_a-memcmp.*(.rodata .rodata.*)
    *libc.a:lib_a-memcpy.*(.rodata .rodata.*)
    *libc.a:lib_a-memmove.*(.rodata .rodata.*)
    *libc.a:lib_a-memrchr.*(.rodata .rodata.*)
    *libc.a:lib_a-memset.*(.rodata .rodata.*)
    *libc.a:lib_a-mktime.*(.rodata .rodata.*)
    *libc.a:lib_a-month_lengths.*(.rodata .rodata.*)
    *libc.a:lib_a-open.*(.rodata .rodata.*)
    *libc.a:lib_a-quorem.*(.rodata .rodata.*)
    *libc.a:lib_a-raise.*(.rodata .rodata.*)
    *libc.a:lib_a-rand.*(.rodata .rodata.*)
    *libc.a:lib_a-rand_r.*(.rodata .rodata.*)
    *libc.a:lib_a-read.*(.rodata .rodata.*)
    *libc.a:lib_a-refill.*(.rodata .rodata.*)
    *libc.a:lib_a-rshift.*(.rodata .rodata.*)
    *libc.a:lib_a-s_fpclassify.*(.rodata .rodata.*)
    *libc.a:lib_a-sbrk.*(.rodata .rodata.*)
    *libc.a:lib_a-sccl.*(.rodata .rodata.*)
    *libc.a:lib_a-setjmp.*(.rodata .rodata.*)
    *libc.a:lib_a-sf_nan.*(.rodata .rodata.*)
    *libc.a:lib_a-srand.*(.rodata .rodata.*)
    *libc.a:lib_a-stdio.*(.rodata .rodata.*)
    *libc.a:lib_a-strcasecmp.*(.rodata .rodata.*)
    *libc.a:lib_a-strcasestr.*(.rodata .rodata.*)
    *libc.a:lib_a-strcat.*(.rodata .rodata.*)
    *libc.a:lib_a-strchr.*(.rodata .rodata.*)
    *libc.a:lib_a-strcmp.*(.rodata .rodata.*)
    *libc.a:lib_a-strcoll.*(.rodata .rodata.*)
    *libc.a:lib_a-strcpy.*(.rodata .rodata.*)
    *libc.a:lib_a-strcspn.*(.rodata .rodata.*)
    *libc.a:lib_a-strdup.*(.rodata .rodata.*)
    *libc.a:lib_a-strdup_r.*(.rodata .rodata.*)
    *libc.a:lib_a-strftime.*(.rodata .rodata.*)
    *libc.a:lib_a-strlcat.*(.rodata .rodata.*)
    *libc.a:lib_a-strlcpy.*(.rodata .rodata.*)
    *libc.a:lib_a-strlen.*(.rodata .rodata.*)
    *libc.a:lib_a-strlwr.*(.rodata .rodata.*)
    *libc.a:lib_a-strncasecmp.*(.rodata .rodata.*)
    *libc.a:lib_a-strncat.*(.rodata .rodata.*)
    *libc.a:lib_a-strncmp.*(.rodata .rodata.*)
    *libc.a:lib_a-strncpy.*(.rodata .rodata.*)
    *libc.a:lib_a-strndup.*(.rodata .rodata.*)
    *libc.a:lib_a-strndup_r.*(.rodata .rodata.*)
    *libc.a:lib_a-strnlen.*(.rodata .rodata.*)
    *libc.a:lib_a-strptime.*(.rodata .rodata.*)
    *libc.a:lib_a-strrchr.*(.rodata .rodata.*)
    *libc.a:lib_a-strsep.*(.rodata .rodata.*)
    *libc.a:lib_a-strspn.*(.rodata .rodata.*)
    *libc.a:lib_a-strstr.*(.rodata .rodata.*)
    *libc.a:lib_a-strtok_r.*(.rodata .rodata.*)
    *libc.a:lib_a-strtol.*(.rodata .rodata.*)
    *libc.a:lib_a-strtoul.*(.rodata .rodata.*)
    *libc.a:lib_a-strupr.*(.rodata .rodata.*)
    *libc.a:lib_a-sysclose.*(.rodata .rodata.*)
    *libc.a:lib_a-sysopen.*(.rodata .rodata.*)
    *libc.a:lib_a-sysread.*(.rodata .rodata.*)
    *libc.a:lib_a-syssbrk.*(.rodata .rodata.*)
    *libc.a:lib_a-system.*(.rodata .rodata.*)
    *libc.a:lib_a-systimes.*(.rodata .rodata.*)
    *libc.a:lib_a-syswrite.*(.rodata .rodata.*)
    *libc.a:lib_a-time.*(.rodata .rodata.*)
    *libc.a:lib_a-timelocal.*(.rodata .rodata.*)
    *libc.a:lib_a-toascii.*(.rodata .rodata.*)
    *libc.a:lib_a-tolower.*(.rodata .rodata.*)
    *libc.a:lib_a-toupper.*(.rodata .rodata.*)
    *libc.a:lib_a-tzcalc_limits.*(.rodata .rodata.*)
    *libc.a:lib_a-tzlock.*(.rodata .rodata.*)
    *libc.a:lib_a-tzset.*(.rodata .rodata.*)
    *libc.a:lib_a-tzset_r.*(.rodata .rodata.*)
    *libc.a:lib_a-tzvars.*(.rodata .rodata.*)
    *libc.a:lib_a-ungetc.*(.rodata .rodata.*)
    *libc.a:lib_a-utoa.*(.rodata .rodata.*)
    *libc.a:lib_a-wbuf.*(.rodata .rodata.*)
    *libc.a:lib_a-wcrtomb.*(.rodata .rodata.*)
    *libc.a:lib_a-wctomb_r.*(.rodata .rodata.*)
    *libc.a:lib_a-wsetup.*(.rodata .rodata.*)
    *libc.a:lock.*(.rodata .rodata.*)
    *libesp_hw_support.a:rtc_clk.*(.rodata .rodata.*)
    *libesp_system.a:esp_err.*(.rodata .rodata.*)
    *libesp_system.a:ubsan.*(.rodata .rodata.*)
    *libgcov.a:(.rodata .rodata.*)
    *libhal.a:cpu_hal.*(.rodata .rodata.*)
    *libhal.a:i2c_hal_iram.*(.rodata .rodata.*)
    *libhal.a:ledc_hal_iram.*(.rodata .rodata.*)
    *libhal.a:soc_hal.*(.rodata .rodata.*)
    *libhal.a:spi_flash_encrypt_hal_iram.*(.rodata .rodata.*)
    *libhal.a:spi_flash_hal_iram.*(.rodata .rodata.*)
    *libhal.a:spi_hal_iram.*(.rodata .rodata.*)
    *libhal.a:spi_slave_hal_iram.*(.rodata .rodata.*)
    *libhal.a:wdt_hal_iram.*(.rodata .rodata.*)
    *libheap.a:heap_tlsf.*(.rodata .rodata.*)
    *libheap.a:multi_heap.*(.rodata .rodata.*)
    *libheap.a:multi_heap_poisoning.*(.rodata .rodata.*)
    *libnewlib.a:abort.*(.rodata .rodata.*)
    *libnewlib.a:assert.*(.rodata .rodata.*)
    *libnewlib.a:heap.*(.rodata .rodata.*)
    *libnewlib.a:stdatomic.*(.rodata .rodata.*)
    _nimble_data_start = ABSOLUTE(.);
    *libnimble.a:(.data .data.*)
    . = ALIGN(4);
    _nimble_data_end = ABSOLUTE(.);
    *libphy.a:(.rodata .rodata.*)
    *libsoc.a:lldesc.*(.rodata .rodata.*)
    *libspi_flash.a:memspi_host_driver.*(.rodata .rodata.*)
    *libspi_flash.a:spi_flash_chip_boya.*(.rodata .rodata.*)
    *libspi_flash.a:spi_flash_chip_gd.*(.rodata .rodata.*)
    *libspi_flash.a:spi_flash_chip_generic.*(.rodata .rodata.*)
    *libspi_flash.a:spi_flash_chip_issi.*(.rodata .rodata.*)
    *libspi_flash.a:spi_flash_chip_mxic.*(.rodata .rodata.*)
    *libspi_flash.a:spi_flash_chip_th.*(.rodata .rodata.*)
    *libspi_flash.a:spi_flash_chip_winbond.*(.rodata .rodata.*)
    *libspi_flash.a:spi_flash_rom_patch.*(.rodata .rodata.*)

    _data_end = ABSOLUTE(.);
    . = ALIGN(4);
  } > dram0_0_seg

  /**
   * This section holds data that won't be initialised when startup.
   * This section locates in External RAM region.
   */
  .ext_ram_noinit (NOLOAD) :
  {
    _ext_ram_noinit_start = ABSOLUTE(.);
    *(.ext_ram_noinit*)
    . = ALIGN(4);
    _ext_ram_noinit_end = ABSOLUTE(.);
  } > extern_ram_seg

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

   /* external memory bss, from any global variable with EXT_RAM_ATTR attribute*/
  .ext_ram.bss (NOLOAD) :
  {
    _ext_ram_bss_start = ABSOLUTE(.);


    . = ALIGN(4);
    _ext_ram_bss_end = ABSOLUTE(.);
  } > extern_ram_seg

  /* Shared RAM */
  .dram0.bss (NOLOAD) :
  {
    . = ALIGN (8);
    _bss_start = ABSOLUTE(.);

    *(.bss .bss.*)
    *(.ext_ram.bss .ext_ram.bss.*)
    *(.dynbss .dynsbss .gnu.linkonce.b .gnu.linkonce.b.* .gnu.linkonce.sb .gnu.linkonce.sb.* .gnu.linkonce.sb2 .gnu.linkonce.sb2.* .sbss .sbss.* .sbss2 .sbss2.* .scommon .share.mem)
    *(COMMON)
    _bt_bss_start = ABSOLUTE(.);
    *libbt.a:(.bss .bss.* COMMON)
    . = ALIGN(4);
    _bt_bss_end = ABSOLUTE(.);
    _btdm_bss_start = ABSOLUTE(.);
    *libbtdm_app.a:(.bss .bss.* COMMON)
    . = ALIGN(4);
    _btdm_bss_end = ABSOLUTE(.);
    _nimble_bss_start = ABSOLUTE(.);
    *libnimble.a:(.bss .bss.* COMMON)
    . = ALIGN(4);
    _nimble_bss_end = ABSOLUTE(.);

    . = ALIGN (8);
    _bss_end = ABSOLUTE(.);
  } > dram0_0_seg

  ASSERT(((_bss_end - ORIGIN(dram0_0_seg)) <= LENGTH(dram0_0_seg)),
          "DRAM segment data does not fit.")

  .flash.appdesc : ALIGN(0x10)
  {
    _rodata_start = ABSOLUTE(.);

    *(.rodata_desc .rodata_desc.*)               /* Should be the first.  App version info.        DO NOT PUT ANYTHING BEFORE IT! */
    *(.rodata_custom_desc .rodata_custom_desc.*) /* Should be the second. Custom app version info. DO NOT PUT ANYTHING BEFORE IT! */

    /* Create an empty gap within this section. Thanks to this, the end of this
     * section will match .flah.rodata's begin address. Thus, both sections
     * will be merged when creating the final bin image. */
    . = ALIGN(ALIGNOF(.flash.rodata));
  } >default_rodata_seg

  .flash.rodata : ALIGN(0x10)
  {
    _flash_rodata_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libgcov.a *libphy.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libc.a:creat.* *libc.a:isatty.* *libc.a:lib_a-abs.* *libc.a:lib_a-asctime.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-atoi.* *libc.a:lib_a-atol.* *libc.a:lib_a-bzero.* *libc.a:lib_a-close.* *libc.a:lib_a-creat.* *libc.a:lib_a-ctime.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-div.* *libc.a:lib_a-environ.* *libc.a:lib_a-envlock.* *libc.a:lib_a-fclose.* *libc.a:lib_a-fflush.* *libc.a:lib_a-findfp.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-impure.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-isblank.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-islower.* *libc.a:lib_a-isprint.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isupper.* *libc.a:lib_a-itoa.* *libc.a:lib_a-labs.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-memchr.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-memmove.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-memset.* *libc.a:lib_a-mktime.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-open.* *libc.a:lib_a-quorem.* *libc.a:lib_a-raise.* *libc.a:lib_a-rand.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-read.* *libc.a:lib_a-refill.* *libc.a:lib_a-rshift.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-sccl.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-srand.* *libc.a:lib_a-stdio.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-strcspn.* *libc.a:lib_a-strdup.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strftime.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-strlen.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-strncat.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-strptime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-strspn.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strtol.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strupr.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-sysread.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-system.* *libc.a:lib_a-systimes.* *libc.a:lib_a-syswrite.* *libc.a:lib_a-time.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-toascii.* *libc.a:lib_a-tolower.* *libc.a:lib_a-toupper.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-tzset.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-utoa.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-wsetup.* *libc.a:lock.* *libesp_hw_support.a:rtc_clk.* *libesp_system.a:esp_err.* *libesp_system.a:ubsan.* *libhal.a:cpu_hal.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libheap.a:heap_tlsf.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.* *libspi_flash.a:spi_flash_rom_patch.*) .rodata EXCLUDE_FILE(*libgcov.a *libphy.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libc.a:creat.* *libc.a:isatty.* *libc.a:lib_a-abs.* *libc.a:lib_a-asctime.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-atoi.* *libc.a:lib_a-atol.* *libc.a:lib_a-bzero.* *libc.a:lib_a-close.* *libc.a:lib_a-creat.* *libc.a:lib_a-ctime.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-div.* *libc.a:lib_a-environ.* *libc.a:lib_a-envlock.* *libc.a:lib_a-fclose.* *libc.a:lib_a-fflush.* *libc.a:lib_a-findfp.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-impure.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-isblank.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-islower.* *libc.a:lib_a-isprint.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isupper.* *libc.a:lib_a-itoa.* *libc.a:lib_a-labs.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-memchr.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-memmove.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-memset.* *libc.a:lib_a-mktime.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-open.* *libc.a:lib_a-quorem.* *libc.a:lib_a-raise.* *libc.a:lib_a-rand.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-read.* *libc.a:lib_a-refill.* *libc.a:lib_a-rshift.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-sccl.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-srand.* *libc.a:lib_a-stdio.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-strcspn.* *libc.a:lib_a-strdup.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strftime.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-strlen.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-strncat.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-strptime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-strspn.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strtol.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strupr.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-sysread.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-system.* *libc.a:lib_a-systimes.* *libc.a:lib_a-syswrite.* *libc.a:lib_a-time.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-toascii.* *libc.a:lib_a-tolower.* *libc.a:lib_a-toupper.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-tzset.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-utoa.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-wsetup.* *libc.a:lock.* *libesp_hw_support.a:rtc_clk.* *libesp_system.a:esp_err.* *libesp_system.a:ubsan.* *libhal.a:cpu_hal.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libheap.a:heap_tlsf.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.* *libspi_flash.a:spi_flash_rom_patch.*) .rodata.*)
    *(.rodata_wlog_error .rodata_wlog_error.*)


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

  _flash_rodata_align = ALIGNOF(.flash.rodata);

  .flash.rodata_noload (NOLOAD) :
  {
    . = ALIGN (4);
    *(.rodata_wlog_debug .rodata_wlog_debug.*)
    *(.rodata_wlog_info .rodata_wlog_info.*)
    *(.rodata_wlog_verbose .rodata_wlog_verbose.*)
    *(.rodata_wlog_warning .rodata_wlog_warning.*)
  } > default_rodata_seg

  .flash.text :
  {
    _stext = .;
    _text_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libesp_ringbuf.a *libfreertos.a *libgcov.a *librtc.a *libxt_hal.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libc.a:creat.* *libc.a:isatty.* *libc.a:lib_a-abs.* *libc.a:lib_a-asctime.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-atoi.* *libc.a:lib_a-atol.* *libc.a:lib_a-bzero.* *libc.a:lib_a-close.* *libc.a:lib_a-creat.* *libc.a:lib_a-ctime.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-div.* *libc.a:lib_a-environ.* *libc.a:lib_a-envlock.* *libc.a:lib_a-fclose.* *libc.a:lib_a-fflush.* *libc.a:lib_a-findfp.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-impure.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-isblank.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-islower.* *libc.a:lib_a-isprint.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isupper.* *libc.a:lib_a-itoa.* *libc.a:lib_a-labs.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-memchr.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-memmove.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-memset.* *libc.a:lib_a-mktime.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-open.* *libc.a:lib_a-quorem.* *libc.a:lib_a-raise.* *libc.a:lib_a-rand.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-read.* *libc.a:lib_a-refill.* *libc.a:lib_a-rshift.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-sccl.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-srand.* *libc.a:lib_a-stdio.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-strcspn.* *libc.a:lib_a-strdup.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strftime.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-strlen.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-strncat.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-strptime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-strspn.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strtol.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strupr.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-sysread.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-system.* *libc.a:lib_a-systimes.* *libc.a:lib_a-syswrite.* *libc.a:lib_a-time.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-toascii.* *libc.a:lib_a-tolower.* *libc.a:lib_a-toupper.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-tzset.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-utoa.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-wsetup.* *libc.a:lock.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *libesp_hw_support.a:cpu_util.* *libesp_hw_support.a:rtc_clk.* *libesp_hw_support.a:rtc_init.* *libesp_hw_support.a:rtc_pm.* *libesp_hw_support.a:rtc_sleep.* *libesp_hw_support.a:rtc_time.* *libesp_hw_support.a:rtc_wdt.* *libesp_system.a:esp_err.* *libesp_system.a:esp_system.* *libesp_system.a:ubsan.* *libgcc.a:lib2funcs.* *libhal.a:cpu_hal.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libheap.a:heap_tlsf.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *liblog.a:log.* *liblog.a:log_freertos.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.* *libspi_flash.a:spi_flash_rom_patch.* *libxtensa.a:eri.* *libxtensa.a:xtensa_intr_asm.*) .literal EXCLUDE_FILE(*libesp_ringbuf.a *libfreertos.a *libgcov.a *librtc.a *libxt_hal.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libc.a:creat.* *libc.a:isatty.* *libc.a:lib_a-abs.* *libc.a:lib_a-asctime.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-atoi.* *libc.a:lib_a-atol.* *libc.a:lib_a-bzero.* *libc.a:lib_a-close.* *libc.a:lib_a-creat.* *libc.a:lib_a-ctime.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-div.* *libc.a:lib_a-environ.* *libc.a:lib_a-envlock.* *libc.a:lib_a-fclose.* *libc.a:lib_a-fflush.* *libc.a:lib_a-findfp.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-impure.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-isblank.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-islower.* *libc.a:lib_a-isprint.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isupper.* *libc.a:lib_a-itoa.* *libc.a:lib_a-labs.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-memchr.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-memmove.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-memset.* *libc.a:lib_a-mktime.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-open.* *libc.a:lib_a-quorem.* *libc.a:lib_a-raise.* *libc.a:lib_a-rand.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-read.* *libc.a:lib_a-refill.* *libc.a:lib_a-rshift.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-sccl.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-srand.* *libc.a:lib_a-stdio.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-strcspn.* *libc.a:lib_a-strdup.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strftime.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-strlen.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-strncat.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-strptime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-strspn.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strtol.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strupr.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-sysread.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-system.* *libc.a:lib_a-systimes.* *libc.a:lib_a-syswrite.* *libc.a:lib_a-time.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-toascii.* *libc.a:lib_a-tolower.* *libc.a:lib_a-toupper.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-tzset.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-utoa.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-wsetup.* *libc.a:lock.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *libesp_hw_support.a:cpu_util.* *libesp_hw_support.a:rtc_clk.* *libesp_hw_support.a:rtc_init.* *libesp_hw_support.a:rtc_pm.* *libesp_hw_support.a:rtc_sleep.* *libesp_hw_support.a:rtc_time.* *libesp_hw_support.a:rtc_wdt.* *libesp_system.a:esp_err.* *libesp_system.a:esp_system.* *libesp_system.a:ubsan.* *libgcc.a:lib2funcs.* *libhal.a:cpu_hal.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libheap.a:heap_tlsf.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *liblog.a:log.* *liblog.a:log_freertos.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.* *libspi_flash.a:spi_flash_rom_patch.* *libxtensa.a:eri.* *libxtensa.a:xtensa_intr_asm.*) .literal.* EXCLUDE_FILE(*libesp_ringbuf.a *libfreertos.a *libgcov.a *librtc.a *libxt_hal.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libc.a:creat.* *libc.a:isatty.* *libc.a:lib_a-abs.* *libc.a:lib_a-asctime.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-atoi.* *libc.a:lib_a-atol.* *libc.a:lib_a-bzero.* *libc.a:lib_a-close.* *libc.a:lib_a-creat.* *libc.a:lib_a-ctime.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-div.* *libc.a:lib_a-environ.* *libc.a:lib_a-envlock.* *libc.a:lib_a-fclose.* *libc.a:lib_a-fflush.* *libc.a:lib_a-findfp.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-impure.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-isblank.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-islower.* *libc.a:lib_a-isprint.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isupper.* *libc.a:lib_a-itoa.* *libc.a:lib_a-labs.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-memchr.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-memmove.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-memset.* *libc.a:lib_a-mktime.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-open.* *libc.a:lib_a-quorem.* *libc.a:lib_a-raise.* *libc.a:lib_a-rand.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-read.* *libc.a:lib_a-refill.* *libc.a:lib_a-rshift.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-sccl.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-srand.* *libc.a:lib_a-stdio.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-strcspn.* *libc.a:lib_a-strdup.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strftime.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-strlen.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-strncat.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-strptime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-strspn.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strtol.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strupr.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-sysread.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-system.* *libc.a:lib_a-systimes.* *libc.a:lib_a-syswrite.* *libc.a:lib_a-time.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-toascii.* *libc.a:lib_a-tolower.* *libc.a:lib_a-toupper.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-tzset.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-utoa.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-wsetup.* *libc.a:lock.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *libesp_hw_support.a:cpu_util.* *libesp_hw_support.a:rtc_clk.* *libesp_hw_support.a:rtc_init.* *libesp_hw_support.a:rtc_pm.* *libesp_hw_support.a:rtc_sleep.* *libesp_hw_support.a:rtc_time.* *libesp_hw_support.a:rtc_wdt.* *libesp_system.a:esp_err.* *libesp_system.a:esp_system.* *libesp_system.a:ubsan.* *libgcc.a:lib2funcs.* *libhal.a:cpu_hal.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libheap.a:heap_tlsf.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *liblog.a:log.* *liblog.a:log_freertos.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.* *libspi_flash.a:spi_flash_rom_patch.* *libxtensa.a:eri.* *libxtensa.a:xtensa_intr_asm.*) .text EXCLUDE_FILE(*libesp_ringbuf.a *libfreertos.a *libgcov.a *librtc.a *libxt_hal.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libc.a:creat.* *libc.a:isatty.* *libc.a:lib_a-abs.* *libc.a:lib_a-asctime.* *libc.a:lib_a-asctime_r.* *libc.a:lib_a-atoi.* *libc.a:lib_a-atol.* *libc.a:lib_a-bzero.* *libc.a:lib_a-close.* *libc.a:lib_a-creat.* *libc.a:lib_a-ctime.* *libc.a:lib_a-ctime_r.* *libc.a:lib_a-ctype_.* *libc.a:lib_a-div.* *libc.a:lib_a-environ.* *libc.a:lib_a-envlock.* *libc.a:lib_a-fclose.* *libc.a:lib_a-fflush.* *libc.a:lib_a-findfp.* *libc.a:lib_a-fputwc.* *libc.a:lib_a-fvwrite.* *libc.a:lib_a-fwalk.* *libc.a:lib_a-getenv_r.* *libc.a:lib_a-gettzinfo.* *libc.a:lib_a-gmtime.* *libc.a:lib_a-gmtime_r.* *libc.a:lib_a-impure.* *libc.a:lib_a-isalnum.* *libc.a:lib_a-isalpha.* *libc.a:lib_a-isascii.* *libc.a:lib_a-isblank.* *libc.a:lib_a-iscntrl.* *libc.a:lib_a-isdigit.* *libc.a:lib_a-isgraph.* *libc.a:lib_a-islower.* *libc.a:lib_a-isprint.* *libc.a:lib_a-ispunct.* *libc.a:lib_a-isspace.* *libc.a:lib_a-isupper.* *libc.a:lib_a-itoa.* *libc.a:lib_a-labs.* *libc.a:lib_a-lcltime.* *libc.a:lib_a-lcltime_r.* *libc.a:lib_a-ldiv.* *libc.a:lib_a-longjmp.* *libc.a:lib_a-makebuf.* *libc.a:lib_a-memccpy.* *libc.a:lib_a-memchr.* *libc.a:lib_a-memcmp.* *libc.a:lib_a-memcpy.* *libc.a:lib_a-memmove.* *libc.a:lib_a-memrchr.* *libc.a:lib_a-memset.* *libc.a:lib_a-mktime.* *libc.a:lib_a-month_lengths.* *libc.a:lib_a-open.* *libc.a:lib_a-quorem.* *libc.a:lib_a-raise.* *libc.a:lib_a-rand.* *libc.a:lib_a-rand_r.* *libc.a:lib_a-read.* *libc.a:lib_a-refill.* *libc.a:lib_a-rshift.* *libc.a:lib_a-s_fpclassify.* *libc.a:lib_a-sbrk.* *libc.a:lib_a-sccl.* *libc.a:lib_a-setjmp.* *libc.a:lib_a-sf_nan.* *libc.a:lib_a-srand.* *libc.a:lib_a-stdio.* *libc.a:lib_a-strcasecmp.* *libc.a:lib_a-strcasestr.* *libc.a:lib_a-strcat.* *libc.a:lib_a-strchr.* *libc.a:lib_a-strcmp.* *libc.a:lib_a-strcoll.* *libc.a:lib_a-strcpy.* *libc.a:lib_a-strcspn.* *libc.a:lib_a-strdup.* *libc.a:lib_a-strdup_r.* *libc.a:lib_a-strftime.* *libc.a:lib_a-strlcat.* *libc.a:lib_a-strlcpy.* *libc.a:lib_a-strlen.* *libc.a:lib_a-strlwr.* *libc.a:lib_a-strncasecmp.* *libc.a:lib_a-strncat.* *libc.a:lib_a-strncmp.* *libc.a:lib_a-strncpy.* *libc.a:lib_a-strndup.* *libc.a:lib_a-strndup_r.* *libc.a:lib_a-strnlen.* *libc.a:lib_a-strptime.* *libc.a:lib_a-strrchr.* *libc.a:lib_a-strsep.* *libc.a:lib_a-strspn.* *libc.a:lib_a-strstr.* *libc.a:lib_a-strtok_r.* *libc.a:lib_a-strtol.* *libc.a:lib_a-strtoul.* *libc.a:lib_a-strupr.* *libc.a:lib_a-sysclose.* *libc.a:lib_a-sysopen.* *libc.a:lib_a-sysread.* *libc.a:lib_a-syssbrk.* *libc.a:lib_a-system.* *libc.a:lib_a-systimes.* *libc.a:lib_a-syswrite.* *libc.a:lib_a-time.* *libc.a:lib_a-timelocal.* *libc.a:lib_a-toascii.* *libc.a:lib_a-tolower.* *libc.a:lib_a-toupper.* *libc.a:lib_a-tzcalc_limits.* *libc.a:lib_a-tzlock.* *libc.a:lib_a-tzset.* *libc.a:lib_a-tzset_r.* *libc.a:lib_a-tzvars.* *libc.a:lib_a-ungetc.* *libc.a:lib_a-utoa.* *libc.a:lib_a-wbuf.* *libc.a:lib_a-wcrtomb.* *libc.a:lib_a-wctomb_r.* *libc.a:lib_a-wsetup.* *libc.a:lock.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *libesp_hw_support.a:cpu_util.* *libesp_hw_support.a:rtc_clk.* *libesp_hw_support.a:rtc_init.* *libesp_hw_support.a:rtc_pm.* *libesp_hw_support.a:rtc_sleep.* *libesp_hw_support.a:rtc_time.* *libesp_hw_support.a:rtc_wdt.* *libesp_system.a:esp_err.* *libesp_system.a:esp_system.* *libesp_system.a:ubsan.* *libgcc.a:lib2funcs.* *libhal.a:cpu_hal.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:soc_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libheap.a:heap_tlsf.* *libheap.a:multi_heap.* *libheap.a:multi_heap_poisoning.* *liblog.a:log.* *liblog.a:log_freertos.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.* *libspi_flash.a:spi_flash_rom_patch.* *libxtensa.a:eri.* *libxtensa.a:xtensa_intr_asm.*) .text.*)
    *(.wifi0iram .wifi0iram.*)
    *(.wifiorslpiram .wifiorslpiram.*)
    *(.wifirxiram .wifirxiram.*)
    *(.wifislpiram .wifislpiram.*)
    *(.wifislprxiram .wifislprxiram.*)
    *libesp_event.a:default_event_loop.*(.literal.esp_event_handler_instance_register .literal.esp_event_handler_instance_unregister .literal.esp_event_handler_register .literal.esp_event_handler_unregister .literal.esp_event_loop_create_default .literal.esp_event_loop_delete_default .literal.esp_event_post .literal.esp_event_send_to_default_loop .text .text.esp_event_handler_instance_register .text.esp_event_handler_instance_unregister .text.esp_event_handler_register .text.esp_event_handler_unregister .text.esp_event_loop_create_default .text.esp_event_loop_delete_default .text.esp_event_post .text.esp_event_send_to_default_loop)
    *libesp_event.a:esp_event.*(.literal.base_node_add_handler .literal.esp_event_handler_instance_register_with .literal.esp_event_handler_instance_unregister_with .literal.esp_event_handler_register_with .literal.esp_event_handler_register_with_internal .literal.esp_event_handler_unregister_with .literal.esp_event_handler_unregister_with_internal .literal.esp_event_loop_create .literal.esp_event_loop_delete .literal.esp_event_loop_run .literal.esp_event_loop_run_task .literal.esp_event_post_to .literal.handler_instances_add .literal.handler_instances_remove .literal.handler_instances_remove_all$isra$1 .literal.loop_node_add_handler .text .text.base_node_add_handler .text.esp_event_dump .text.esp_event_handler_instance_register_with .text.esp_event_handler_instance_unregister_with .text.esp_event_handler_register_with .text.esp_event_handler_register_with_internal .text.esp_event_handler_unregister_with .text.esp_event_handler_unregister_with_internal .text.esp_event_loop_create .text.esp_event_loop_delete .text.esp_event_loop_run .text.esp_event_loop_run_task .text.esp_event_post_to .text.handler_instances_add .text.handler_instances_remove .text.handler_instances_remove_all$isra$1 .text.loop_node_add_handler)
    *libesp_hw_support.a:rtc_init.*(.literal.rtc_init .literal.rtc_vddsdio_get_config .text .text.rtc_init .text.rtc_vddsdio_get_config)
    *libesp_system.a:esp_system.*(.literal.esp_get_free_heap_size .literal.esp_get_free_internal_heap_size .literal.esp_get_idf_version .literal.esp_get_minimum_free_heap_size .literal.esp_register_shutdown_handler .literal.esp_unregister_shutdown_handler .text .text.esp_get_free_heap_size .text.esp_get_free_internal_heap_size .text.esp_get_idf_version .text.esp_get_minimum_free_heap_size .text.esp_register_shutdown_handler .text.esp_unregister_shutdown_handler)
    *libfreertos.a:port.*(.literal.esp_startup_start_app .text.esp_startup_start_app)
    *libfreertos.a:port.*(.literal.esp_startup_start_app_other_cores .text.esp_startup_start_app_other_cores)
    *libfreertos.a:port_common.*(.literal.main_task .text.main_task)
    *liblog.a:log.*(.literal.esp_log_level_get .literal.esp_log_level_set .literal.esp_log_set_vprintf .literal.esp_log_writev .literal.heap_bubble_down .literal.s_log_level_get_and_unlock .text .text.esp_log_level_get .text.esp_log_level_set .text.esp_log_set_vprintf .text.esp_log_writev .text.heap_bubble_down .text.s_log_level_get_and_unlock)
    *liblog.a:log_freertos.*(.literal.esp_log_system_timestamp .text .text.esp_log_system_timestamp)

    *(.stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.irom0.text) /* catch stray ICACHE_RODATA_ATTR */
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)

    /** CPU will try to prefetch up to 16 bytes of
      * of instructions. This means that any configuration (e.g. MMU, PMS) must allow
      * safe access to up to 16 bytes after the last real instruction, add
      * dummy bytes to ensure this
      */
    . += _esp_flash_mmap_prefetch_pad_size;

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

    *(.iram.data .iram.data.*)
    _coredump_iram_start = ABSOLUTE(.);
    *(.iram2.coredump .iram2.coredump.*)
    _coredump_iram_end = ABSOLUTE(.);

    _iram_data_end = ABSOLUTE(.);
  } > iram0_0_seg

  .iram0.bss (NOLOAD) :
  {
    . = ALIGN(4);
    _iram_bss_start = ABSOLUTE(.);

    *(.iram.bss .iram.bss.*)

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
