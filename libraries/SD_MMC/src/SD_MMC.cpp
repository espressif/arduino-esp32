// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Disable the automatic pin remapping of the API calls in this file
#define ARDUINO_CORE_BUILD

#include "pins_arduino.h"
#include "io_pin_remap.h"
#include "SD_MMC.h"
#ifdef SOC_SDMMC_HOST_SUPPORTED
#include "vfs_api.h"

#include <dirent.h>
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "diskio_sdmmc.h"
#include "diskio.h"
#include "soc/sdmmc_pins.h"
#include "ff.h"
#include "esp32-hal-periman.h"
#include "esp32-hal-ldo.h"

#if SOC_SDMMC_IO_POWER_EXTERNAL || SOC_SDMMC_IO_UHS_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif
#ifdef SOC_SDMMC_IO_UHS_POWER_EXTERNAL
#include "hal/sdmmc_ll.h"
#endif

using namespace fs;

#if SOC_SDMMC_IO_POWER_EXTERNAL || SOC_SDMMC_IO_UHS_POWER_EXTERNAL
static void sdmmcReleaseLdo(int8_t channel, void *handle) {
  if (channel == -1 || handle == nullptr) {
    return;
  }
#ifdef SOC_SDMMC_IO_UHS_POWER_EXTERNAL
  // CMD11 leaves the SD pins on the 1.8 V LDO. The next mount starts at 3.3 V.
  sdmmc_ll_switch_io_power_control_src(SDMMC_LL_IO_POWER_CONTROL_SRC_INTERNAL);
#endif
  sd_pwr_ctrl_del_on_chip_ldo((sd_pwr_ctrl_handle_t)handle);
  ldoSdmmcDriverDetached((uint8_t)channel);
}
#endif

SDMMCFS::SDMMCFS(FSImplPtr impl) : FS(impl), _card(nullptr) {
#if defined(SOC_SDMMC_USE_GPIO_MATRIX) && defined(BOARD_HAS_SDMMC) && !defined(CONFIG_IDF_TARGET_ESP32P4) && !defined(CONFIG_IDF_TARGET_ESP32S31)
  _pin_clk = SDMMC_CLK;
  _pin_cmd = SDMMC_CMD;
  _pin_d0 = SDMMC_D0;
#ifndef BOARD_HAS_1BIT_SDMMC
  _pin_d1 = SDMMC_D1;
  _pin_d2 = SDMMC_D2;
  _pin_d3 = SDMMC_D3;
#endif  // BOARD_HAS_1BIT_SDMMC

#elif defined(SOC_SDMMC_USE_IOMUX) && defined(CONFIG_IDF_TARGET_ESP32)
  _pin_clk = SDMMC_SLOT1_IOMUX_PIN_NUM_CLK;
  _pin_cmd = SDMMC_SLOT1_IOMUX_PIN_NUM_CMD;
  _pin_d0 = SDMMC_SLOT1_IOMUX_PIN_NUM_D0;
#ifndef BOARD_HAS_1BIT_SDMMC
  _pin_d1 = SDMMC_SLOT1_IOMUX_PIN_NUM_D1;
  _pin_d2 = SDMMC_SLOT1_IOMUX_PIN_NUM_D2;
  _pin_d3 = SDMMC_SLOT1_IOMUX_PIN_NUM_D3;
#endif  // BOARD_HAS_1BIT_SDMMC

// ESP32-P4 can use either IOMUX or GPIO matrix
#elif defined(BOARD_HAS_SDMMC) && (defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32S31))
#if defined(BOARD_SDMMC_SLOT) && (BOARD_SDMMC_SLOT == 0)
  _pin_clk = SDMMC_SLOT0_IOMUX_PIN_NUM_CLK;
  _pin_cmd = SDMMC_SLOT0_IOMUX_PIN_NUM_CMD;
  _pin_d0 = SDMMC_SLOT0_IOMUX_PIN_NUM_D0;
#ifndef BOARD_HAS_1BIT_SDMMC
  _pin_d1 = SDMMC_SLOT0_IOMUX_PIN_NUM_D1;
  _pin_d2 = SDMMC_SLOT0_IOMUX_PIN_NUM_D2;
  _pin_d3 = SDMMC_SLOT0_IOMUX_PIN_NUM_D3;
#endif  // BOARD_HAS_1BIT_SDMMC
#else
  _pin_clk = SDMMC_CLK;
  _pin_cmd = SDMMC_CMD;
  _pin_d0 = SDMMC_D0;
#ifndef BOARD_HAS_1BIT_SDMMC
  _pin_d1 = SDMMC_D1;
  _pin_d2 = SDMMC_D2;
  _pin_d3 = SDMMC_D3;
#endif  // BOARD_HAS_1BIT_SDMMC
#endif  // BOARD_SDMMC_SLOT_NO
#endif
#if defined(SOC_SDMMC_IO_POWER_EXTERNAL) && defined(BOARD_SDMMC_POWER_CHANNEL)
  _power_channel = BOARD_SDMMC_POWER_CHANNEL;
#elif defined(SOC_SDMMC_IO_UHS_POWER_EXTERNAL)
  // One LDO unit, VO1. PSRAM already holds it at 1.8 V. The SD driver acquires
  // that same fixed voltage, so the two owners share the channel.
#ifdef BOARD_SDMMC_POWER_CHANNEL
  _power_channel = BOARD_SDMMC_POWER_CHANNEL;
#else
  _power_channel = 1;
#endif
#endif
}

bool SDMMCFS::sdmmcDetachBus(void *bus_pointer) {
  SDMMCFS *bus = (SDMMCFS *)bus_pointer;
  if (bus->_card) {
    bus->end();
  }
  return true;
}

bool SDMMCFS::setPins(int clk, int cmd, int d0) {
  return setPins(clk, cmd, d0, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC);
}

bool SDMMCFS::setPins(int clk, int cmd, int d0, int d1, int d2, int d3) {
  if (_card != nullptr) {
    log_e("SD_MMC.setPins must be called before SD_MMC.begin");
    return false;
  }

  // map logical pins to GPIO numbers
  clk = digitalPinToGPIONumber(clk);
  cmd = digitalPinToGPIONumber(cmd);
  d0 = digitalPinToGPIONumber(d0);
  d1 = digitalPinToGPIONumber(d1);
  d2 = digitalPinToGPIONumber(d2);
  d3 = digitalPinToGPIONumber(d3);

#if defined(SOC_SDMMC_USE_GPIO_MATRIX) && !defined(CONFIG_IDF_TARGET_ESP32P4) && !defined(CONFIG_IDF_TARGET_ESP32S31)
  // SoC supports SDMMC pin configuration via GPIO matrix. Save the pins for later use in SDMMCFS::begin.
  _pin_clk = (int8_t)clk;
  _pin_cmd = (int8_t)cmd;
  _pin_d0 = (int8_t)d0;
  _pin_d1 = (int8_t)d1;
  _pin_d2 = (int8_t)d2;
  _pin_d3 = (int8_t)d3;
  return true;
#elif CONFIG_IDF_TARGET_ESP32
  // ESP32 doesn't support SDMMC pin configuration via GPIO matrix.
  // Since SDMMCFS::begin hardcodes the usage of slot 1, only check if
  // the pins match slot 1 pins.
  bool pins_ok =
    (clk == (int)SDMMC_SLOT1_IOMUX_PIN_NUM_CLK) && (cmd == (int)SDMMC_SLOT1_IOMUX_PIN_NUM_CMD) && (d0 == (int)SDMMC_SLOT1_IOMUX_PIN_NUM_D0)
    && (((d1 == -1) && (d2 == -1) && (d3 == -1)) || ((d1 == (int)SDMMC_SLOT1_IOMUX_PIN_NUM_D1) && (d2 == (int)SDMMC_SLOT1_IOMUX_PIN_NUM_D2) && (d3 == (int)SDMMC_SLOT1_IOMUX_PIN_NUM_D3)));
  if (!pins_ok) {
    log_e("SDMMCFS: specified pins are not supported by this chip.");
    return false;
  }
  return true;
#elif defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32S31)
#if defined(BOARD_SDMMC_SLOT) && (BOARD_SDMMC_SLOT == 0)
  // ESP32-P4 can use either IOMUX or GPIO matrix
  bool pins_ok =
    (clk == (int)SDMMC_SLOT0_IOMUX_PIN_NUM_CLK) && (cmd == (int)SDMMC_SLOT0_IOMUX_PIN_NUM_CMD) && (d0 == (int)SDMMC_SLOT0_IOMUX_PIN_NUM_D0)
    && (((d1 == -1) && (d2 == -1) && (d3 == -1)) || ((d1 == (int)SDMMC_SLOT0_IOMUX_PIN_NUM_D1) && (d2 == (int)SDMMC_SLOT0_IOMUX_PIN_NUM_D2) && (d3 == (int)SDMMC_SLOT0_IOMUX_PIN_NUM_D3)));
  if (!pins_ok) {
    log_e("SDMMCFS: specified pins are not supported when using IOMUX (SDMMC SLOT 0).");
    return false;
  }
  return true;
#else
  _pin_clk = (int8_t)clk;
  _pin_cmd = (int8_t)cmd;
  _pin_d0 = (int8_t)d0;
  _pin_d1 = (int8_t)d1;
  _pin_d2 = (int8_t)d2;
  _pin_d3 = (int8_t)d3;
  return true;
#endif
#else
#error SoC not supported
#endif
}

#if SOC_SDMMC_IO_POWER_EXTERNAL || SOC_SDMMC_IO_UHS_POWER_EXTERNAL
bool SDMMCFS::setPowerChannel(int power_channel) {
  if (_card != nullptr) {
    log_e("SD_MMC.setPowerChannel must be called before SD_MMC.begin");
    return false;
  }
  _power_channel = power_channel;
  return true;
}
#endif

bool SDMMCFS::begin(const char *mountpoint, bool mode1bit, bool format_if_mount_failed, int sdmmc_frequency, uint8_t maxOpenFiles) {
  if (_card) {
    return true;
  }
  perimanSetBusDeinit(ESP32_BUS_TYPE_SDMMC_CLK, SDMMCFS::sdmmcDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_SDMMC_CMD, SDMMCFS::sdmmcDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_SDMMC_D0, SDMMCFS::sdmmcDetachBus);
  if (!mode1bit) {
    perimanSetBusDeinit(ESP32_BUS_TYPE_SDMMC_D1, SDMMCFS::sdmmcDetachBus);
    perimanSetBusDeinit(ESP32_BUS_TYPE_SDMMC_D2, SDMMCFS::sdmmcDetachBus);
    perimanSetBusDeinit(ESP32_BUS_TYPE_SDMMC_D3, SDMMCFS::sdmmcDetachBus);
  }
  //mount
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
#if (defined(SOC_SDMMC_USE_GPIO_MATRIX) && !defined(CONFIG_IDF_TARGET_ESP32P4) && !defined(CONFIG_IDF_TARGET_ESP32S31)) \
  || ((defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32S31)) && ((defined(BOARD_SDMMC_SLOT) && (BOARD_SDMMC_SLOT == 1)) || !defined(BOARD_HAS_SDMMC)))
  log_d("pin_cmd: %d, pin_clk: %d, pin_d0: %d, pin_d1: %d, pin_d2: %d, pin_d3: %d", _pin_cmd, _pin_clk, _pin_d0, _pin_d1, _pin_d2, _pin_d3);
  // SoC supports SDMMC pin configuration via GPIO matrix.
  // Check that the pins have been set either in the constructor or setPins function.
  if (_pin_cmd == -1 || _pin_clk == -1 || _pin_d0 == -1 || (!mode1bit && (_pin_d1 == -1 || _pin_d2 == -1 || _pin_d3 == -1))) {
    log_e("SDMMCFS: some SD pins are not set");
    return false;
  }

  slot_config.clk = (gpio_num_t)_pin_clk;
  slot_config.cmd = (gpio_num_t)_pin_cmd;
  slot_config.d0 = (gpio_num_t)_pin_d0;
  slot_config.d1 = (gpio_num_t)_pin_d1;
  slot_config.d2 = (gpio_num_t)_pin_d2;
  slot_config.d3 = (gpio_num_t)_pin_d3;
  slot_config.width = 4;
#endif  // SOC_SDMMC_USE_GPIO_MATRIX

  if (!perimanClearPinBus(_pin_cmd)) {
    return false;
  }
  if (!perimanClearPinBus(_pin_clk)) {
    return false;
  }
  if (!perimanClearPinBus(_pin_d0)) {
    return false;
  }
  if (!mode1bit) {
    if (!perimanClearPinBus(_pin_d1)) {
      return false;
    }
    if (!perimanClearPinBus(_pin_d2)) {
      return false;
    }
    if (!perimanClearPinBus(_pin_d3)) {
      return false;
    }
  }

#if (defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32S31)) && defined(BOARD_SDMMC_SLOT) && (BOARD_SDMMC_SLOT == 0)
  // reconfigure slot_config to remove all pins in order to use IO_MUX
  // Use 0 instead of GPIO_NUM_NC (-1) because ESP-IDF's s_check_pin_not_set()
  // function uses !pin which doesn't work correctly with -1 (GPIO_NUM_NC)
  slot_config = sdmmc_slot_config_t{
    .clk = GPIO_NUM_0,
    .cmd = GPIO_NUM_0,
    .d0 = GPIO_NUM_0,
    .d1 = GPIO_NUM_0,
    .d2 = GPIO_NUM_0,
    .d3 = GPIO_NUM_0,
    .d4 = GPIO_NUM_0,
    .d5 = GPIO_NUM_0,
    .d6 = GPIO_NUM_0,
    .d7 = GPIO_NUM_0,
    .cd = SDMMC_SLOT_NO_CD,
    .wp = SDMMC_SLOT_NO_WP,
    .width = 4,
    .flags = 0,
  };
#endif  // slot 0 IOMUX

#ifdef SOC_SDMMC_UHS_I_SUPPORTED
  // One retry at 40 MHz when UHS-I is requested but the card never switches to 1.8 V.
  // IDF still programs host.max_freq_khz in that case (sdmmc_enable_hs_mode_and_check).
  bool uhs_fallback = false;
mount_again:
  slot_config.flags = 0;
#endif
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  // Keep DEINIT_ARG. SDMMC_HOST_DEFAULT stores sdmmc_host_deinit_slot in deinit_p.
  // Clearing the flag makes VFS call that function with no slot, so the slot
  // handle is null, slot 0 stays reserved, and a UHS fallback cannot mount.
  host.flags = SDMMC_HOST_FLAG_4BIT | SDMMC_HOST_FLAG_DEINIT_ARG;
#if (defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32S31)) && defined(BOARD_SDMMC_SLOT) && (BOARD_SDMMC_SLOT == 0)
  host.slot = SDMMC_HOST_SLOT_0;
#else
  host.slot = SDMMC_HOST_SLOT_1;
#endif
#ifdef BOARD_HAS_1BIT_SDMMC
  mode1bit = true;
#endif
  if (mode1bit) {
    host.flags = SDMMC_HOST_FLAG_1BIT | SDMMC_HOST_FLAG_DEINIT_ARG;
    slot_config.width = 1;
  }
  _mode1bit = mode1bit;

#ifdef SOC_SDMMC_UHS_I_SUPPORTED
  // UHS-I is 1.8 V and 4-line, slot 0 only.
  const bool uhs_slot = !mode1bit && host.slot == SDMMC_HOST_SLOT_0;
  if (!uhs_slot && sdmmc_frequency > SDMMC_FREQ_HIGHSPEED) {
    log_w("UHS-I needs 4-line slot 0, using %d kHz", SDMMC_FREQ_HIGHSPEED);
    sdmmc_frequency = SDMMC_FREQ_HIGHSPEED;
  } else if (sdmmc_frequency == SDMMC_FREQ_DDR50) {
    host.flags |= SDMMC_HOST_FLAG_DDR;
    slot_config.flags |= SDMMC_SLOT_FLAG_UHS1;
  } else if (sdmmc_frequency == SDMMC_FREQ_SDR50) {
    slot_config.flags |= SDMMC_SLOT_FLAG_UHS1;
  } else if (sdmmc_frequency > SDMMC_FREQ_HIGHSPEED) {
    // Any other value above high speed is not a UHS-I mode.
    // IDF would still program that clock at 3.3 V, so clamp before the mount.
    log_w("SDMMC frequency %d kHz is not a supported UHS-I mode, using %d kHz", sdmmc_frequency, SDMMC_FREQ_HIGHSPEED);
    sdmmc_frequency = SDMMC_FREQ_HIGHSPEED;
  }
#endif
#ifdef SOC_SDMMC_IO_UHS_POWER_EXTERNAL
  // Same as the IDF sdmmc example. IOMUX setup does not enable pull-ups.
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
#endif
  host.max_freq_khz = sdmmc_frequency;

#if SOC_SDMMC_IO_POWER_EXTERNAL || SOC_SDMMC_IO_UHS_POWER_EXTERNAL
  bool acquire_ldo = (_power_channel != -1);
#if SOC_SDMMC_IO_UHS_POWER_EXTERNAL
  // Fixed 1.8 V rail. A 3.3 V mount must not acquire it: card_init would try to
  // adjust the rail and the non-adjustable channel would reject that.
  acquire_ldo = acquire_ldo && ((slot_config.flags & SDMMC_SLOT_FLAG_UHS1) != 0);
#endif
  if (!acquire_ldo) {
#if SOC_SDMMC_IO_POWER_EXTERNAL
    if (_power_channel == -1) {
      log_i("On-chip power channel specified, use external power for SDMMC");
    }
#endif
  } else {
    sd_pwr_ctrl_ldo_config_t ldo_config = {
      .ldo_chan_id = _power_channel,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    ldoSdmmcPrepareAcquire((uint8_t)_power_channel);
    if (sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle) != ESP_OK) {
      log_e("Failed to create a new on-chip LDO power control driver");
      ldoSdmmcDriverCreateFailed((uint8_t)_power_channel);
#if SOC_SDMMC_IO_UHS_POWER_EXTERNAL && SOC_SDMMC_UHS_I_SUPPORTED
      if (!uhs_fallback) {
        log_w("SDMMC UHS-I needs the 1.8 V LDO, using %d kHz", SDMMC_FREQ_HIGHSPEED);
        sdmmc_frequency = SDMMC_FREQ_HIGHSPEED;
        uhs_fallback = true;
        goto mount_again;
      }
#endif
      return false;
    }
    _pwr_ctrl_handle = pwr_ctrl_handle;
    ldoSdmmcDriverAttached((uint8_t)_power_channel);
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#if SOC_SDMMC_IO_UHS_POWER_EXTERNAL
    // Fixed 1800 mV rail. card_init does (int)(io_voltage * 1000) before CMD0.
    // 1.8f rounds to 1800, which this LDO accepts. CMD11 then moves the pins onto it.
    host.io_voltage = 1.8f;
    log_i("SDMMC UHS-I using 1.8 V LDO channel %d", _power_channel);
#endif
  }
#endif

#if defined(BOARD_SDMMC_POWER_PIN)
#ifndef BOARD_SDMMC_POWER_ON_LEVEL
#error "BOARD_SDMMC_POWER_ON_LEVEL not defined, please define it in pins_arduino.h"
#endif
  pinMode(BOARD_SDMMC_POWER_PIN, OUTPUT);
  digitalWrite(BOARD_SDMMC_POWER_PIN, !BOARD_SDMMC_POWER_ON_LEVEL);
  delay(200);
  digitalWrite(BOARD_SDMMC_POWER_PIN, BOARD_SDMMC_POWER_ON_LEVEL);
  perimanSetPinBusExtraType(BOARD_SDMMC_POWER_PIN, "SDMMC_POWER");
#endif

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = format_if_mount_failed,
    .max_files = maxOpenFiles,
    .allocation_unit_size = 0,
    .disk_status_check_enable = false,
    .use_one_fat = false
  };

  esp_err_t ret = esp_vfs_fat_sdmmc_mount(mountpoint, &host, &slot_config, &mount_config, &_card);
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      log_e("Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
    } else if (ret == ESP_ERR_INVALID_STATE && _card != nullptr) {
      _impl->mountpoint(mountpoint);
      log_w("SD Already mounted");
      return true;
    } else {
      log_e("Failed to initialize the card (0x%x). Make sure SD card lines have pull-up resistors in place.", ret);
    }
#if SOC_SDMMC_IO_POWER_EXTERNAL || SOC_SDMMC_IO_UHS_POWER_EXTERNAL
    if (_power_channel != -1 && _pwr_ctrl_handle != nullptr) {
      sdmmcReleaseLdo(_power_channel, _pwr_ctrl_handle);
      host.pwr_ctrl_handle = NULL;
      _pwr_ctrl_handle = nullptr;
    }
#endif
#ifdef SOC_SDMMC_UHS_I_SUPPORTED
    if (!uhs_fallback && ret != ESP_FAIL && sdmmc_frequency > SDMMC_FREQ_HIGHSPEED) {
      log_w("SDMMC init failed (0x%x) at %d kHz, retrying at %d kHz", ret, sdmmc_frequency, SDMMC_FREQ_HIGHSPEED);
      _card = NULL;
      sdmmc_frequency = SDMMC_FREQ_HIGHSPEED;
      uhs_fallback = true;
      goto mount_again;
    }
#endif
    _card = NULL;
    return false;
  }
#ifdef SOC_SDMMC_UHS_I_SUPPORTED
  // Mount can succeed with is_uhs1 clear while the host clock is still SDR50.
  // That is 100 MHz at 3.3 V. Drop it and mount at high speed.
  if (!uhs_fallback && !_card->is_uhs1 && _card->real_freq_khz > SDMMC_FREQ_HIGHSPEED) {
    log_w("SDMMC clock %d kHz without UHS-I, retrying at %d kHz", _card->real_freq_khz, SDMMC_FREQ_HIGHSPEED);
    _impl->mountpoint(mountpoint);
    sdmmc_frequency = SDMMC_FREQ_HIGHSPEED;
    uhs_fallback = true;
    end();
    goto mount_again;
  }
#endif
  _impl->mountpoint(mountpoint);
  _pdrv = ff_diskio_get_pdrv_card(_card);

  if (!perimanSetPinBus(_pin_cmd, ESP32_BUS_TYPE_SDMMC_CMD, (void *)(this), -1, -1)) {
    goto err;
  }
  if (!perimanSetPinBus(_pin_clk, ESP32_BUS_TYPE_SDMMC_CLK, (void *)(this), -1, -1)) {
    goto err;
  }
  if (!perimanSetPinBus(_pin_d0, ESP32_BUS_TYPE_SDMMC_D0, (void *)(this), -1, -1)) {
    goto err;
  }
  if (!mode1bit) {
    if (!perimanSetPinBus(_pin_d1, ESP32_BUS_TYPE_SDMMC_D1, (void *)(this), -1, -1)) {
      goto err;
    }
    if (!perimanSetPinBus(_pin_d2, ESP32_BUS_TYPE_SDMMC_D2, (void *)(this), -1, -1)) {
      goto err;
    }
    if (!perimanSetPinBus(_pin_d3, ESP32_BUS_TYPE_SDMMC_D3, (void *)(this), -1, -1)) {
      goto err;
    }
  }
  return true;

err:
  log_e("Failed to set all pins bus to SDMMC");
  SDMMCFS::sdmmcDetachBus((void *)(this));
  return false;
}

void SDMMCFS::end() {
  if (_card) {
#if SOC_SDMMC_IO_POWER_EXTERNAL || SOC_SDMMC_IO_UHS_POWER_EXTERNAL
    void *pwr_ctrl = _pwr_ctrl_handle;
    if (pwr_ctrl == nullptr && _power_channel != -1) {
      pwr_ctrl = _card->host.pwr_ctrl_handle;
    }
#endif
    esp_vfs_fat_sdcard_unmount(_impl->mountpoint(), _card);
    _impl->mountpoint(NULL);
    _card = NULL;
    perimanClearPinBus(_pin_cmd);
    perimanClearPinBus(_pin_clk);
    perimanClearPinBus(_pin_d0);
    if (!_mode1bit) {
      perimanClearPinBus(_pin_d1);
      perimanClearPinBus(_pin_d2);
      perimanClearPinBus(_pin_d3);
    }
#if defined(BOARD_SDMMC_POWER_PIN)
    perimanClearPinBus(BOARD_SDMMC_POWER_PIN);
#endif
#if SOC_SDMMC_IO_POWER_EXTERNAL || SOC_SDMMC_IO_UHS_POWER_EXTERNAL
    if (_power_channel != -1 && pwr_ctrl != nullptr) {
      sdmmcReleaseLdo(_power_channel, pwr_ctrl);
      _pwr_ctrl_handle = nullptr;
    }
#endif
  }
}

sdcard_type_t SDMMCFS::cardType() {
  if (!_card) {
    return CARD_NONE;
  }
  return (_card->ocr & SD_OCR_SDHC_CAP) ? CARD_SDHC : CARD_SD;
}

uint64_t SDMMCFS::cardSize() {
  if (!_card) {
    return 0;
  }
  return (uint64_t)_card->csd.capacity * _card->csd.sector_size;
}

uint64_t SDMMCFS::totalBytes() {
  FATFS *fsinfo;
  DWORD fre_clust;
  if (f_getfree("0:", &fre_clust, &fsinfo) != 0) {
    return 0;
  }
  uint64_t size = ((uint64_t)(fsinfo->csize)) * (fsinfo->n_fatent - 2)
#if _MAX_SS != 512
                  * (fsinfo->ssize);
#else
                  * 512;
#endif
  return size;
}

uint64_t SDMMCFS::usedBytes() {
  FATFS *fsinfo;
  DWORD fre_clust;
  if (f_getfree("0:", &fre_clust, &fsinfo) != 0) {
    return 0;
  }
  uint64_t size = ((uint64_t)(fsinfo->csize)) * ((fsinfo->n_fatent - 2) - (fsinfo->free_clst))
#if _MAX_SS != 512
                  * (fsinfo->ssize);
#else
                  * 512;
#endif
  return size;
}

int SDMMCFS::sectorSize() {
  if (!_card) {
    return 0;
  }
  return _card->csd.sector_size;
}

int SDMMCFS::numSectors() {
  if (!_card) {
    return 0;
  }
  return (totalBytes() / _card->csd.sector_size);
}

int SDMMCFS::cardClockKHz() {
  if (!_card) {
    return 0;
  }
  return _card->real_freq_khz;
}

bool SDMMCFS::cardIsDDR() {
  return _card && _card->is_ddr;
}

bool SDMMCFS::cardIsUHS1() {
  return _card && _card->is_uhs1;
}

bool SDMMCFS::readRAW(uint8_t *buffer, uint32_t sector) {
  return (disk_read(_pdrv, buffer, sector, 1) == 0);
}

bool SDMMCFS::writeRAW(uint8_t *buffer, uint32_t sector) {
  return (disk_write(_pdrv, buffer, sector, 1) == 0);
}

SDMMCFS SD_MMC = SDMMCFS(FSImplPtr(new VFSImpl()));
#endif /* SOC_SDMMC_HOST_SUPPORTED */
