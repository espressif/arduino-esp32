/*
* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: ESPRESSIF MIT
*/

#pragma once

#include "sdkconfig.h"
#if (CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE || CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE)
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#include "Arduino.h"
#include <memory>
#include <string>
#include "linux/videodev2.h"
#include "esp_video_init.h"
#include "esp_video_device.h"

typedef enum {
  ESP_VIDEO_FORMAT_UNKNOWN = 0,
  ESP_VIDEO_FORMAT_RAW8,
  ESP_VIDEO_FORMAT_RAW10,
  ESP_VIDEO_FORMAT_RAW12,
  ESP_VIDEO_FORMAT_RGB565,
  ESP_VIDEO_FORMAT_RGB888,
  ESP_VIDEO_FORMAT_YUV420,
  ESP_VIDEO_FORMAT_YUV422_YUYV,
  ESP_VIDEO_FORMAT_YUV422_UYVY,
  ESP_VIDEO_FORMAT_GRAY8,
  ESP_VIDEO_FORMAT_JPEG,
  ESP_VIDEO_FORMAT_MAX
} esp_video_format_t;

class ESPVideoSolutionClass {
public:
  ESPVideoSolutionClass() = default;
  ESPVideoSolutionClass(const ESPVideoSolutionClass &config) = default;
  ~ESPVideoSolutionClass() = default;
  ESPVideoSolutionClass &operator=(const ESPVideoSolutionClass &config);

  void begin(uint32_t width, uint32_t height);

  uint32_t getWidth() const;
  uint32_t getHeight() const;

private:
  uint32_t width_ = 0;
  uint32_t height_ = 0;
};

class ESPVideoFormatClass {
public:
  ESPVideoFormatClass() = default;
  ESPVideoFormatClass(const ESPVideoFormatClass &config) = default;
  ~ESPVideoFormatClass() = default;
  ESPVideoFormatClass &operator=(const ESPVideoFormatClass &config);

  void begin(esp_video_format_t format_type, const std::string &format_name = "");

  esp_video_format_t getFormat() const;
  const char *getFormatName() const;

private:
  esp_video_format_t format_type_ = ESP_VIDEO_FORMAT_UNKNOWN;
  mutable std::string format_name_;
};

class ESPVideoCamConfigClass {
public:
  ESPVideoCamConfigClass() = default;
  ESPVideoCamConfigClass(const ESPVideoCamConfigClass &config);

  ESPVideoCamConfigClass &operator=(const ESPVideoCamConfigClass &config);

  virtual ~ESPVideoCamConfigClass() = default;

  bool begin(i2c_port_num_t port, int8_t scl_pin, int8_t sda_pin, uint32_t i2c_freq = i2c_freq_hz, int8_t reset_pin = -1, int8_t pwdn_pin = -1);
  bool begin(i2c_master_bus_handle_t i2c_handle, uint32_t i2c_freq = i2c_freq_hz, int8_t reset_pin = -1, int8_t pwdn_pin = -1);

  esp_video_init_sccb_config_t getSccbConfig() const;

  gpio_num_t getResetPin() const {
    return reset_pin;
  }
  gpio_num_t getPwdnPin() const {
    return pwdn_pin;
  }

  bool acquirePins(void *bus);
  bool releasePins();

protected:
  virtual bool setPinsBus(void *bus);
  virtual bool clearPinsBus();

private:
  bool needInitI2C() const;

  i2c_port_num_t port = static_cast<i2c_port_num_t>(-1);
  gpio_num_t scl_pin = GPIO_NUM_NC;
  gpio_num_t sda_pin = GPIO_NUM_NC;

  uint32_t i2c_freq = i2c_freq_hz;

  gpio_num_t reset_pin = GPIO_NUM_NC;
  gpio_num_t pwdn_pin = GPIO_NUM_NC;

  i2c_master_bus_handle_t i2c_handle = nullptr;
  static const uint32_t i2c_freq_hz;
};

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
class ESPVideoCSIConfigClass : public ESPVideoCamConfigClass {
public:
  ESPVideoCSIConfigClass() = default;
  ESPVideoCSIConfigClass(const ESPVideoCSIConfigClass &config);

  bool begin(const ESPVideoCamConfigClass &config, bool dont_init_ldo = false);

  bool getDontInitLdo() const {
    return dont_init_ldo;
  }

  esp_video_init_csi_config_t getCsiInitConfig() const;

private:
  bool dont_init_ldo = false;
};
#endif  // CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE

#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
class ESPVideoDVPPinsConfigClass {
public:
  ESPVideoDVPPinsConfigClass() = default;
  ESPVideoDVPPinsConfigClass(const ESPVideoDVPPinsConfigClass &config);

  ESPVideoDVPPinsConfigClass &operator=(const ESPVideoDVPPinsConfigClass &config);

  bool begin(
    int8_t vsync_pin, int8_t de_pin, int8_t pclk_pin, int8_t xclk_pin, int8_t data0_pin, int8_t data1_pin, int8_t data2_pin, int8_t data3_pin, int8_t data4_pin,
    int8_t data5_pin, int8_t data6_pin, int8_t data7_pin
  );

  const esp_cam_ctlr_dvp_pin_config_t &getDvpPin() const {
    return dvp_pin;
  }

protected:
  bool setPinsBus(void *bus);
  bool clearPinsBus();

private:
  esp_cam_ctlr_dvp_pin_config_t dvp_pin = {};
};
#endif  // CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE

#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
class ESPVideoDVPConfigClass : public ESPVideoCamConfigClass, public ESPVideoDVPPinsConfigClass {
public:
  ESPVideoDVPConfigClass() = default;
  ESPVideoDVPConfigClass(const ESPVideoDVPConfigClass &config);

  ESPVideoDVPConfigClass &operator=(const ESPVideoDVPConfigClass &config);

  bool begin(const ESPVideoCamConfigClass &config, const ESPVideoDVPPinsConfigClass &dvp_pin, uint32_t xclk_freq);

  uint32_t getXclkFreq() const {
    return xclk_freq;
  }

  esp_video_init_dvp_config_t getDvpInitConfig() const;

  bool acquirePins(void *bus);
  bool releasePins();

private:
  bool setPinsBus(void *bus) override;
  bool clearPinsBus() override;

  uint32_t xclk_freq = 0;
};
#endif  // CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE

class ESPVideoBufferClass : public ESPVideoSolutionClass, public ESPVideoFormatClass {
public:
  ESPVideoBufferClass() = default;

  ESPVideoBufferClass(ESPVideoBufferClass &&other) noexcept;

  ESPVideoBufferClass(const ESPVideoBufferClass &) = delete;
  ESPVideoBufferClass &operator=(const ESPVideoBufferClass &) = delete;

  ESPVideoBufferClass &operator=(ESPVideoBufferClass &&other) noexcept;

  ~ESPVideoBufferClass();

  void end();

  bool valid() const;
  uint8_t *data() const;
  size_t size() const;
  int index() const;
  const char *formatName() const;
  esp_video_format_t formatType() const;

private:
  friend class ESPVideoCaptureDevClass;

  ESPVideoBufferClass(
    int fd, uint8_t *ptr, size_t n, int index, uint32_t type, uint32_t memory, const ESPVideoSolutionClass &solution, const ESPVideoFormatClass &format
  );

  void reclaim();

  int buf_fd = -1;
  uint8_t *buf_ptr = nullptr;
  size_t buf_size = 0;
  int buf_index = -1;
  uint32_t buf_type = 0;
  uint32_t memory_type = 0;
};

class ESPVideoCaptureDevClass : public ESPVideoSolutionClass, public ESPVideoFormatClass {
public:
  ESPVideoCaptureDevClass() = default;

  ESPVideoCaptureDevClass(const ESPVideoCaptureDevClass &) = delete;
  ESPVideoCaptureDevClass &operator=(const ESPVideoCaptureDevClass &) = delete;
  ESPVideoCaptureDevClass(ESPVideoCaptureDevClass &&) = delete;
  ESPVideoCaptureDevClass &operator=(ESPVideoCaptureDevClass &&) = delete;

  ~ESPVideoCaptureDevClass();

  bool begin(const char *path, size_t buf_count = 2);
  void end();

  bool requestBuffer(size_t buf_count);
  bool startCapture();
  bool stopCapture();
  ESPVideoBufferClass captureBuffer();

  bool isOpened() const;
  bool isCaptureStarted() const;

  bool setFormat(esp_video_format_t format);

  bool setSensorGain(int32_t gain);
  bool setSensorExposure(int32_t exposure);
  bool setSensorExposureTime(int32_t exposure_time);
  bool setSensorAETargetLevel(int32_t target_level);
  bool setSensorJPEGQuality(int32_t quality);
  bool setSensorVFlip(bool vflip);
  bool setSensorHFlip(bool hflip);
  bool setSensorTestPattern(bool test_pattern);

private:
  bool open(const char *path);
  bool applyFormat();
  void releaseBuffers();
  bool setExtCtrlValue(uint32_t id, int32_t value);

  int fd = -1;
  bool capture_started_ = false;
  std::vector<uint8_t *> buf_ptr_;
  std::vector<size_t> buf_len_;
  const int buffer_type_ = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int memory_type_ = V4L2_MEMORY_MMAP;
  uint32_t buf_count_ = 0;
};

class ESPVideoClass {
public:
  ESPVideoClass() = default;

  ~ESPVideoClass();

  ESPVideoClass(const ESPVideoClass &) = delete;
  ESPVideoClass &operator=(const ESPVideoClass &) = delete;

  void end();

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
  bool begin(const ESPVideoCSIConfigClass &config);
  bool isCSIInitialized() const;
#endif  // CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE

#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
  bool begin(const ESPVideoDVPConfigClass &config);
  bool isDVPInitialized() const;
#endif  // CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE

  bool isActive() const;

private:
  uint32_t flags_ = 0;
#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
  std::unique_ptr<ESPVideoCSIConfigClass> csi_config_;
#endif  // CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
  std::unique_ptr<ESPVideoDVPConfigClass> dvp_config_;
#endif  // CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
};

#endif  // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#endif  // CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE || CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
