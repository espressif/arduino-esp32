#include <string.h>

#include <esp_system.h>
#include <esp32s3/rom/cache.h>
#include <esp_heap_caps.h>

#include "double_tap.h"

#define NUM_TOKENS 3
static const uint32_t MAGIC_TOKENS[NUM_TOKENS] = {
  0xf01681de,
  0xbd729b29,
  0xd359be7a,
};

static void *magic_area;
static uint32_t backup_area[NUM_TOKENS];

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// Current IDF does not map external RAM to a fixed address.
// The actual VMA depends on other enabled devices, so the precise
// location must be discovered.
#include <esp_psram.h>
#include <esp_private/esp_psram_extram.h>
static uintptr_t get_extram_data_high(void) {
  // get a pointer into SRAM area (only the address is useful)
  void *psram_ptr = heap_caps_malloc(16, MALLOC_CAP_SPIRAM);
  heap_caps_free(psram_ptr);

  // keep moving backwards until leaving PSRAM area
  uintptr_t psram_base_addr = (uintptr_t)psram_ptr;
  psram_base_addr &= ~(CONFIG_MMU_PAGE_SIZE - 1);  // align to start of page
  while (esp_psram_check_ptr_addr((void *)psram_base_addr)) {
    psram_base_addr -= CONFIG_MMU_PAGE_SIZE;
  }

  // offset is one page from start of PSRAM
  return psram_base_addr + CONFIG_MMU_PAGE_SIZE + esp_psram_get_size();
}
#else
#include <soc/soc.h>
#define get_extram_data_high() ((uintptr_t)SOC_EXTRAM_DATA_HIGH)
#endif

void double_tap_init(void) {
  // magic location block ends 0x20 bytes from end of PSRAM
  magic_area = (void *)(get_extram_data_high() - 0x20 - sizeof(MAGIC_TOKENS));
}

void double_tap_mark() {
  memcpy(backup_area, magic_area, sizeof(MAGIC_TOKENS));
  memcpy(magic_area, MAGIC_TOKENS, sizeof(MAGIC_TOKENS));
  Cache_WriteBack_Addr((uintptr_t)magic_area, sizeof(MAGIC_TOKENS));
}

void double_tap_invalidate() {
  if (memcmp(backup_area, MAGIC_TOKENS, sizeof(MAGIC_TOKENS))) {
    // different contents: restore backup
    memcpy(magic_area, backup_area, sizeof(MAGIC_TOKENS));
  } else {
    // clear memory
    memset(magic_area, 0, sizeof(MAGIC_TOKENS));
  }
  Cache_WriteBack_Addr((uintptr_t)magic_area, sizeof(MAGIC_TOKENS));
}

bool double_tap_check_match() {
  return (memcmp(magic_area, MAGIC_TOKENS, sizeof(MAGIC_TOKENS)) == 0);
}
