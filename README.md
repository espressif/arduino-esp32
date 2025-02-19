# Arduino core for the ESP32, ESP32-P4, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6 and ESP32-H2

[![Build Status](https://img.shields.io/github/actions/workflow/status/espressif/arduino-esp32/push.yml?branch=master&event=push&label=Compilation%20Tests)](https://github.com/espressif/arduino-esp32/actions/workflows/push.yml?query=branch%3Amaster+event%3Apush)
[![Verbose Build Status](https://img.shields.io/github/actions/workflow/status/espressif/arduino-esp32/push.yml?branch=master&event=schedule&label=Compilation%20Tests%20(Verbose))](https://github.com/espressif/arduino-esp32/actions/workflows/push.yml?query=branch%3Amaster+event%3Aschedule)
[![External Libraries Test](https://img.shields.io/github/actions/workflow/status/espressif/arduino-esp32/lib.yml?branch=master&event=schedule&label=External%20Libraries%20Test)](https://github.com/espressif/arduino-esp32/blob/gh-pages/LIBRARIES_TEST.md)
[![Runtime Tests](https://github.com/espressif/arduino-esp32/blob/gh-pages/runtime-tests-results/badge.svg)](https://github.com/espressif/arduino-esp32/actions/workflows/tests_results.yml)

### Need help or have a question? Join the chat at [Discord](https://discord.gg/8xY6e9crwv) or [open a new Discussion](https://github.com/espressif/arduino-esp32/discussions)

[![Discord invite](https://img.shields.io/discord/1327272229427216425?logo=discord&logoColor=white&logoSize=auto&label=Discord)](https://discord.gg/8xY6e9crwv)

## Contents

  - [Development Status](#development-status)
  - [Development Planning](#development-planning)
  - [Documentation](#documentation)
  - [Supported Chips](#supported-chips)
  - [Decoding exceptions](#decoding-exceptions)
  - [Issue/Bug report template](#issuebug-report-template)
  - [Contributing](#contributing)

### Development Status

#### Latest Stable Release

[![Release Version](https://img.shields.io/github/release/espressif/arduino-esp32.svg)](https://github.com/espressif/arduino-esp32/releases/latest/)
[![Release Date](https://img.shields.io/github/release-date/espressif/arduino-esp32.svg)](https://github.com/espressif/arduino-esp32/releases/latest/)
[![Downloads](https://img.shields.io/github/downloads/espressif/arduino-esp32/latest/total.svg)](https://github.com/espressif/arduino-esp32/releases/latest/)

#### Latest Development Release

[![Release Version](https://img.shields.io/github/release/espressif/arduino-esp32/all.svg)](https://github.com/espressif/arduino-esp32/releases/)
[![Release Date](https://img.shields.io/github/release-date-pre/espressif/arduino-esp32.svg)](https://github.com/espressif/arduino-esp32/releases/)
[![Downloads](https://img.shields.io/github/downloads-pre/espressif/arduino-esp32/latest/total.svg)](https://github.com/espressif/arduino-esp32/releases/)

### Development Planning

Our Development is fully tracked on this public **[Roadmap 🎉](https://github.com/orgs/espressif/projects/3)**

For even more information you can join our **[Monthly Community Meetings 🔔](https://github.com/espressif/arduino-esp32/discussions/categories/monthly-community-meetings).**

### Documentation

You can use the [Arduino-ESP32 Online Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/) to get all information about this project.

---

**Migration guide from version 2.x to 3.x is available [here](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html).**

---

**APIs compatibility with ESP8266 and Arduino-CORE (Arduino.cc) is explained [here](https://docs.espressif.com/projects/arduino-esp32/en/latest/libraries.html#apis).**

---

* [Getting Started](https://docs.espressif.com/projects/arduino-esp32/en/latest/getting_started.html)
* [Installing (Windows, Linux and macOS)](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
* [Libraries](https://docs.espressif.com/projects/arduino-esp32/en/latest/libraries.html)
* [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
* [FAQ](https://docs.espressif.com/projects/arduino-esp32/en/latest/faq.html)
* [Troubleshooting](https://docs.espressif.com/projects/arduino-esp32/en/latest/troubleshooting.html)

### Supported Chips

Here are the ESP32 series supported by the Arduino-ESP32 project:

| **SoC**  | **Stable** | **Development** |                                           **Datasheet**                                           |
|----------|:----------:|:---------------:|:-------------------------------------------------------------------------------------------------:|
| ESP32    |     Yes    |       Yes       |    [ESP32](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)    |
| ESP32-S2 |     Yes    |       Yes       | [ESP32-S2](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf) |
| ESP32-C3 |     Yes    |       Yes       | [ESP32-C3](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf) |
| ESP32-S3 |     Yes    |       Yes       | [ESP32-S3](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf) |
| ESP32-C6 |     Yes    |       Yes       | [ESP32-C6](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf) |
| ESP32-H2 |     Yes    |       Yes       | [ESP32-H2](https://www.espressif.com/sites/default/files/documentation/esp32-h2_datasheet_en.pdf) |
| ESP32-P4 |     Yes    |       Yes       | [ESP32-P4](https://www.espressif.com/sites/default/files/documentation/esp32-p4_datasheet_en.pdf) |

> [!NOTE]
> ESP32-C2 is also supported by Arduino-ESP32 but requires rebuilding the static libraries. This is not trivial and requires a good understanding of the ESP-IDF
> build system. For more information, see the [Lib Builder documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/lib_builder.html).

For more details visit the [supported chips](https://docs.espressif.com/projects/arduino-esp32/en/latest/getting_started.html#supported-soc-s) documentation page.

### Decoding exceptions

You can use [EspExceptionDecoder](https://github.com/me-no-dev/EspExceptionDecoder) to get meaningful call trace.

### Issue/Bug report template

Before reporting an issue, make sure you've searched for similar one that was already created. Also make sure to go through all the issues labeled as [Type: For reference](https://github.com/espressif/arduino-esp32/issues?q=is%3Aissue+label%3A%22Type%3A+For+reference%22+).

Finally, if you are sure no one else had the issue, follow the **Issue template** or **Feature request template** while reporting any [new Issue](https://github.com/espressif/arduino-esp32/issues/new/choose).

### External libraries compilation test

We have set-up CI testing for external libraries for ESP32 Arduino core. You can check test results in the file [LIBRARIES_TEST](https://github.com/espressif/arduino-esp32/blob/gh-pages/LIBRARIES_TEST.md).
For more information and how to add your library to the test see [external library testing](https://docs.espressif.com/projects/arduino-esp32/en/latest/external_libraries_test.html) in the documentation.

### Contributing

We welcome contributions to the Arduino ESP32 project!

See [contributing](https://docs.espressif.com/projects/arduino-esp32/en/latest/contributing.html) in the documentation for more information on how to contribute to the project.

> We would like to have this repository in a polite and friendly atmosphere, so please be kind and respectful to others. For more details, look at [Code of Conduct](https://github.com/espressif/arduino-esp32/blob/master/CODE_OF_CONDUCT.md).
