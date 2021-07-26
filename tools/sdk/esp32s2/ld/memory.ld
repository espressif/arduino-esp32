/* ESP32S2 Linker Script Memory Layout

   This file describes the memory layout (memory blocks) by virtual memory addresses.

   This linker script is passed through the C preprocessor to include configuration options.

   Please use preprocessor features sparingly!
   Restrict to simple macros with numeric values, and/or #if/#endif blocks.
*/
/*
 * Automatically generated file. DO NOT EDIT.
 * Espressif IoT Development Framework (ESP-IDF) Configuration Header
 */
       
/* List of deprecated options */
MEMORY
{
  /* All these values assume the flash cache is on, and have the blocks this uses subtracted from the length
  of the various regions. The 'data access port' dram/drom regions map to the same iram/irom regions but
  are connected to the data port of the CPU and eg allow bytewise access. */
  /* IRAM for CPU.*/
  iram0_0_seg (RX) : org = (0x40020000 + 0x2000 + 0x2000), len = 0x3FFE0000 - (0x3FFB0000 + 0x2000 + 0x2000)
  /* Even though the segment name is iram, it is actually mapped to flash
  */
  iram0_2_seg (RX) : org = 0x40080020, len = 0x780000-0x20
  /*
    (0x20 offset above is a convenience for the app binary image generation.
    Flash cache has 64KB pages. The .bin file which is flashed to the chip
    has a 0x18 byte file header, and each segment has a 0x08 byte segment
    header. Setting this offset makes it simple to meet the flash cache MMU's
    constraint that (paddr % 64KB == vaddr % 64KB).)
  */
  /* Shared data RAM, excluding memory reserved for bootloader and ROM bss/data/stack. */
  dram0_0_seg (RW) : org = (0x3FFB0000 + 0x2000 + 0x2000), len = 0x3FFE0000 - (0x3FFB0000 + 0x2000 + 0x2000) - 0
  /* Flash mapped constant data */
  drom0_0_seg (R) : org = 0x3F000020, len = 0x3f0000-0x20
  /* (See iram0_2_seg for meaning of 0x20 offset in the above.) */
  /* RTC fast memory (executable). Persists over deep sleep.
   */
  rtc_iram_seg(RWX) : org = 0x40070000, len = 0x2000
  /* RTC slow memory (data accessible). Persists over deep sleep.

     Start of RTC slow memory is reserved for ULP co-processor code + data, if enabled.
  */
  rtc_slow_seg(RW) : org = 0x50000000 + 0,
                                     len = 0x2000 - 0
  /* RTC fast memory (same block as above), viewed from data bus */
  rtc_data_seg(RW) : org = 0x3ff9e000, len = 0x2000 - (0x10)
}
_static_data_end = _bss_end;
_heap_end = 0x40000000;
_data_seg_org = ORIGIN(rtc_data_seg);
/* The lines below define location alias for .rtc.data section based on Kconfig option.
   When the option is not defined then use slow memory segment
   else the data will be placed in fast memory segment
   TODO: check whether the rtc_data_location is correct for esp32s2 - IDF-761 */
REGION_ALIAS("rtc_data_location", rtc_slow_seg );
  REGION_ALIAS("default_code_seg", iram0_2_seg);
  REGION_ALIAS("default_rodata_seg", drom0_0_seg);
/**
 *  If rodata default segment is placed in `drom0_0_seg`, then flash's first rodata section must
 *  also be first in the segment.
 */
  ASSERT(_rodata_reserved_start == ORIGIN(default_rodata_seg),
         ".flash.appdesc section must be placed at the beginning of the rodata segment.")
