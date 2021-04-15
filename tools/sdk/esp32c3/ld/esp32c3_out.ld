/**
 *                    ESP32-C3 Linker Script Memory Layout
 * This file describes the memory layout (memory blocks) by virtual memory addresses.
 * This linker script is passed through the C preprocessor to include configuration options.
 * Please use preprocessor features sparingly!
 * Restrict to simple macros with numeric values, and/or #if/#endif blocks.
 */
/*
 * Automatically generated file. DO NOT EDIT.
 * Espressif IoT Development Framework (ESP-IDF) Configuration Header
 */
       
/* List of deprecated options */
MEMORY
{
  /**
   *  All these values assume the flash cache is on, and have the blocks this uses subtracted from the length
   *  of the various regions. The 'data access port' dram/drom regions map to the same iram/irom regions but
   *  are connected to the data port of the CPU and eg allow byte-wise access.
   */
  /* IRAM for PRO CPU. */
  iram0_0_seg (RX) : org = (0x4037C000 + 0x4000), len = 0x403D0000 - (0x4037C000 - 0x3FC7C000) - (0x3FC7C000 + 0x4000)
  /* Flash mapped instruction data */
  iram0_2_seg (RX) : org = 0x42000020, len = 0x8000000-0x20
  /**
   * (0x20 offset above is a convenience for the app binary image generation.
   * Flash cache has 64KB pages. The .bin file which is flashed to the chip
   * has a 0x18 byte file header, and each segment has a 0x08 byte segment
   * header. Setting this offset makes it simple to meet the flash cache MMU's
   * constraint that (paddr % 64KB == vaddr % 64KB).)
   */
  /**
   * Shared data RAM, excluding memory reserved for ROM bss/data/stack.
   * Enabling Bluetooth & Trace Memory features in menuconfig will decrease the amount of RAM available.
   */
  dram0_0_seg (RW) : org = (0x3FC7C000 + 0x4000), len = 0x403D0000 - (0x4037C000 - 0x3FC7C000) - (0x3FC7C000 + 0x4000)
  /* Flash mapped constant data */
  drom0_0_seg (R) : org = 0x3C000020, len = 0x8000000-0x20
  /* (See iram0_2_seg for meaning of 0x20 offset in the above.) */
  /**
   * RTC fast memory (executable). Persists over deep sleep.
   */
  rtc_iram_seg(RWX) : org = 0x50000000, len = 0x2000
}
_static_data_end = _bss_end;
/* Heap ends at top of dram0_0_seg */
_heap_end = 0x40000000;
_data_seg_org = ORIGIN(rtc_data_seg);
/**
 *  The lines below define location alias for .rtc.data section
 *  As C3 only has RTC fast memory, this is not configurable like on other targets
 */
REGION_ALIAS("rtc_data_seg", rtc_iram_seg );
REGION_ALIAS("rtc_slow_seg", rtc_iram_seg );
REGION_ALIAS("rtc_data_location", rtc_iram_seg );
  REGION_ALIAS("default_code_seg", iram0_2_seg);
  REGION_ALIAS("default_rodata_seg", drom0_0_seg);
