#########
ESP Video
#########

About
-----

The ESP Video library wraps the ESP-IDF `esp_video` component and exposes a V4L2-style capture API for camera sensors on supported SoCs.
It handles low-level initialization for **MIPI-CSI** (with ISP) and **DVP** interfaces, then lets applications open a video device node, allocate buffers, and dequeue frames.

Typical use cases include preview pipelines, frame grabbing for computer vision, JPEG capture, and sensor tuning (gain, exposure, flip, and similar controls).

The library is available when **ESP-IDF 5.4.0 or later** is used and the target is **ESP32-S3** or **ESP32-P4**.
At least one of ``CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE`` or ``CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE`` must be enabled in the IDF configuration; otherwise ``ESP_Video.h`` exposes no public API.
Supported interfaces depend on those Kconfig options.
The library examples target **ESP32-P4** (MIPI-CSI + DVP) and **ESP32-S3** (DVP).

Include the header in your sketch:

.. code-block:: cpp

    #include "ESP_Video.h"

Video device nodes
******************

After hardware initialization, capture uses standard device paths defined in ``esp_video_device.h``:

* ``ESP_VIDEO_MIPI_CSI_DEVICE_NAME`` — ``/dev/video0`` (MIPI-CSI camera)
* ``ESP_VIDEO_ISP_DVP_DEVICE_NAME`` — ``/dev/video1`` (ISP + DVP)
* ``ESP_VIDEO_DVP_DEVICE_NAME`` — ``/dev/video2`` (DVP without ISP path)
* Additional nodes exist for SPI, USB UVC, JPEG/H264 codec, and ISP devices when enabled in the IDF configuration.

Arduino-ESP32 ESP Video API
---------------------------

Overview
********

The API is organized in seven layers:

1. **Pixel Format** — ``esp_video_format_t`` and ``ESPVideoFormatClass``
2. **Resolution** — ``ESPVideoSolutionClass`` (frame width and height)
3. **Hardware Configuration** — ``ESPVideoCamConfigClass``, ``ESPVideoCSIConfigClass``, ``ESPVideoDVPPinsConfigClass``, ``ESPVideoDVPConfigClass``
4. **Hardware Initialization** — ``ESPVideoClass::begin()``
5. **Capture Device** — ``ESPVideoCaptureDevClass`` (inherits format and resolution bases)
6. **Buffer** — ``ESPVideoBufferClass`` (inherits format and resolution bases)
7. **Sensor Controls** — ``ESPVideoCaptureDevClass::setSensor*()`` methods

MIPI-CSI flow (ESP32-P4):

.. code-block:: cpp

    #include "esp_video_device.h"

    ESPVideoClass video;
    ESPVideoCamConfigClass cam_config;
    cam_config.begin(port, scl, sda);

    ESPVideoCSIConfigClass csi_config;
    csi_config.begin(cam_config);

    if (!video.begin(csi_config)) {
        // handle error
    }

    ESPVideoCaptureDevClass capture;
    if (!capture.begin(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, 2)) {
        // handle error
    }
    if (!capture.setFormat(ESP_VIDEO_FORMAT_RGB565)) {
        // handle error
    }
    if (!capture.startCapture()) {
        // handle error
    }

    ESPVideoBufferClass frame = capture.captureBuffer();
    if (frame.valid()) {
        uint8_t *pixels = frame.data();
        size_t nbytes = frame.size();
        const char *format_name = frame.formatName();  // e.g. "JPEG", "RGB565"
        uint32_t width = frame.getWidth();
        uint32_t height = frame.getHeight();
        // use frame; buffer is returned to the driver when `frame` is destroyed
    }

DVP flow (ESP32-S3 or ESP32-P4):

.. code-block:: cpp

    #include "esp_video_device.h"

    ESPVideoClass video;
    ESPVideoCamConfigClass cam_config;
    cam_config.begin(port, scl, sda);

    ESPVideoDVPPinsConfigClass dvp_pins;
    dvp_pins.begin(vsync, de, pclk, xclk, d0, d1, d2, d3, d4, d5, d6, d7);

    ESPVideoDVPConfigClass dvp_config;
    dvp_config.begin(cam_config, dvp_pins, xclk_freq_hz);

    if (!video.begin(dvp_config)) {
        // handle error
    }

    ESPVideoCaptureDevClass capture;
    if (!capture.begin(ESP_VIDEO_DVP_DEVICE_NAME, 2)) {
        // handle error
    }
    if (!capture.startCapture()) {
        // handle error
    }

    ESPVideoBufferClass frame = capture.captureBuffer();
    if (frame.valid()) {
        uint8_t *pixels = frame.data();
        size_t nbytes = frame.size();
        const char *format_name = frame.formatName();
        uint32_t width = frame.getWidth();
        uint32_t height = frame.getHeight();
        // use frame; buffer is returned to the driver when `frame` is destroyed
    }

Design notes
************

* **Configuration objects are value types.** They store pin and bus settings only and can be copied or assigned. Pin reservation through ``periman`` is performed by ``ESPVideoClass::begin()`` and released in ``ESPVideoClass::end()`` or the destructor.
* **CSI and DVP can coexist** on SoCs that support both (for example ESP32-P4). Each interface is tracked independently via internal flags and released in ``end()`` or the destructor.
* **Capture device opening is unified.** ``ESPVideoCaptureDevClass::begin()`` opens the device, reads and re-applies the current V4L2 format (``VIDIOC_G_FMT`` / ``VIDIOC_S_FMT``), stores width and height, and requests capture buffers.
* **Format and resolution are shared base classes.** ``ESPVideoFormatClass`` and ``ESPVideoSolutionClass`` hold pixel format and frame dimensions. Both ``ESPVideoCaptureDevClass`` and ``ESPVideoBufferClass`` inherit from them, so metadata is available on the device and on each dequeued frame.
* **Pixel format is an Arduino enum.** Use ``setFormat()`` before ``startCapture()`` to select a supported ``esp_video_format_t`` value. If the format changes after buffers were allocated, the driver buffers are re-requested automatically.
* **Frame metadata travels with the buffer.** ``ESPVideoBufferClass::formatName()`` / ``formatType()`` and ``getWidth()`` / ``getHeight()`` reflect the capture device's active format and resolution when the frame was dequeued.
* **Resource-owning classes are non-copyable** (``ESPVideoClass``, ``ESPVideoCaptureDevClass``); ``ESPVideoBufferClass`` is move-only.

Pixel format
************

``esp_video_format_t`` is the Arduino-side pixel format identifier.
``ESPVideoCaptureDevClass::setFormat()`` maps each value to a V4L2 fourcc before ``VIDIOC_S_FMT`` is issued.
Format metadata is stored in ``ESPVideoFormatClass``, which is inherited by the capture device and each buffer.

.. code-block:: cpp

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

Supported formats and their V4L2 mappings:

.. list-table::
   :header-rows: 1
   :widths: 30 30 40

   * - ``esp_video_format_t``
     - Name string
     - V4L2 fourcc
   * - ``ESP_VIDEO_FORMAT_RAW8``
     - ``RAW8``
     - ``V4L2_PIX_FMT_SRGGB8``
   * - ``ESP_VIDEO_FORMAT_RAW10``
     - ``RAW10``
     - ``V4L2_PIX_FMT_SGRBG10``
   * - ``ESP_VIDEO_FORMAT_RAW12``
     - ``RAW12``
     - ``V4L2_PIX_FMT_SGRBG12``
   * - ``ESP_VIDEO_FORMAT_RGB565``
     - ``RGB565``
     - ``V4L2_PIX_FMT_RGB565``
   * - ``ESP_VIDEO_FORMAT_RGB888``
     - ``RGB888``
     - ``V4L2_PIX_FMT_RGB24``
   * - ``ESP_VIDEO_FORMAT_YUV420``
     - ``YUV420``
     - ``V4L2_PIX_FMT_YUV420``
   * - ``ESP_VIDEO_FORMAT_YUV422_YUYV``
     - ``YUV422_YUYV``
     - ``V4L2_PIX_FMT_YUYV``
   * - ``ESP_VIDEO_FORMAT_YUV422_UYVY``
     - ``YUV422_UYVY``
     - ``V4L2_PIX_FMT_UYVY``
   * - ``ESP_VIDEO_FORMAT_GRAY8``
     - ``GRAY8``
     - ``V4L2_PIX_FMT_GREY``
   * - ``ESP_VIDEO_FORMAT_JPEG``
     - ``JPEG``
     - ``V4L2_PIX_FMT_JPEG``

``ESP_VIDEO_FORMAT_UNKNOWN`` and ``ESP_VIDEO_FORMAT_MAX`` are sentinel values; they cannot be passed to ``setFormat()``.
Actual availability depends on the sensor driver and the active video device.

Resolution
**********

``ESPVideoSolutionClass`` stores the active frame width and height.
It is inherited by ``ESPVideoCaptureDevClass`` (populated during ``begin()`` via ``VIDIOC_G_FMT``) and by ``ESPVideoBufferClass`` (copied from the capture device when a frame is dequeued).

Default constructor
^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoSolutionClass();

Creates an empty resolution object (width and height are zero until ``begin()`` is called).

Copy constructor and assignment operator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoSolutionClass(const ESPVideoSolutionClass &config);
    ESPVideoSolutionClass &operator=(const ESPVideoSolutionClass &config);

Copies width and height values.

begin
^^^^^

.. code-block:: cpp

    void begin(uint32_t width, uint32_t height);

Stores the frame dimensions. Called internally when the capture device reads the driver format.

getWidth / getHeight
^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    uint32_t getWidth() const;
    uint32_t getHeight() const;

Return the stored frame width and height.

Format metadata
***************

``ESPVideoFormatClass`` stores the active pixel format type and its human-readable name.
It is inherited by ``ESPVideoCaptureDevClass`` and ``ESPVideoBufferClass``.

Default constructor
^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoFormatClass();

Creates an empty format object (``ESP_VIDEO_FORMAT_UNKNOWN`` until ``begin()`` is called).

Copy constructor and assignment operator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoFormatClass(const ESPVideoFormatClass &config);
    ESPVideoFormatClass &operator=(const ESPVideoFormatClass &config);

Copies format type and name.

begin
^^^^^

.. code-block:: cpp

    void begin(esp_video_format_t format_type, const std::string &format_name = "");

Stores the format enum and optional name string.
When ``format_name`` is empty, ``getFormatName()`` derives the name from ``format_type``.

getFormat
^^^^^^^^^

.. code-block:: cpp

    esp_video_format_t getFormat() const;

Returns the stored ``esp_video_format_t`` value.

getFormatName
^^^^^^^^^^^^^

.. code-block:: cpp

    const char *getFormatName() const;

Returns the human-readable format name (for example ``"RGB565"`` or ``"JPEG"``).
If no name was provided to ``begin()``, the name is derived from ``getFormat()`` and cached on first access.

Camera configuration (SCCB / I2C)
*********************************

``ESPVideoCamConfigClass`` holds SCCB (I2C) settings shared by CSI and DVP setups.
Create an instance, call ``begin()`` to fill in the settings, then pass it to ``ESPVideoCSIConfigClass`` or ``ESPVideoDVPConfigClass``.

Default constructor
^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoCamConfigClass();

Creates an empty configuration object. Call one of the ``begin()`` overloads before use.

Copy constructor and assignment operator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoCamConfigClass(const ESPVideoCamConfigClass &config);
    ESPVideoCamConfigClass &operator=(const ESPVideoCamConfigClass &config);

Copies SCCB port or handle, pin numbers, and I2C frequency.
Does not reserve GPIOs; pin ownership remains with ``ESPVideoClass::begin()``.

begin (Arduino pin numbers)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool begin(i2c_port_num_t port, int8_t scl_pin, int8_t sda_pin,
               uint32_t i2c_freq = i2c_freq_hz,
               int8_t reset_pin = -1, int8_t pwdn_pin = -1);

* ``port`` — I2C port used for SCCB when the library initializes the bus.
* ``scl_pin`` / ``sda_pin`` — SCCB clock and data pins.
* ``i2c_freq`` — SCCB frequency in Hz (default 100 kHz).
* ``reset_pin`` / ``pwdn_pin`` — Optional sensor reset and power-down GPIOs; use ``-1`` if unused.

Returns ``true`` on success.

begin (existing I2C master bus)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool begin(i2c_master_bus_handle_t i2c_handle,
               uint32_t i2c_freq = i2c_freq_hz,
               int8_t reset_pin = -1, int8_t pwdn_pin = -1);

Use this when SCCB should reuse an I2C bus already created by your application.
In that case, the library does not initialize a new I2C driver for SCCB.

getSccbConfig
^^^^^^^^^^^^^

.. code-block:: cpp

    esp_video_init_sccb_config_t getSccbConfig() const;

Returns the SCCB section used by ESP-IDF ``esp_video`` initialization structures.

getResetPin / getPwdnPin
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    gpio_num_t getResetPin() const;
    gpio_num_t getPwdnPin() const;

Read-only accessors for optional sensor control GPIOs.

acquirePins / releasePins
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool acquirePins(void *bus);
    bool releasePins();

Reserve or release SCCB and optional reset/power-down GPIOs through ``periman``.
These are called automatically by ``ESPVideoClass::begin()`` and ``end()``; applications normally do not need to call them directly.

MIPI-CSI configuration
**********************

``ESPVideoCSIConfigClass`` extends ``ESPVideoCamConfigClass`` for MIPI-CSI + ISP initialization.
Available when ``CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE`` is enabled.

Default constructor
^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoCSIConfigClass();

Creates an empty CSI configuration object.

Copy constructor
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoCSIConfigClass(const ESPVideoCSIConfigClass &config);

Copies the base camera configuration and the LDO initialization flag.

begin
^^^^^

.. code-block:: cpp

    bool begin(const ESPVideoCamConfigClass &config, bool dont_init_ldo = false);

* ``config`` — Base camera / SCCB configuration.
* ``dont_init_ldo`` — When ``true``, skips LDO initialization inside the CSI driver (use if power is supplied externally).

Returns ``true`` on success.

getCsiInitConfig
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    esp_video_init_csi_config_t getCsiInitConfig() const;

Builds the ESP-IDF CSI initialization structure from the stored configuration.

getDontInitLdo
^^^^^^^^^^^^^^

.. code-block:: cpp

    bool getDontInitLdo() const;

Returns the LDO initialization skip flag.

DVP pin configuration
*********************

``ESPVideoDVPPinsConfigClass`` describes the parallel DVP data and sync pins.
Available when ``CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE`` is enabled.
The ``begin()`` overload always configures 8-bit data width (D0–D7).

Default constructor
^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoDVPPinsConfigClass();

Creates an empty DVP pin configuration object.

Copy constructor and assignment operator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoDVPPinsConfigClass(const ESPVideoDVPPinsConfigClass &config);
    ESPVideoDVPPinsConfigClass &operator=(const ESPVideoDVPPinsConfigClass &config);

Copies the stored ``esp_cam_ctlr_dvp_pin_config_t`` structure.

begin
^^^^^

.. code-block:: cpp

    bool begin(int8_t vsync_pin, int8_t de_pin, int8_t pclk_pin, int8_t xclk_pin,
               int8_t data0_pin, int8_t data1_pin, int8_t data2_pin, int8_t data3_pin,
               int8_t data4_pin, int8_t data5_pin, int8_t data6_pin, int8_t data7_pin);

* ``vsync_pin`` / ``de_pin`` / ``pclk_pin`` / ``xclk_pin`` — Sync and clock pins.
* ``data0_pin`` … ``data7_pin`` — Eight parallel data lines (D0–D7).

Returns ``true`` on success.

getDvpPin
^^^^^^^^^

.. code-block:: cpp

    const esp_cam_ctlr_dvp_pin_config_t &getDvpPin() const;

Returns the native ESP-IDF DVP pin structure.

DVP configuration
*****************

``ESPVideoDVPConfigClass`` combines SCCB settings, DVP pins, and XCLK frequency.
It inherits from both ``ESPVideoCamConfigClass`` and ``ESPVideoDVPPinsConfigClass``, so a single instance carries the full DVP setup.
Available when ``CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE`` is enabled.

Default constructor
^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoDVPConfigClass();

Creates an empty DVP configuration object.

Copy constructor and assignment operator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoDVPConfigClass(const ESPVideoDVPConfigClass &config);
    ESPVideoDVPConfigClass &operator=(const ESPVideoDVPConfigClass &config);

Copies SCCB settings, DVP pin mapping, and XCLK frequency.

begin
^^^^^

.. code-block:: cpp

    bool begin(const ESPVideoCamConfigClass &config,
               const ESPVideoDVPPinsConfigClass &dvp_pin,
               uint32_t xclk_freq);

* ``config`` — Base camera / SCCB configuration.
* ``dvp_pin`` — DVP data, sync, and clock pin mapping (see ESP-IDF camera controller docs).
* ``xclk_freq`` — Sensor external clock (XCLK) frequency in Hz.

Returns ``true`` on success.

getDvpInitConfig
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    esp_video_init_dvp_config_t getDvpInitConfig() const;

Builds the ESP-IDF DVP initialization structure from the stored configuration.

getXclkFreq
^^^^^^^^^^^

.. code-block:: cpp

    uint32_t getXclkFreq() const;

Returns the configured XCLK frequency in Hz.

acquirePins / releasePins
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool acquirePins(void *bus);
    bool releasePins();

Reserve or release SCCB, reset/power-down, and DVP parallel pins through ``periman``.
Called automatically by ``ESPVideoClass::begin()`` and ``end()`` for DVP setups.

ESPVideoClass
*************

``ESPVideoClass`` initializes the ``esp_video`` stack for CSI or DVP.
Create one instance per application (or use a static/global instance).

Destructor
^^^^^^^^^^

Calls ``end()`` to stop initialized subsystems and release reserved pins.

end
^^^

.. code-block:: cpp

    void end();

Stops the initialized subsystems via ``esp_video_deinit_with_flags()``, releases pins reserved during ``begin()``, and clears stored configuration.

begin (MIPI-CSI)
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool begin(const ESPVideoCSIConfigClass &config);

Available when ``CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE`` is enabled.

Initializes MIPI-CSI and ISP (``ESP_VIDEO_INIT_FLAGS_MIPI_CSI | ESP_VIDEO_INIT_FLAGS_ISP``).
Reserves pins through ``periman``, calls ``esp_video_init_with_flags()``, and stores the configuration for teardown.
Returns ``true`` if initialization succeeded or was already done.
On failure, reserved pins are released before returning ``false``.

isCSIInitialized
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool isCSIInitialized() const;

Returns ``true`` if CSI and ISP flags are active.

begin (DVP)
^^^^^^^^^^^

.. code-block:: cpp

    bool begin(const ESPVideoDVPConfigClass &config);

Available when ``CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE`` is enabled.

Initializes the DVP video device (``ESP_VIDEO_INIT_FLAGS_DVP``).
Follows the same pin reservation and rollback rules as the CSI ``begin()`` overload.

isDVPInitialized
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool isDVPInitialized() const;

Returns ``true`` if the DVP subsystem is initialized.

ESPVideoCaptureDevClass
***********************

Opens a V4L2 capture device, manages buffers, and dequeues frames.
Inherits ``ESPVideoSolutionClass`` and ``ESPVideoFormatClass`` for resolution and format metadata.
The class is non-copyable and non-movable; use a single long-lived instance or manage lifetime explicitly.

Default constructor
^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoCaptureDevClass();

Creates an empty instance. Call ``begin()`` before ``startCapture()``.

Destructor
^^^^^^^^^^

Calls ``end()`` to stop capture, unmap MMAP buffers, release driver buffers, and close the file descriptor.

begin
^^^^^

.. code-block:: cpp

    bool begin(const char *path, size_t buf_count = 2);

Opens the device at ``path`` if not already open, reads the driver format (``VIDIOC_G_FMT``), re-applies it (``VIDIOC_S_FMT``), stores width, height, and pixel format, and requests ``buf_count`` MMAP capture buffers.
The detected format and resolution are exposed through ``getFormat()``, ``getFormatName()``, ``getWidth()``, and ``getHeight()``.
If the device is already open, returns ``true`` immediately without changing the path.
On any failure, the device is closed and the instance remains unopened.

end
^^^

.. code-block:: cpp

    void end();

Stops capture, unmaps buffers, releases driver buffers, and closes the file descriptor.

requestBuffer
^^^^^^^^^^^^^

.. code-block:: cpp

    bool requestBuffer(size_t buf_count);

* ``buf_count`` — Number of capture buffers (commonly 2 for double buffering).

Releases any existing buffers, requests new MMAP buffers from the driver, and maps them into userspace.
On failure, partially allocated buffers are rolled back before returning ``false``.
Requires the device to be open (via ``begin()``).

startCapture
^^^^^^^^^^^^

.. code-block:: cpp

    bool startCapture();

Queues all buffers and starts the video stream (``VIDIOC_STREAMON``).
Requires buffers to be set up first.
Returns ``true`` on success.

stopCapture
^^^^^^^^^^^

.. code-block:: cpp

    bool stopCapture();

Stops the stream (``VIDIOC_STREAMOFF``) if capture was started.
Returns ``true`` on success or ``false`` if capture was not started or stopped.

captureBuffer
^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoBufferClass captureBuffer();

Dequeues one filled buffer (``VIDIOC_DQBUF``).
Returns an invalid buffer if capture is not running or dequeue fails.

isOpened
^^^^^^^^

.. code-block:: cpp

    bool isOpened() const;

Returns ``true`` if the device file descriptor is valid.

isCaptureStarted
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool isCaptureStarted() const;

Returns ``true`` if ``startCapture()`` succeeded and the stream has not been stopped.

getFormat
^^^^^^^^^

.. code-block:: cpp

    esp_video_format_t getFormat() const;

Inherited from ``ESPVideoFormatClass``.
Returns the active pixel format after ``begin()`` or a successful ``setFormat()`` call.
If the driver reports an unrecognized V4L2 fourcc during ``begin()``, returns ``ESP_VIDEO_FORMAT_UNKNOWN``.

getFormatName
^^^^^^^^^^^^^

.. code-block:: cpp

    const char *getFormatName() const;

Inherited from ``ESPVideoFormatClass``.
Returns the human-readable format name for ``getFormat()`` (for example ``"RGB565"`` or ``"JPEG"``).

getWidth / getHeight
^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    uint32_t getWidth() const;
    uint32_t getHeight() const;

Inherited from ``ESPVideoSolutionClass``.
Return the frame width and height, as reported by the driver during ``begin()``.

setFormat
^^^^^^^^^

.. code-block:: cpp

    bool setFormat(esp_video_format_t format);

Selects a pixel format before capture starts, or changes it while the device is open.
Issues ``VIDIOC_G_FMT`` / ``VIDIOC_S_FMT`` with the V4L2 fourcc that corresponds to ``format``.
If the format changes and capture buffers were already allocated, existing MMAP buffers are released and re-requested using the buffer count from the last ``begin()`` or ``requestBuffer()`` call.
Returns ``false`` for unsupported enum values, I/O errors, or buffer re-allocation failures.
Call this after ``begin()`` and before ``startCapture()`` in typical applications.

Sensor controls
^^^^^^^^^^^^^^^

These methods map to V4L2 extended controls (``VIDIOC_S_EXT_CTRLS``).
Availability depends on the sensor driver and current format.

setSensorGain
^^^^^^^^^^^^^

.. code-block:: cpp

    bool setSensorGain(int32_t gain);

Sets ``V4L2_CID_GAIN``.

setSensorExposure
^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool setSensorExposure(int32_t exposure);

Sets ``V4L2_CID_EXPOSURE``.

setSensorExposureTime
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool setSensorExposureTime(int32_t exposure_time);

Sets ``V4L2_CID_EXPOSURE_ABSOLUTE``.

setSensorAETargetLevel
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool setSensorAETargetLevel(int32_t target_level);

Sets ``V4L2_CID_CAMERA_AE_LEVEL``.

setSensorJPEGQuality
^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool setSensorJPEGQuality(int32_t quality);

Sets ``V4L2_CID_JPEG_COMPRESSION_QUALITY``.

setSensorVFlip
^^^^^^^^^^^^^^

.. code-block:: cpp

    bool setSensorVFlip(bool vflip);

Sets ``V4L2_CID_VFLIP``.

setSensorHFlip
^^^^^^^^^^^^^^

.. code-block:: cpp

    bool setSensorHFlip(bool hflip);

Sets ``V4L2_CID_HFLIP``.

setSensorTestPattern
^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool setSensorTestPattern(bool test_pattern);

Sets ``V4L2_CID_TEST_PATTERN``.

Each control method returns ``true`` if the driver accepted the value.

ESPVideoBufferClass
*******************

RAII wrapper for one dequeued frame.
Inherits ``ESPVideoSolutionClass`` and ``ESPVideoFormatClass``, so each frame carries its own format and resolution metadata.
When the object is destroyed, moved-from, or ``end()`` is called, the buffer is re-queued (``VIDIOC_QBUF``) so capture can continue.

The class is move-only (copy is deleted).

Move constructor and move assignment operator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    ESPVideoBufferClass(ESPVideoBufferClass &&other) noexcept;
    ESPVideoBufferClass &operator=(ESPVideoBufferClass &&other) noexcept;

Transfers buffer ownership to another instance.
The source object is left empty and no longer re-queues the buffer on destruction.

valid
^^^^^

.. code-block:: cpp

    bool valid() const;

Returns ``true`` if ``data()`` is non-null and ``size()`` is greater than zero.

data
^^^^

.. code-block:: cpp

    uint8_t *data() const;

Pointer to captured pixel data (MMAP mode).
Do not store this pointer past the lifetime of the ``ESPVideoBufferClass`` object.

size
^^^^

.. code-block:: cpp

    size_t size() const;

Number of valid bytes in the buffer (``bytesused`` from V4L2).

index
^^^^^

.. code-block:: cpp

    int index() const;

Driver buffer index, or ``-1`` after the buffer has been re-queued.

getWidth / getHeight
^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    uint32_t getWidth() const;
    uint32_t getHeight() const;

Inherited from ``ESPVideoSolutionClass``.
Return the frame width and height copied from the capture device when this buffer was dequeued.

formatName
^^^^^^^^^^

.. code-block:: cpp

    const char *formatName() const;

Convenience alias for ``getFormatName()`` inherited from ``ESPVideoFormatClass``.

formatType
^^^^^^^^^^

.. code-block:: cpp

    esp_video_format_t formatType() const;

Convenience alias for ``getFormat()`` inherited from ``ESPVideoFormatClass``.

end
^^^

.. code-block:: cpp

    void end();

Re-queues the buffer to the driver immediately without waiting for destruction.

.. note::
    Hold ``ESPVideoBufferClass`` only while processing one frame.
    Destroying, moving, or calling ``end()`` returns the buffer to the driver; failing to release buffers will stall ``captureBuffer()``.

Examples
--------

MIPI-CSI capture (ESP32-P4):

.. literalinclude:: ../../../libraries/ESP_Video/examples/mipi_csi_camera/mipi_csi_camera.ino
    :language: cpp

DVP capture (ESP32-S3 or ESP32-P4):

.. literalinclude:: ../../../libraries/ESP_Video/examples/dvp_camera/dvp_camera.ino
    :language: cpp

See also the `ESP_Video library`_ on GitHub for board-specific pin defines and Kconfig requirements.

.. _`ESP_Video library`: https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP_Video
