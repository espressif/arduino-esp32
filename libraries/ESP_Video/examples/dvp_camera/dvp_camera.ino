/**
 * DVP ESP_Video capture example.
 *
 * This sketch demonstrates the minimal flow for capturing camera frames over a
 * DVP (parallel) interface with the ESP_Video library: initialize the camera
 * sensor via SCCB, configure parallel data pins, open a V4L2-style capture
 * device, start streaming, and dequeue frames in a loop.
 *
 * Requirements:
 * - ESP-IDF >= 5.4.0
 * - CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE=y
 *
 * Supported targets and default board wiring:
 * - ESP32-S3: DVP camera on ESP32-S3-EYE (SCCB + parallel data pins, 10 MHz
 *   XCLK).
 * - ESP32-P4: DVP camera on ESP32-P4-Function-EV-Board V1.5 (SCCB + parallel
 *   data pins, 20 MHz XCLK).
 *
 * How it works:
 * 1. setup() configures SCCB (I2C) with ESPVideoCamConfigClass, parallel pins
 *    with ESPVideoDVPPinsConfigClass, and wraps both in ESPVideoDVPConfigClass
 *    before calling ESPVideoClass::begin().
 * 2. capture_dev.begin() opens ESP_VIDEO_DVP_DEVICE_NAME, requests two mmap
 *    capture buffers, and startCapture() starts streaming.
 * 3. loop() calls ESPVideoCaptureDevClass::captureBuffer() to dequeue the next
 *    frame. On success it prints the buffer pointer and size to Serial; the
 *    buffer is returned to the driver when the ESPVideoBufferClass object is
 *    destroyed at the end of each iteration.
 *
 * Open Serial Monitor at 115200 baud to see capture status and frame metadata.
 * Adapt the EXAMPLE_DVP_* pin defines if your camera is wired to a different board.
 */
#include "Arduino.h"
#include <ESP_Video.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#if CONFIG_IDF_TARGET_ESP32P4
/**
 * @brief This configuration is for the ESP32-P4-Function-EV-Board V1.5
 */
#define EXAMPLE_DVP_SCCB_I2C_PORT    0
#define EXAMPLE_DVP_SCCB_I2C_SCL_PIN 8
#define EXAMPLE_DVP_SCCB_I2C_SDA_PIN 7
#define EXAMPLE_DVP_XCLK_PIN         20
#define EXAMPLE_DVP_PCLK_PIN         4
#define EXAMPLE_DVP_VSYNC_PIN        37
#define EXAMPLE_DVP_DE_PIN           22
#define EXAMPLE_DVP_D0_PIN           2
#define EXAMPLE_DVP_D1_PIN           32
#define EXAMPLE_DVP_D2_PIN           33
#define EXAMPLE_DVP_D3_PIN           23
#define EXAMPLE_DVP_D4_PIN           3
#define EXAMPLE_DVP_D5_PIN           6
#define EXAMPLE_DVP_D6_PIN           5
#define EXAMPLE_DVP_D7_PIN           21
#define EXAMPLE_DVP_XCLK_FREQ        20000000
#elif CONFIG_IDF_TARGET_ESP32S3
/**
 * @brief This configuration is for the ESP32-S3-EYE board
 */
#define EXAMPLE_DVP_SCCB_I2C_PORT    0
#define EXAMPLE_DVP_SCCB_I2C_SCL_PIN 5
#define EXAMPLE_DVP_SCCB_I2C_SDA_PIN 4
#define EXAMPLE_DVP_XCLK_PIN         15
#define EXAMPLE_DVP_PCLK_PIN         13
#define EXAMPLE_DVP_VSYNC_PIN        6
#define EXAMPLE_DVP_DE_PIN           7
#define EXAMPLE_DVP_D0_PIN           11
#define EXAMPLE_DVP_D1_PIN           9
#define EXAMPLE_DVP_D2_PIN           8
#define EXAMPLE_DVP_D3_PIN           10
#define EXAMPLE_DVP_D4_PIN           12
#define EXAMPLE_DVP_D5_PIN           18
#define EXAMPLE_DVP_D6_PIN           17
#define EXAMPLE_DVP_D7_PIN           16
#define EXAMPLE_DVP_XCLK_FREQ        10000000
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
  cam_config.begin(EXAMPLE_DVP_SCCB_I2C_PORT, EXAMPLE_DVP_SCCB_I2C_SCL_PIN, EXAMPLE_DVP_SCCB_I2C_SDA_PIN);

  ESPVideoDVPPinsConfigClass dvp_pins;
  dvp_pins.begin(
    EXAMPLE_DVP_VSYNC_PIN, EXAMPLE_DVP_DE_PIN, EXAMPLE_DVP_PCLK_PIN, EXAMPLE_DVP_XCLK_PIN, EXAMPLE_DVP_D0_PIN, EXAMPLE_DVP_D1_PIN, EXAMPLE_DVP_D2_PIN,
    EXAMPLE_DVP_D3_PIN, EXAMPLE_DVP_D4_PIN, EXAMPLE_DVP_D5_PIN, EXAMPLE_DVP_D6_PIN, EXAMPLE_DVP_D7_PIN
  );

  ESPVideoDVPConfigClass dvp_config;
  dvp_config.begin(cam_config, dvp_pins, EXAMPLE_DVP_XCLK_FREQ);

  if (!video.begin(dvp_config)) {
    Serial.println("failed to init DVP camera");
    return;
  }

  if (!capture_dev.begin(ESP_VIDEO_DVP_DEVICE_NAME, kCaptureBufferCount)) {
    Serial.println("failed to open capture device");
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
