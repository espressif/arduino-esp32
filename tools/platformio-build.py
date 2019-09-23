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

from os.path import abspath, isdir, isfile, join

from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()
platform = env.PioPlatform()

FRAMEWORK_DIR = platform.get_package_dir("framework-arduinoespressif32")
assert isdir(FRAMEWORK_DIR)

env.Append(
    ASFLAGS=["-x", "assembler-with-cpp"],

    CFLAGS=[
        "-std=gnu99",
        "-Wno-old-style-declaration"
    ],

    CCFLAGS=[
        "-Os",
        "-g3",
        "-Wall",
        "-nostdlib",
        "-Wpointer-arith",
        "-Wno-error=unused-but-set-variable",
        "-Wno-error=unused-variable",
        "-mlongcalls",
        "-ffunction-sections",
        "-fdata-sections",
        "-fstrict-volatile-bitfields",
        "-Wno-error=deprecated-declarations",
        "-Wno-error=unused-function",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
        "-fstack-protector",
        "-fexceptions",
        "-Werror=reorder"
    ],

    CXXFLAGS=[
        "-fno-rtti",
        "-fno-exceptions",
        "-std=gnu++11"
    ],

    LINKFLAGS=[
        "-nostdlib",
        "-Wl,-static",
        "-u", "call_user_start_cpu0",
        "-Wl,--undefined=uxTopUsedPriority",
        "-Wl,--gc-sections",
        "-Wl,-EL",
        "-T", "esp32.common.ld",
        "-T", "esp32.rom.ld",
        "-T", "esp32.peripherals.ld",
        "-T", "esp32.rom.libgcc.ld",
        "-T", "esp32.rom.spiram_incompatible_fns.ld",
        "-u", "ld_include_panic_highint_hdl",
        "-u", "__cxa_guard_dummy",
        "-u", "__cxx_fatal_exception"
    ],

    CPPDEFINES=[
        "ESP32",
        "ESP_PLATFORM",
        ("F_CPU", "$BOARD_F_CPU"),
        "HAVE_CONFIG_H",
        ("MBEDTLS_CONFIG_FILE", '\\"mbedtls/esp_config.h\\"'),
        ("ARDUINO", 10805),
        "ARDUINO_ARCH_ESP32",
        ("ARDUINO_VARIANT", '\\"%s\\"' % env.BoardConfig().get("build.variant").replace('"', "")),
        ("ARDUINO_BOARD", '\\"%s\\"' % env.BoardConfig().get("name").replace('"', ""))
    ],

    CPPPATH=[
       join(FRAMEWORK_DIR, "tools", "sdk", "include", "config"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "app_trace"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "app_update"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "asio"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "bootloader_support"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "bt"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "coap"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "console"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "driver"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp-tls"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp32"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp_adc_cal"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp_event"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp_http_client"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp_http_server"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp_https_ota"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp_ringbuf"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "ethernet"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "expat"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "fatfs"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "freemodbus"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "freertos"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "heap"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "idf_test"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "jsmn"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "json"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "libsodium"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "log"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "lwip"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "mbedtls"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "mdns"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "micro-ecc"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "mqtt"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "newlib"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "nghttp"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "nvs_flash"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "openssl"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "protobuf-c"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "protocomm"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "pthread"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "sdmmc"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "smartconfig_ack"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "soc"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "spi_flash"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "spiffs"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "tcp_transport"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "tcpip_adapter"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "ulp"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "vfs"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "wear_levelling"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "wifi_provisioning"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "wpa_supplicant"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "xtensa-debug-module"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp-face"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp32-camera"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "esp-face"),
        join(FRAMEWORK_DIR, "tools", "sdk", "include", "fb_gfx"),
        join(FRAMEWORK_DIR, "cores", env.BoardConfig().get("build.core"))
    ],

    LIBPATH=[
        join(FRAMEWORK_DIR, "tools", "sdk", "lib"),
        join(FRAMEWORK_DIR, "tools", "sdk", "ld")
    ],

    LIBS=[
        "-lgcc", "-lopenssl", "-lbtdm_app", "-lfatfs", "-lwps", "-lcoexist", "-lwear_levelling", "-lesp_http_client", "-lprotobuf-c", "-lhal", "-lnewlib", "-ldriver", "-lbootloader_support", "-lpp", "-lfreemodbus", "-lmesh", "-lsmartconfig", "-ljsmn", "-lwpa", "-lethernet", "-lphy", "-lapp_trace", "-lconsole", "-lulp", "-lwpa_supplicant", "-lfreertos", "-lbt", "-lmicro-ecc", "-lesp32-camera", "-lcxx", "-lxtensa-debug-module", "-ltcp_transport", "-lmdns", "-lvfs", "-lesp_ringbuf", "-lsoc", "-lcore", "-lfb_gfx", "-lsdmmc", "-llibsodium", "-lcoap", "-ltcpip_adapter", "-lprotocomm", "-lesp_event", "-limage_util", "-lc_nano", "-lesp-tls", "-lasio", "-lrtc", "-lspi_flash", "-lwpa2", "-lwifi_provisioning", "-lesp32", "-lface_recognition", "-lapp_update", "-lnghttp", "-lspiffs", "-lface_detection", "-lespnow", "-lnvs_flash", "-lesp_adc_cal", "-llog", "-ldl_lib", "-lsmartconfig_ack", "-lexpat", "-lm", "-lfr", "-lmqtt", "-lc", "-lheap", "-lmbedtls", "-llwip", "-lnet80211", "-lesp_http_server", "-lpthread", "-ljson", "-lesp_https_ota", "-lfd", "-lstdc++"
    ],

    LIBSOURCE_DIRS=[
        join(FRAMEWORK_DIR, "libraries")
    ],

    FLASH_EXTRA_IMAGES=[
        ("0x1000", join(FRAMEWORK_DIR, "tools", "sdk", "bin", "bootloader_${BOARD_FLASH_MODE}_${__get_board_f_flash(__env__)}.bin")),
        ("0x8000", join(env.subst("$BUILD_DIR"), "partitions.bin")),
        ("0xe000", join(FRAMEWORK_DIR, "tools", "partitions", "boot_app0.bin"))
    ]
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

fwpartitions_dir = join(FRAMEWORK_DIR, "tools", "partitions")
partitions_csv = env.BoardConfig().get("build.partitions", "default.csv")
env.Replace(
    PARTITIONS_TABLE_CSV=abspath(
        join(fwpartitions_dir, partitions_csv) if isfile(
            join(fwpartitions_dir, partitions_csv)) else partitions_csv))

partition_table = env.Command(
    join("$BUILD_DIR", "partitions.bin"),
    "$PARTITIONS_TABLE_CSV",
    env.VerboseAction('"$PYTHONEXE" "%s" -q $SOURCE $TARGET' % join(
        FRAMEWORK_DIR, "tools", "gen_esp32part.py"),
                      "Generating partitions $TARGET"))
env.Depends("$BUILD_DIR/$PROGNAME$PROGSUFFIX", partition_table)
