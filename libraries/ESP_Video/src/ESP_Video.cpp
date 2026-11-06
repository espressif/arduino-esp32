/*
* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: ESPRESSIF MIT
*/

#include "sdkconfig.h"
#if (CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE || CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE)
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#include "ESP_Video.h"
#include "esp32-hal-periman.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>

#include "esp_video_ioctl.h"

const uint32_t ESPVideoCamConfigClass::i2c_freq_hz = 100 * 1000;

static bool video_format_to_v4l2_format(esp_video_format_t format, uint32_t &v4l2_format, std::string &format_name) {
  switch (format) {
    case ESP_VIDEO_FORMAT_RAW8:
      v4l2_format = V4L2_PIX_FMT_SRGGB8;
      format_name = "RAW8";
      break;
    case ESP_VIDEO_FORMAT_RAW10:
      v4l2_format = V4L2_PIX_FMT_SGRBG10;
      format_name = "RAW10";
      break;
    case ESP_VIDEO_FORMAT_RAW12:
      v4l2_format = V4L2_PIX_FMT_SGRBG12;
      format_name = "RAW12";
      break;
    case ESP_VIDEO_FORMAT_RGB565:
      v4l2_format = V4L2_PIX_FMT_RGB565;
      format_name = "RGB565";
      break;
    case ESP_VIDEO_FORMAT_RGB888:
      v4l2_format = V4L2_PIX_FMT_RGB24;
      format_name = "RGB888";
      break;
    case ESP_VIDEO_FORMAT_YUV420:
      v4l2_format = V4L2_PIX_FMT_YUV420;
      format_name = "YUV420";
      break;
    case ESP_VIDEO_FORMAT_YUV422_YUYV:
      v4l2_format = V4L2_PIX_FMT_YUYV;
      format_name = "YUV422_YUYV";
      break;
    case ESP_VIDEO_FORMAT_YUV422_UYVY:
      v4l2_format = V4L2_PIX_FMT_UYVY;
      format_name = "YUV422_UYVY";
      break;
    case ESP_VIDEO_FORMAT_GRAY8:
      v4l2_format = V4L2_PIX_FMT_GREY;
      format_name = "GRAY8";
      break;
    case ESP_VIDEO_FORMAT_JPEG:
      v4l2_format = V4L2_PIX_FMT_JPEG;
      format_name = "JPEG";
      break;
    default: return false;
  }

  return true;
}

static bool v4l2_format_to_video_format(uint32_t v4l2_format, esp_video_format_t &format_type, std::string &format_name) {
  switch (v4l2_format) {
    case V4L2_PIX_FMT_SRGGB8:
      format_type = ESP_VIDEO_FORMAT_RAW8;
      format_name = "RAW8";
      break;
    case V4L2_PIX_FMT_SGRBG10:
      format_type = ESP_VIDEO_FORMAT_RAW10;
      format_name = "RAW10";
      break;
    case V4L2_PIX_FMT_SGRBG12:
      format_type = ESP_VIDEO_FORMAT_RAW12;
      format_name = "RAW12";
      break;
    case V4L2_PIX_FMT_RGB565:
      format_type = ESP_VIDEO_FORMAT_RGB565;
      format_name = "RGB565";
      break;
    case V4L2_PIX_FMT_RGB24:
      format_type = ESP_VIDEO_FORMAT_RGB888;
      format_name = "RGB888";
      break;
    case V4L2_PIX_FMT_YUV420:
      format_type = ESP_VIDEO_FORMAT_YUV420;
      format_name = "YUV420";
      break;
    case V4L2_PIX_FMT_YUYV:
      format_type = ESP_VIDEO_FORMAT_YUV422_YUYV;
      format_name = "YUV422_YUYV";
      break;
    case V4L2_PIX_FMT_UYVY:
      format_type = ESP_VIDEO_FORMAT_YUV422_UYVY;
      format_name = "YUV422_UYVY";
      break;
    case V4L2_PIX_FMT_GREY:
      format_type = ESP_VIDEO_FORMAT_GRAY8;
      format_name = "GRAY8";
      break;
    case V4L2_PIX_FMT_JPEG:
      format_type = ESP_VIDEO_FORMAT_JPEG;
      format_name = "JPEG";
      break;
    default: return false;
  }

  return true;
}

static bool videoDetachBus(void *bus_pointer) {
  ESPVideoClass *video = static_cast<ESPVideoClass *>(bus_pointer);
  if (video == nullptr || video->isActive()) {
    log_e("ESP-Video: call video.end() before reusing camera pins");
    return false;
  }
  return true;
}

ESPVideoSolutionClass &ESPVideoSolutionClass::operator=(const ESPVideoSolutionClass &config) {
  if (this != &config) {
    width_ = config.width_;
    height_ = config.height_;
  }
  return *this;
}

void ESPVideoSolutionClass::begin(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
}

uint32_t ESPVideoSolutionClass::getWidth() const {
  return width_;
}

uint32_t ESPVideoSolutionClass::getHeight() const {
  return height_;
}

ESPVideoFormatClass &ESPVideoFormatClass::operator=(const ESPVideoFormatClass &config) {
  if (this != &config) {
    format_type_ = config.format_type_;
    format_name_ = config.format_name_;
  }
  return *this;
}

void ESPVideoFormatClass::begin(esp_video_format_t format_type, const std::string &format_name) {
  format_type_ = format_type;
  format_name_ = format_name;
}

esp_video_format_t ESPVideoFormatClass::getFormat() const {
  return format_type_;
}

const char *ESPVideoFormatClass::getFormatName() const {
  if (format_name_.empty()) {
    uint32_t buf_format = 0;

    if (!video_format_to_v4l2_format(format_type_, buf_format, format_name_)) {
      format_name_ = "UNKNOWN";
    }
  }

  return format_name_.c_str();
}

ESPVideoCamConfigClass::ESPVideoCamConfigClass(const ESPVideoCamConfigClass &config)
  : port(config.port), scl_pin(config.scl_pin), sda_pin(config.sda_pin), i2c_freq(config.i2c_freq), reset_pin(config.reset_pin), pwdn_pin(config.pwdn_pin),
    i2c_handle(config.i2c_handle) {}

ESPVideoCamConfigClass &ESPVideoCamConfigClass::operator=(const ESPVideoCamConfigClass &config) {
  if (this != &config) {
    port = config.port;
    scl_pin = config.scl_pin;
    sda_pin = config.sda_pin;
    i2c_freq = config.i2c_freq;
    reset_pin = config.reset_pin;
    pwdn_pin = config.pwdn_pin;
    i2c_handle = config.i2c_handle;
  }
  return *this;
}

bool ESPVideoCamConfigClass::begin(i2c_port_num_t port, int8_t scl_pin, int8_t sda_pin, uint32_t i2c_freq, int8_t reset_pin, int8_t pwdn_pin) {
  this->port = port;
  this->scl_pin = static_cast<gpio_num_t>(scl_pin);
  this->sda_pin = static_cast<gpio_num_t>(sda_pin);
  this->i2c_freq = i2c_freq;
  this->reset_pin = static_cast<gpio_num_t>(reset_pin);
  this->pwdn_pin = static_cast<gpio_num_t>(pwdn_pin);
  this->i2c_handle = nullptr;
  return true;
}

bool ESPVideoCamConfigClass::begin(i2c_master_bus_handle_t i2c_handle, uint32_t i2c_freq, int8_t reset_pin, int8_t pwdn_pin) {
  this->port = static_cast<i2c_port_num_t>(-1);
  this->scl_pin = GPIO_NUM_NC;
  this->sda_pin = GPIO_NUM_NC;
  this->i2c_freq = i2c_freq;
  this->reset_pin = static_cast<gpio_num_t>(reset_pin);
  this->pwdn_pin = static_cast<gpio_num_t>(pwdn_pin);
  this->i2c_handle = i2c_handle;
  return true;
}

bool ESPVideoCamConfigClass::needInitI2C() const {
  return i2c_handle == nullptr;
}

bool ESPVideoCamConfigClass::acquirePins(void *bus) {
  return setPinsBus(bus);
}

bool ESPVideoCamConfigClass::releasePins() {
  return clearPinsBus();
}

bool ESPVideoCamConfigClass::setPinsBus(void *bus) {
  if (!clearPinsBus()) {
    log_e("failed to clear pins bus");
    return false;
  }

  if (reset_pin != GPIO_NUM_NC) {
    if (!perimanSetPinBus(reset_pin, ESP32_BUS_TYPE_VIDEO_CAM_RESET, bus, -1, -1)) {
      log_e("failed to set reset pin");
      goto cleanup;
    }
    perimanSetBusDeinit(ESP32_BUS_TYPE_VIDEO_CAM_RESET, videoDetachBus);
  }

  if (pwdn_pin != GPIO_NUM_NC) {
    if (!perimanSetPinBus(pwdn_pin, ESP32_BUS_TYPE_VIDEO_CAM_PWDN, bus, -1, -1)) {
      log_e("failed to set pwdn pin");
      goto cleanup;
    }
    perimanSetBusDeinit(ESP32_BUS_TYPE_VIDEO_CAM_PWDN, videoDetachBus);
  }

  if (needInitI2C()) {
    if (!perimanSetPinBus(scl_pin, ESP32_BUS_TYPE_VIDEO_SCCB_SCL, bus, port, -1)) {
      log_e("failed to set scl pin");
      goto cleanup;
    }
    perimanSetBusDeinit(ESP32_BUS_TYPE_VIDEO_SCCB_SCL, videoDetachBus);

    if (!perimanSetPinBus(sda_pin, ESP32_BUS_TYPE_VIDEO_SCCB_SDA, bus, port, -1)) {
      log_e("failed to set sda pin");
      goto cleanup;
    }
    perimanSetBusDeinit(ESP32_BUS_TYPE_VIDEO_SCCB_SDA, videoDetachBus);
  }

  return true;

cleanup:
  clearPinsBus();
  return false;
}

bool ESPVideoCamConfigClass::clearPinsBus() {
  if (reset_pin != GPIO_NUM_NC) {
    if (!perimanClearPinBus(reset_pin)) {
      log_e("failed to clear reset pin");
      return false;
    }
  }

  if (pwdn_pin != GPIO_NUM_NC) {
    if (!perimanClearPinBus(pwdn_pin)) {
      log_e("failed to clear pwdn pin");
      return false;
    }
  }

  if (needInitI2C()) {
    if (!perimanClearPinBus(scl_pin)) {
      log_e("failed to clear scl pin");
      return false;
    }
    if (!perimanClearPinBus(sda_pin)) {
      log_e("failed to clear sda pin");
      return false;
    }
  }

  return true;
}

esp_video_init_sccb_config_t ESPVideoCamConfigClass::getSccbConfig() const {
  esp_video_init_sccb_config_t config = {};

  if (needInitI2C()) {
    config.init_sccb = true;
    config.i2c_config.port = static_cast<uint8_t>(port);
    config.i2c_config.scl_pin = scl_pin;
    config.i2c_config.sda_pin = sda_pin;
  } else {
    config.init_sccb = false;
    config.i2c_handle = i2c_handle;
  }
  config.freq = i2c_freq;

  return config;
}

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
ESPVideoCSIConfigClass::ESPVideoCSIConfigClass(const ESPVideoCSIConfigClass &config) : ESPVideoCamConfigClass(config), dont_init_ldo(config.dont_init_ldo) {}

bool ESPVideoCSIConfigClass::begin(const ESPVideoCamConfigClass &config, bool dont_init_ldo) {
  ESPVideoCamConfigClass::operator=(config);
  this->dont_init_ldo = dont_init_ldo;
  return true;
}

esp_video_init_csi_config_t ESPVideoCSIConfigClass::getCsiInitConfig() const {
  esp_video_init_csi_config_t config = {
    .sccb_config = getSccbConfig(),
    .reset_pin = getResetPin(),
    .pwdn_pin = getPwdnPin(),
    .dont_init_ldo = dont_init_ldo,
  };

  return config;
}
#endif  // CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE

#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
ESPVideoDVPPinsConfigClass::ESPVideoDVPPinsConfigClass(const ESPVideoDVPPinsConfigClass &config) : dvp_pin(config.dvp_pin) {}

bool ESPVideoDVPPinsConfigClass::begin(
  int8_t vsync_pin, int8_t de_pin, int8_t pclk_pin, int8_t xclk_pin, int8_t data0_pin, int8_t data1_pin, int8_t data2_pin, int8_t data3_pin, int8_t data4_pin,
  int8_t data5_pin, int8_t data6_pin, int8_t data7_pin
) {
  dvp_pin.data_width = CAM_CTLR_DATA_WIDTH_8;

  dvp_pin.vsync_io = static_cast<gpio_num_t>(vsync_pin);
  dvp_pin.de_io = static_cast<gpio_num_t>(de_pin);
  dvp_pin.pclk_io = static_cast<gpio_num_t>(pclk_pin);
  dvp_pin.xclk_io = static_cast<gpio_num_t>(xclk_pin);

  dvp_pin.data_io[0] = static_cast<gpio_num_t>(data0_pin);
  dvp_pin.data_io[1] = static_cast<gpio_num_t>(data1_pin);
  dvp_pin.data_io[2] = static_cast<gpio_num_t>(data2_pin);
  dvp_pin.data_io[3] = static_cast<gpio_num_t>(data3_pin);
  dvp_pin.data_io[4] = static_cast<gpio_num_t>(data4_pin);
  dvp_pin.data_io[5] = static_cast<gpio_num_t>(data5_pin);
  dvp_pin.data_io[6] = static_cast<gpio_num_t>(data6_pin);
  dvp_pin.data_io[7] = static_cast<gpio_num_t>(data7_pin);

  return true;
}

ESPVideoDVPPinsConfigClass &ESPVideoDVPPinsConfigClass::operator=(const ESPVideoDVPPinsConfigClass &config) {
  if (this != &config) {
    dvp_pin = config.dvp_pin;
  }
  return *this;
}

bool ESPVideoDVPPinsConfigClass::setPinsBus(void *bus) {
  if (!clearPinsBus()) {
    log_e("failed to clear pins bus");
    return false;
  }

  if (!perimanSetPinBus(dvp_pin.xclk_io, ESP32_BUS_TYPE_LCDCAM_CAM_XCLK, bus, -1, -1)) {
    log_e("failed to set xclk pin");
    goto cleanup;
  }
  perimanSetBusDeinit(ESP32_BUS_TYPE_LCDCAM_CAM_XCLK, videoDetachBus);

  if (!perimanSetPinBus(dvp_pin.vsync_io, ESP32_BUS_TYPE_LCDCAM_CAM_VSYNC, bus, -1, -1)) {
    log_e("failed to set vsync pin");
    goto cleanup;
  }
  perimanSetBusDeinit(ESP32_BUS_TYPE_LCDCAM_CAM_VSYNC, videoDetachBus);

  if (!perimanSetPinBus(dvp_pin.de_io, ESP32_BUS_TYPE_LCDCAM_CAM_HSYNC, bus, -1, -1)) {
    log_e("failed to set de pin");
    goto cleanup;
  }
  perimanSetBusDeinit(ESP32_BUS_TYPE_LCDCAM_CAM_HSYNC, videoDetachBus);

  if (!perimanSetPinBus(dvp_pin.pclk_io, ESP32_BUS_TYPE_LCDCAM_CAM_PCLK, bus, -1, -1)) {
    log_e("failed to set pclk pin");
    goto cleanup;
  }
  perimanSetBusDeinit(ESP32_BUS_TYPE_LCDCAM_CAM_PCLK, videoDetachBus);

  for (int i = 0; i < 8; ++i) {
    if (!perimanSetPinBus(dvp_pin.data_io[i], (peripheral_bus_type_t)(ESP32_BUS_TYPE_LCDCAM_CAM_D0 + i), bus, -1, -1)) {
      log_e("failed to set data pin");
      goto cleanup;
    }
    perimanSetBusDeinit((peripheral_bus_type_t)(ESP32_BUS_TYPE_LCDCAM_CAM_D0 + i), videoDetachBus);
  }

  return true;

cleanup:
  clearPinsBus();
  return false;
}

bool ESPVideoDVPPinsConfigClass::clearPinsBus() {
  if (!perimanClearPinBus(dvp_pin.xclk_io)) {
    log_e("failed to clear xclk pin");
    return false;
  }
  if (!perimanClearPinBus(dvp_pin.vsync_io)) {
    log_e("failed to clear vsync pin");
    return false;
  }
  if (!perimanClearPinBus(dvp_pin.de_io)) {
    log_e("failed to clear de pin");
    return false;
  }
  if (!perimanClearPinBus(dvp_pin.pclk_io)) {
    log_e("failed to clear pclk pin");
    return false;
  }
  for (int i = 0; i < 8; ++i) {
    if (!perimanClearPinBus(dvp_pin.data_io[i])) {
      log_e("failed to clear data pin %d", i);
      return false;
    }
  }

  return true;
}

ESPVideoDVPConfigClass::ESPVideoDVPConfigClass(const ESPVideoDVPConfigClass &config)
  : ESPVideoCamConfigClass(config), ESPVideoDVPPinsConfigClass(config), xclk_freq(config.xclk_freq) {}

bool ESPVideoDVPConfigClass::begin(const ESPVideoCamConfigClass &config, const ESPVideoDVPPinsConfigClass &dvp_pin, uint32_t xclk_freq) {
  ESPVideoCamConfigClass::operator=(config);
  ESPVideoDVPPinsConfigClass::operator=(dvp_pin);
  this->xclk_freq = xclk_freq;
  return true;
}

ESPVideoDVPConfigClass &ESPVideoDVPConfigClass::operator=(const ESPVideoDVPConfigClass &config) {
  if (this != &config) {
    ESPVideoCamConfigClass::operator=(config);
    ESPVideoDVPPinsConfigClass::operator=(config);
    xclk_freq = config.xclk_freq;
  }
  return *this;
}

esp_video_init_dvp_config_t ESPVideoDVPConfigClass::getDvpInitConfig() const {
  esp_video_init_dvp_config_t config = {
    .sccb_config = getSccbConfig(),
    .reset_pin = getResetPin(),
    .pwdn_pin = getPwdnPin(),
    .dvp_pin = getDvpPin(),
    .xclk_freq = xclk_freq,
  };

  return config;
}

bool ESPVideoDVPConfigClass::acquirePins(void *bus) {
  return setPinsBus(bus);
}

bool ESPVideoDVPConfigClass::releasePins() {
  return clearPinsBus();
}

bool ESPVideoDVPConfigClass::setPinsBus(void *bus) {
  if (!ESPVideoCamConfigClass::setPinsBus(bus)) {
    log_e("failed to set camera pins bus");
    return false;
  }

  if (!ESPVideoDVPPinsConfigClass::setPinsBus(bus)) {
    log_e("failed to set pins bus");
    ESPVideoCamConfigClass::clearPinsBus();
    return false;
  }

  return true;
}

bool ESPVideoDVPConfigClass::clearPinsBus() {
  bool ok = ESPVideoDVPPinsConfigClass::clearPinsBus();
  if (!ESPVideoCamConfigClass::clearPinsBus()) {
    log_e("failed to clear camera pins bus");
    ok = false;
  }

  return ok;
}
#endif  // CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE

ESPVideoBufferClass::ESPVideoBufferClass(
  int fd, uint8_t *ptr, size_t n, int index, uint32_t type, uint32_t memory, const ESPVideoSolutionClass &solution, const ESPVideoFormatClass &format
)
  : ESPVideoSolutionClass(solution), ESPVideoFormatClass(format), buf_fd(fd), buf_ptr(ptr), buf_size(n), buf_index(index), buf_type(type), memory_type(memory) {
}

ESPVideoBufferClass::ESPVideoBufferClass(ESPVideoBufferClass &&other) noexcept
  : ESPVideoSolutionClass(other), ESPVideoFormatClass(other), buf_fd(other.buf_fd), buf_ptr(other.buf_ptr), buf_size(other.buf_size),
    buf_index(other.buf_index), buf_type(other.buf_type), memory_type(other.memory_type) {
  other.buf_fd = -1;
  other.buf_ptr = nullptr;
  other.buf_size = 0;
  other.buf_index = -1;
}

ESPVideoBufferClass &ESPVideoBufferClass::operator=(ESPVideoBufferClass &&other) noexcept {
  if (this != &other) {
    end();

    ESPVideoSolutionClass::operator=(other);
    ESPVideoFormatClass::operator=(other);

    buf_fd = other.buf_fd;
    buf_ptr = other.buf_ptr;
    buf_size = other.buf_size;
    buf_index = other.buf_index;
    buf_type = other.buf_type;
    memory_type = other.memory_type;

    other.buf_fd = -1;
    other.buf_ptr = nullptr;
    other.buf_size = 0;
    other.buf_index = -1;
  }
  return *this;
}

ESPVideoBufferClass::~ESPVideoBufferClass() {
  end();
}

void ESPVideoBufferClass::end() {
  reclaim();
}

bool ESPVideoBufferClass::valid() const {
  return buf_ptr != nullptr && buf_size > 0;
}

uint8_t *ESPVideoBufferClass::data() const {
  return buf_ptr;
}

size_t ESPVideoBufferClass::size() const {
  return buf_size;
}

int ESPVideoBufferClass::index() const {
  return buf_index;
}

const char *ESPVideoBufferClass::formatName() const {
  return getFormatName();
}

esp_video_format_t ESPVideoBufferClass::formatType() const {
  return getFormat();
}

void ESPVideoBufferClass::reclaim() {
  if (buf_fd >= 0 && buf_index >= 0) {
    struct v4l2_buffer buf = {};

    buf.type = buf_type;
    buf.memory = memory_type;
    buf.index = buf_index;
    if (ioctl(buf_fd, VIDIOC_QBUF, &buf) != 0) {
      log_e("failed to queue video frame");
    }

    buf_fd = -1;
    buf_index = -1;
  }
}

ESPVideoCaptureDevClass::~ESPVideoCaptureDevClass() {
  end();
}

bool ESPVideoCaptureDevClass::begin(const char *path, size_t buf_count) {
  if (isOpened()) {
    return true;
  }

  if (!open(path)) {
    return false;
  }

  if (!requestBuffer(buf_count)) {
    end();
    return false;
  }

  buf_count_ = buf_count;

  return true;
}

void ESPVideoCaptureDevClass::end() {
  if (!stopCapture()) {
    log_e("failed to stop capture");
  }
  releaseBuffers();

  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}

bool ESPVideoCaptureDevClass::open(const char *path) {
  if (fd >= 0) {
    return true;
  }

  fd = ::open(path, O_RDWR);
  if (fd < 0) {
    log_e("failed to open %s", path);
    return false;
  }

  if (!applyFormat()) {
    close(fd);
    fd = -1;
    return false;
  }

  return true;
}

bool ESPVideoCaptureDevClass::applyFormat() {
  if (fd < 0) {
    return false;
  }

  struct v4l2_format format = {};
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
    log_e("failed to get format");
    return false;
  }

  esp_video_format_t format_type = ESP_VIDEO_FORMAT_UNKNOWN;
  std::string format_name;

  if (!v4l2_format_to_video_format(format.fmt.pix.pixelformat, format_type, format_name)) {
    ESPVideoFormatClass::begin(ESP_VIDEO_FORMAT_UNKNOWN, "UNKNOWN");
  } else {
    ESPVideoFormatClass::begin(format_type, format_name);
  }

  if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
    log_e("failed to set format");
    return false;
  }

  ESPVideoSolutionClass::begin(format.fmt.pix.width, format.fmt.pix.height);

  return true;
}

void ESPVideoCaptureDevClass::releaseBuffers() {
  for (size_t i = 0; i < buf_ptr_.size(); ++i) {
    if (buf_ptr_[i] != nullptr && buf_ptr_[i] != MAP_FAILED) {
      munmap(buf_ptr_[i], buf_len_[i]);
    }
  }
  buf_ptr_.clear();
  buf_len_.clear();

  if (fd >= 0) {
    struct v4l2_requestbuffers req = {};
    req.count = 0;
    req.type = buffer_type_;
    req.memory = memory_type_;
    ioctl(fd, VIDIOC_REQBUFS, &req);
  }
}

bool ESPVideoCaptureDevClass::requestBuffer(size_t buf_count) {
  if (fd < 0) {
    return false;
  }

  if (buf_count == 0) {
    log_w("requested zero buffers");
    return false;
  }

  releaseBuffers();

  struct v4l2_requestbuffers req = {};
  memory_type_ = V4L2_MEMORY_MMAP;
  req.count = buf_count;
  req.type = buffer_type_;
  req.memory = memory_type_;
  if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
    log_e("failed to require buffer");
    return false;
  }

  buf_ptr_.reserve(buf_count);
  buf_len_.reserve(buf_count);

  for (size_t i = 0; i < buf_count; ++i) {
    struct v4l2_buffer buf = {};
    buf.type = buffer_type_;
    buf.memory = memory_type_;
    buf.index = static_cast<uint32_t>(i);
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
      log_e("failed to query buffer");
      releaseBuffers();
      return false;
    }

    uint8_t *ptr = static_cast<uint8_t *>(mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset));
    if (ptr == MAP_FAILED) {
      log_e("failed to map buffer");
      releaseBuffers();
      return false;
    }

    buf_ptr_.push_back(ptr);
    buf_len_.push_back(buf.length);
  }

  return true;
}

bool ESPVideoCaptureDevClass::startCapture() {
  if (!isOpened()) {
    return false;
  }

  if (buf_ptr_.empty()) {
    log_e("no buffer is setup");
    return false;
  }

  for (size_t i = 0; i < buf_ptr_.size(); ++i) {
    struct v4l2_buffer buf = {};
    buf.type = buffer_type_;
    buf.memory = memory_type_;
    buf.index = static_cast<uint32_t>(i);
    if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
      log_e("failed to queue video frame");
      return false;
    }
  }

  int type = buffer_type_;
  if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
    log_e("failed to start stream");
    return false;
  }

  capture_started_ = true;
  return true;
}

bool ESPVideoCaptureDevClass::stopCapture() {
  if (!capture_started_ || fd < 0) {
    return false;
  }

  int type = buffer_type_;
  if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
    log_e("failed to stop stream");
    return false;
  }
  capture_started_ = false;
  return true;
}

ESPVideoBufferClass ESPVideoCaptureDevClass::captureBuffer() {
  if (!capture_started_ || fd < 0) {
    return ESPVideoBufferClass();
  }

  struct v4l2_buffer buf = {};
  buf.type = buffer_type_;
  buf.memory = memory_type_;
  if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
    log_e("failed to receive video frame");
    return ESPVideoBufferClass();
  }

  if (buf.index >= buf_ptr_.size()) {
    log_e("invalid buffer index %u", buf.index);
    if (stopCapture()) {
      log_w("stopped capture");
    } else {
      log_e("failed to stop capture");
    }
    return ESPVideoBufferClass();
  }

  return ESPVideoBufferClass(fd, buf_ptr_[buf.index], buf.bytesused, static_cast<int>(buf.index), buffer_type_, memory_type_, *this, *this);
}

bool ESPVideoCaptureDevClass::isOpened() const {
  return fd >= 0;
}

bool ESPVideoCaptureDevClass::isCaptureStarted() const {
  return capture_started_;
}

bool ESPVideoCaptureDevClass::setFormat(esp_video_format_t format) {
  if (fd < 0) {
    return false;
  }

  if (capture_started_) {
    log_e("capture is started, cannot set format");
    return false;
  }

  uint32_t video_format = 0;
  std::string format_name;

  if (!video_format_to_v4l2_format(format, video_format, format_name)) {
    return false;
  }

  struct v4l2_format v4l2_format = {};
  v4l2_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(fd, VIDIOC_G_FMT, &v4l2_format) != 0) {
    log_e("failed to get format");
    return false;
  }

  if (v4l2_format.fmt.pix.pixelformat != video_format) {
    v4l2_format.fmt.pix.pixelformat = video_format;
    if (ioctl(fd, VIDIOC_S_FMT, &v4l2_format) != 0) {
      log_e("failed to set format");
      return false;
    }

    if (buf_count_ > 0) {
      if (!requestBuffer(buf_count_)) {
        log_e("failed to request buffer");
        releaseBuffers();
        return false;
      }
    }
  }

  ESPVideoFormatClass::begin(format, format_name);

  return true;
}

bool ESPVideoCaptureDevClass::setSensorGain(int32_t gain) {
  return setExtCtrlValue(V4L2_CID_GAIN, gain);
}

bool ESPVideoCaptureDevClass::setSensorExposure(int32_t exposure) {
  return setExtCtrlValue(V4L2_CID_EXPOSURE, exposure);
}

bool ESPVideoCaptureDevClass::setSensorExposureTime(int32_t exposure_time) {
  return setExtCtrlValue(V4L2_CID_EXPOSURE_ABSOLUTE, exposure_time);
}

bool ESPVideoCaptureDevClass::setSensorAETargetLevel(int32_t target_level) {
  return setExtCtrlValue(V4L2_CID_CAMERA_AE_LEVEL, target_level);
}

bool ESPVideoCaptureDevClass::setSensorJPEGQuality(int32_t quality) {
  return setExtCtrlValue(V4L2_CID_JPEG_COMPRESSION_QUALITY, quality);
}

bool ESPVideoCaptureDevClass::setSensorVFlip(bool vflip) {
  return setExtCtrlValue(V4L2_CID_VFLIP, vflip);
}

bool ESPVideoCaptureDevClass::setSensorHFlip(bool hflip) {
  return setExtCtrlValue(V4L2_CID_HFLIP, hflip);
}

bool ESPVideoCaptureDevClass::setSensorTestPattern(bool test_pattern) {
  return setExtCtrlValue(V4L2_CID_TEST_PATTERN, test_pattern);
}

bool ESPVideoCaptureDevClass::setExtCtrlValue(uint32_t id, int32_t value) {
  if (fd < 0) {
    return false;
  }

  struct v4l2_ext_control control = {};
  struct v4l2_ext_controls controls = {};

  controls.count = 1;
  controls.controls = &control;
  control.id = id;
  control.value = value;

  return ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) == 0;
}

ESPVideoClass::~ESPVideoClass() {
  end();
}

void ESPVideoClass::end() {
  if (flags_ != 0) {
    esp_video_deinit_with_flags(flags_);
    flags_ = 0;
  }

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
  if (csi_config_) {
    csi_config_->releasePins();
  }
  csi_config_.reset();
#endif
#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
  if (dvp_config_) {
    dvp_config_->releasePins();
  }
  dvp_config_.reset();
#endif
}

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
bool ESPVideoClass::begin(const ESPVideoCSIConfigClass &config) {
  constexpr uint32_t target_flags = ESP_VIDEO_INIT_FLAGS_MIPI_CSI | ESP_VIDEO_INIT_FLAGS_ISP;
  if ((flags_ & target_flags) == target_flags) {
    return true;
  }

  esp_video_init_csi_config_t csi_config = config.getCsiInitConfig();
  esp_video_init_config_t cam_config = {};
  cam_config.csi = &csi_config;

  csi_config_ = std::make_unique<ESPVideoCSIConfigClass>(config);
  if (!csi_config_->acquirePins(this)) {
    log_e("failed to acquire pins");
    csi_config_->releasePins();
    csi_config_.reset();
    return false;
  }

  if (esp_video_init_with_flags(&cam_config, target_flags) != ESP_OK) {
    log_e("failed to init CSI");
    csi_config_->releasePins();
    csi_config_.reset();
    return false;
  }

  flags_ |= target_flags;
  return true;
}

bool ESPVideoClass::isCSIInitialized() const {
  constexpr uint32_t target_flags = ESP_VIDEO_INIT_FLAGS_MIPI_CSI | ESP_VIDEO_INIT_FLAGS_ISP;
  return (flags_ & target_flags) == target_flags;
}
#endif  // CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE

#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
bool ESPVideoClass::begin(const ESPVideoDVPConfigClass &config) {
  constexpr uint32_t target_flags = ESP_VIDEO_INIT_FLAGS_DVP;
  if ((flags_ & target_flags) == target_flags) {
    return true;
  }

  esp_video_init_dvp_config_t dvp_config = config.getDvpInitConfig();
  esp_video_init_config_t cam_config = {};
  cam_config.dvp = &dvp_config;

  dvp_config_ = std::make_unique<ESPVideoDVPConfigClass>(config);
  if (!dvp_config_->acquirePins(this)) {
    log_e("failed to acquire pins");
    dvp_config_->releasePins();
    dvp_config_.reset();
    return false;
  }

  if (esp_video_init_with_flags(&cam_config, target_flags) != ESP_OK) {
    log_e("failed to init DVP");
    dvp_config_->releasePins();
    dvp_config_.reset();
    return false;
  }

  flags_ |= target_flags;
  return true;
}

bool ESPVideoClass::isDVPInitialized() const {
  constexpr uint32_t target_flags = ESP_VIDEO_INIT_FLAGS_DVP;
  return (flags_ & target_flags) == target_flags;
}
#endif  // CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE

bool ESPVideoClass::isActive() const {
  return (flags_ != 0);
}

#endif  // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#endif  // CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE || CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
