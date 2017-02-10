# Copyright 2014-present PlatformIO <contact@platformio.org>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Arduino

Arduino Wiring-based Framework allows writing cross-platform software to
control devices attached to a wide range of Arduino boards to create all
kinds of creative coding, interactive objects, spaces or physical experiences.

http://arduino.cc/en/Reference/HomePage
"""

# Extends: https://github.com/platformio/platform-espressif32/blob/develop/builder/main.py

from os.path import isdir, join

from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()
platform = env.PioPlatform()

FRAMEWORK_DIR = platform.get_package_dir("framework-arduinoespressif32")
assert isdir(FRAMEWORK_DIR)

env.Prepend(
    CPPDEFINES=[
        ("ARDUINO", 10610),
        "ARDUINO_ARCH_ESP32"
    ],

    CFLAGS=["-Wno-old-style-declaration"],

    CCFLAGS=[
        "-Wno-error=deprecated-declarations",
        "-Wno-unused-parameter",
        "-Wno-sign-compare"
    ],

    CPPPATH=[
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "config"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "bluedroid"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "bt"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "driver"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp32"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "ethernet"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "fatfs"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "freertos"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "log"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "mdns"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "mbedtls"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "mbedtls_port"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "vfs"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "ulp"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "newlib"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "nvs_flash"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "spi_flash"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "sdmmc"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "openssl"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "app_update"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "tcpip_adapter"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "xtensa-debug-module"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "newlib"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "coap"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "wpa_supplicant"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "expat"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "json"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "nghttp"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "lwip"),
        join(FRAMEWORK_DIR, "cores", env.BoardConfig().get("build.core"))
    ],

    LIBPATH=[
        join(FRAMEWORK_DIR, "tools", "sdk", "lib"),
        join(FRAMEWORK_DIR, "tools", "sdk", "ld")
    ],

    LIBS=[
        "gcc", "stdc++", "app_update", "bootloader_support", "bt", "btdm_app", "c", "c_nano", "coap", "coexist", "core", "cxx", "driver", "esp32", "ethernet", "expat", "fatfs", "freertos", "hal", "json", "log", "lwip", "m", "mbedtls", "mdns", "micro-ecc", "net80211", "newlib", "nghttp", "nvs_flash", "openssl", "phy", "pp", "rtc", "sdmmc", "smartconfig", "spi_flash", "tcpip_adapter", "ulp", "vfs", "wpa", "wpa2", "wpa_supplicant", "wps", "xtensa-debug-module"
    ],

    UPLOADERFLAGS=[
        "--before", "default_reset",
        "--after", "hard_reset"
    ]
)

env.Append(
    LIBSOURCE_DIRS=[
        join(FRAMEWORK_DIR, "libraries")
    ],

    LINKFLAGS=[
        "-Wl,-EL",
        "-T", "esp32.common.ld",
        "-T", "esp32.rom.ld",
        "-T", "esp32.peripherals.ld"
    ],

    UPLOADERFLAGS=[
        "0x1000", '"%s"' % join(FRAMEWORK_DIR, "tools", "sdk", "bin", "bootloader.bin"),
        "0x8000", '"%s"' % join("$BUILD_DIR", "partitions.bin"),
        "0xe000", '"%s"' % join(FRAMEWORK_DIR, "tools", "partitions", "boot_app0.bin"),
        "0x10000"
    ]
)

env.Replace(
    UPLOADER=join(FRAMEWORK_DIR, "tools", "esptool.py")
)

#
# Target: Build Core Library
#

libs = []

if "build.variant" in env.BoardConfig():
    env.Append(
        CPPPATH=[
            join(FRAMEWORK_DIR, "variants",
                 env.BoardConfig().get("build.variant"))
        ]
    )
    libs.append(env.BuildLibrary(
        join("$BUILD_DIR", "FrameworkArduinoVariant"),
        join(FRAMEWORK_DIR, "variants", env.BoardConfig().get("build.variant"))
    ))

envsafe = env.Clone()

libs.append(envsafe.BuildLibrary(
    join("$BUILD_DIR", "FrameworkArduino"),
    join(FRAMEWORK_DIR, "cores", env.BoardConfig().get("build.core"))
))

env.Prepend(LIBS=libs)

#
# Generate partition table
#

partition_table = env.Command(
    join("$BUILD_DIR", "partitions.bin"),
    join(FRAMEWORK_DIR, "tools", "partitions", "default.csv"),
    env.VerboseAction('"$PYTHONEXE" "%s" -q $SOURCE $TARGET' %
                      join(FRAMEWORK_DIR, "tools", "gen_esp32part.py"),
                      "Generating partitions $TARGET"))
env.Depends("$BUILD_DIR/$PROGNAME$PROGSUFFIX", partition_table)
