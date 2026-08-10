/**
 * MIPI-CSI ESP_Video capture example.
 *
 * This sketch demonstrates the minimal flow for capturing camera frames over a
 * MIPI-CSI interface with the ESP_Video library: initialize the camera sensor
 * via SCCB, open a V4L2-style capture device, start streaming, and dequeue
 * frames in a loop.
 *
 * Requirements:
 * - ESP-IDF >= 5.4.0
 * - CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE=y
 *
 * Supported target and default board wiring:
 * - ESP32-P4: MIPI-CSI camera on ESP32-P4-Function-EV-Board V1.5
 *   (SCCB I2C port 0, SCL GPIO 8, SDA GPIO 7).
 *
 * How it works:
 * 1. setup() configures SCCB (I2C) with ESPVideoCamConfigClass, wraps it in
 *    ESPVideoCSIConfigClass, and calls ESPVideoClass::begin().
 * 2. capture_dev.begin() opens ESP_VIDEO_MIPI_CSI_DEVICE_NAME, requests two
 *    mmap capture buffers, and startCapture() starts streaming.
 * 3. loop() calls ESPVideoCaptureDevClass::captureBuffer() to dequeue the next
 *    frame. On success it prints the buffer pointer and size to Serial; the
 *    buffer is returned to the driver when the ESPVideoBufferClass object is
 *    destroyed at the end of each iteration.
 *
 * Open Serial Monitor at 115200 baud to see capture status and frame metadata.
 * Adapt the EXAMPLE_MIPI_CSI_* pin defines if your camera is wired differently.
 */
#include "Arduino.h"
#include <ESP_Video.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#if CONFIG_IDF_TARGET_ESP32P4
/**
 * @brief This configuration is for the ESP32-P4-Function-EV-Board V1.5
 */
#define EXAMPLE_MIPI_CSI_SCCB_I2C_PORT    0
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN 8
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN 7
#else
#error "The selected target SoC is not supported"
#endif

ESPVideoClass video;
ESPVideoCaptureDevClass capture_dev;
/**
 * @brief Number of capture buffers, buffer > 2 for double buffering, this can avoid frame dropping
 */
const size_t kCaptureBufferCount = 2;

void setup() {
  Serial.begin(115200);

  ESPVideoCamConfigClass cam_config;
  cam_config.begin(EXAMPLE_MIPI_CSI_SCCB_I2C_PORT, EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN, EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN);

  ESPVideoCSIConfigClass csi_config;
  csi_config.begin(cam_config);

  if (!video.begin(csi_config)) {
    Serial.println("failed to init CSI camera");
    return;
  }

  if (!capture_dev.begin(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, kCaptureBufferCount)) {
    Serial.println("failed to open capture device");
    return;
  }

  if (!capture_dev.setFormat(ESP_VIDEO_FORMAT_RGB565)) {
    Serial.println("failed to set format");
    return;
  }

  if (!capture_dev.startCapture()) {
    Serial.println("failed to start capture");
    return;
  }

  Serial.println("Arduino camera example started");
}

void loop() {
  if (!capture_dev.isOpened() || !capture_dev.isCaptureStarted()) {
    delay(1000);
    return;
  }

  ESPVideoBufferClass buffer = capture_dev.captureBuffer();
  if (!buffer.valid()) {
    Serial.println("failed to capture buffer");
    delay(10);
    return;
  }

  Serial.printf(
    "captured buffer: %p size: %lu format: %s width: %lu height: %lu\n", buffer.data(), (unsigned long)buffer.size(), buffer.formatName(),
    (unsigned long)buffer.getWidth(), (unsigned long)buffer.getHeight()
  );
}
#else
void setup() {
  Serial.begin(115200);
  Serial.println("ESP_IDF_VERSION < 5.4.0, this example is not supported");
}

void loop() {
  delay(1000);
}
#endif  // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
