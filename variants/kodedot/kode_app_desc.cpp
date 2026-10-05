/**
 * @file kode_app_desc.cpp
 * @brief A strong esp_app_desc for the Kode Dot, naming the sketch.
 *
 * kodeOS identifies an installed app by esp_app_desc_t::project_name. The core's prebuilt
 * libraries carry a weak descriptor naming "arduino-lib-builder", so every sketch would be the
 * same app; this strong definition wins the link and names the sketch after its file (the board
 * entry passes -DKODE_SKETCH_NAME="{build.project_name}"). The fields the bootloader checks come
 * from the libraries' sdkconfig, the same way IDF's esp_app_desc.c fills them.
 */
#include <stdint.h>
#include <string_view>

#include "sdkconfig.h"
#include "esp_app_desc.h"
#include "esp_arduino_version.h"

/** @brief The sketch's file name, handed in by the board entry. */
#ifndef KODE_SKETCH_NAME
#define KODE_SKETCH_NAME "sketch"
#endif

/** @brief The IDF the prebuilt libraries were built from. */
#ifndef IDF_VER
#define IDF_VER "unknown"
#endif

/**
 * @brief A sketch file name without its .ino/.pde extension.
 * @param file The file name, for example "arduino_hello.ino".
 * @return The stem, for example "arduino_hello".
 */
static constexpr std::string_view kodeStem(std::string_view file) {
  for (std::string_view ext : {".ino", ".pde"}) {
    if (file.ends_with(ext)) {
      return file.substr(0, file.size() - ext.size());
    }
  }
  return file;
}

static constexpr std::string_view KODE_PROJECT_NAME = kodeStem(KODE_SKETCH_NAME);

/* project_name is char[32]: two sketches whose names agree in the first 31 characters would install as one app. */
static_assert(
  KODE_PROJECT_NAME.size() < sizeof(esp_app_desc_t::project_name), "Kode Dot: the sketch file name must be 31 characters or fewer "
                                                                   "(kodeOS identifies an app by a 31-character project name)"
);

/**
 * @brief Build the descriptor this image carries.
 * @return A fully populated esp_app_desc_t.
 *
 * It runs at compile time: the descriptor has to be in the .bin, where the bootloader and kodeOS
 * read it, so std::string_view (constexpr) rather than <cstring> (not). Each copy stops one short
 * of its field, whose zeros keep the terminator.
 */
static constexpr esp_app_desc_t kodeMakeAppDesc(void) {
  esp_app_desc_t d = {};

  d.magic_word = ESP_APP_DESC_MAGIC_WORD;

  KODE_PROJECT_NAME.copy(d.project_name, sizeof(d.project_name) - 1U);
  std::string_view(ESP_ARDUINO_VERSION_STR).copy(d.version, sizeof(d.version) - 1U);
  std::string_view(__TIME__).copy(d.time, sizeof(d.time) - 1U);
  std::string_view(__DATE__).copy(d.date, sizeof(d.date) - 1U);
  std::string_view(IDF_VER).copy(d.idf_ver, sizeof(d.idf_ver) - 1U);

  /* checked by the bootloader before it jumps; the MMU page size is stored as log2 */
  d.min_efuse_blk_rev_full = CONFIG_ESP_EFUSE_BLOCK_REV_MIN_FULL;
  d.max_efuse_blk_rev_full = CONFIG_ESP_EFUSE_BLOCK_REV_MAX_FULL;
  d.mmu_page_size = 31 - __builtin_clz(CONFIG_MMU_PAGE_SIZE);

  /* app_elf_sha256 stays zero: the image tool fills it in after the link. */
  return d;
}

/** @brief The descriptor itself, overriding the libraries' weak one; constinit refuses anything left for run time. */
extern "C" constinit const __attribute__((section(".rodata_desc"), used)) esp_app_desc_t esp_app_desc = kodeMakeAppDesc();
