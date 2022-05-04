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

from os.path import abspath, basename, isdir, isfile, join

from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()
platform = env.PioPlatform()

FRAMEWORK_DIR = platform.get_package_dir("framework-arduinoespressif32")
assert isdir(FRAMEWORK_DIR)

env.Append(
    ASFLAGS=[
        "-x", "assembler-with-cpp"
    ],

    CFLAGS=[
        "-march=rv32imc",
        "-std=gnu99",
        "-Wno-old-style-declaration"
    ],

    CXXFLAGS=[
        "-march=rv32imc",
        "-std=gnu++11",
        "-fexceptions",
        "-fno-rtti"
    ],

    CCFLAGS=[
        "-ffunction-sections",
        "-fdata-sections",
        "-Wno-error=unused-function",
        "-Wno-error=unused-variable",
        "-Wno-error=deprecated-declarations",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
        "-ggdb",
        "-Wno-error=format=",
        "-nostartfiles",
        "-Wno-format",
        "-Os",
        "-freorder-blocks",
        "-Wwrite-strings",
        "-fstack-protector",
        "-fstrict-volatile-bitfields",
        "-Wno-error=unused-but-set-variable",
        "-fno-jump-tables",
        "-fno-tree-switch-conversion",
        "-MMD"
    ],

    LINKFLAGS=[
        "-nostartfiles",
        "-march=rv32imc",
        "--specs=nosys.specs",
        "-Wl,--cref",
        "-Wl,--gc-sections",
        "-fno-rtti",
        "-fno-lto",
        "-Wl,--undefined=uxTopUsedPriority",
        "-T", "memory.ld",
        "-T", "sections.ld",
        "-T", "esp32c3.rom.ld",
        "-T", "esp32c3.rom.api.ld",
        "-T", "esp32c3.rom.libgcc.ld",
        "-T", "esp32c3.rom.newlib.ld",
        "-T", "esp32c3.rom.version.ld",
        "-T", "esp32c3.rom.newlib-time.ld",
        "-T", "esp32c3.rom.eco3.ld",
        "-T", "esp32c3.peripherals.ld",
        "-u", "_Z5setupv",
        "-u", "_Z4loopv",
        "-u", "esp_app_desc",
        "-u", "pthread_include_pthread_impl",
        "-u", "pthread_include_pthread_cond_impl",
        "-u", "pthread_include_pthread_local_storage_impl",
        "-u", "pthread_include_pthread_rwlock_impl",
        "-u", "include_esp_phy_override",
        "-u", "start_app",
        "-u", "__ubsan_include",
        "-u", "__assert_func",
        "-u", "vfs_include_syscalls_impl",
        "-u", "app_main",
        "-u", "newlib_include_heap_impl",
        "-u", "newlib_include_syscalls_impl",
        "-u", "newlib_include_pthread_impl",
        "-u", "newlib_include_assert_impl",
        "-u", "__cxa_guard_dummy",
        '-Wl,-Map="%s"' % join("${BUILD_DIR}", "${PROGNAME}.map")
    ],

    CPPPATH=[
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "config"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "newlib", "platform_include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "freertos", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "freertos", "include", "esp_additions", "freertos"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "freertos", "port", "riscv", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "freertos", "include", "esp_additions"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_hw_support", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_hw_support", "include", "soc"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_hw_support", "include", "soc", "esp32c3"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_hw_support", "port", "esp32c3"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_hw_support", "port", "esp32c3", "private_include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "heap", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "log", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "lwip", "include", "apps"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "lwip", "include", "apps", "sntp"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "lwip", "lwip", "src", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "lwip", "port", "esp32", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "lwip", "port", "esp32", "include", "arch"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "soc", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "soc", "esp32c3"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "soc", "esp32c3", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "hal", "esp32c3", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "hal", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "hal", "platform_port", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_rom", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_rom", "include", "esp32c3"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_rom", "esp32c3"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_common", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_system", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_system", "port", "soc"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_system", "port", "include", "riscv"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_system", "port", "public_compat"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "riscv", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "driver", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "driver", "esp32c3", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_pm", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_ringbuf", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "efuse", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "efuse", "esp32c3", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "vfs", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_wifi", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_event", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_netif", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_eth", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "tcpip_adapter", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_phy", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_phy", "esp32c3", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_ipc", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "app_trace", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_timer", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "mbedtls", "port", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "mbedtls", "mbedtls", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "mbedtls", "esp_crt_bundle", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "app_update", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "spi_flash", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "bootloader_support", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "nvs_flash", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "pthread", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_gdbstub", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_gdbstub", "riscv"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_gdbstub", "esp32c3"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "espcoredump", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "espcoredump", "include", "port", "riscv"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "wpa_supplicant", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "wpa_supplicant", "port", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "wpa_supplicant", "esp_supplicant", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "ieee802154", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "console"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "asio", "asio", "asio", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "asio", "port", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "bt", "common", "osi", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "bt", "include", "esp32c3", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "bt", "common", "api", "include", "api"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "bt", "common", "btc", "profile", "esp", "blufi", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "bt", "common", "btc", "profile", "esp", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "bt", "host", "bluedroid", "api", "include", "api"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "cbor", "port", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "unity", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "unity", "unity", "src"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "cmock", "CMock", "src"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "coap", "port", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "coap", "libcoap", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "nghttp", "port", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "nghttp", "nghttp2", "lib", "includes"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-tls"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-tls", "esp-tls-crypto"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_adc_cal", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_hid", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "tcp_transport", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_http_client", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_http_server", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_https_ota", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_https_server", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_lcd", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_lcd", "interface"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "protobuf-c", "protobuf-c"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "protocomm", "include", "common"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "protocomm", "include", "security"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "protocomm", "include", "transports"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "mdns", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_local_ctrl", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "sdmmc", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_serial_slave_link", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_websocket_client", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "expat", "expat", "expat", "lib"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "expat", "port", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "wear_levelling", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "fatfs", "diskio"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "fatfs", "vfs"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "fatfs", "src"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "freemodbus", "common", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "idf_test", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "idf_test", "include", "esp32c3"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "jsmn", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "json", "cJSON"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "libsodium", "libsodium", "src", "libsodium", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "libsodium", "port_include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "mqtt", "esp-mqtt", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "openssl", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "spiffs", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "wifi_provisioning", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "button", "button", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "rmaker_common", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "json_parser", "upstream", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "json_parser", "upstream"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "json_generator", "upstream"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_schedule", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_rainmaker", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "qrcode", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "ws2812_led"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "dotprod", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "support", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "windows", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "windows", "hann", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "windows", "blackman", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "windows", "blackman_harris", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "windows", "blackman_nuttall", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "windows", "nuttall", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "windows", "flat_top", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "iir", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "fir", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "math", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "math", "add", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "math", "sub", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "math", "mul", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "math", "addc", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "math", "mulc", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "math", "sqrt", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "matrix", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "fft", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "dct", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "conv", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "common", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "kalman", "ekf", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dsp", "modules", "kalman", "ekf_imu13states", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_littlefs", "src"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp_littlefs", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dl", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dl", "include", "tool"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dl", "include", "typedef"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dl", "include", "image"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dl", "include", "math"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dl", "include", "nn"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dl", "include", "layer"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dl", "include", "detect"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "esp-dl", "include", "model_zoo"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "include", "fb_gfx", "include"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", env.BoardConfig().get("build.arduino.memory_type", "qspi_qspi"), "include"),
        join(FRAMEWORK_DIR, "cores", env.BoardConfig().get("build.core"))
    ],

    LIBPATH=[
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "lib"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "ld"),
        join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", env.BoardConfig().get("build.arduino.memory_type", "qspi_qspi"))
    ],

    LIBS=[
        "-lesp_ringbuf", "-lefuse", "-lesp_ipc", "-ldriver", "-lesp_pm", "-lmbedtls", "-lapp_update", "-lbootloader_support", "-lspi_flash", "-lnvs_flash", "-lpthread", "-lesp_gdbstub", "-lespcoredump", "-lesp_phy", "-lesp_system", "-lesp_rom", "-lhal", "-lvfs", "-lesp_eth", "-ltcpip_adapter", "-lesp_netif", "-lesp_event", "-lwpa_supplicant", "-lesp_wifi", "-lconsole", "-llwip", "-llog", "-lheap", "-lsoc", "-lesp_hw_support", "-lriscv", "-lesp_common", "-lesp_timer", "-lfreertos", "-lnewlib", "-lcxx", "-lapp_trace", "-lasio", "-lbt", "-lcbor", "-lunity", "-lcmock", "-lcoap", "-lnghttp", "-lesp-tls", "-lesp_adc_cal", "-lesp_hid", "-ltcp_transport", "-lesp_http_client", "-lesp_http_server", "-lesp_https_ota", "-lesp_https_server", "-lesp_lcd", "-lprotobuf-c", "-lprotocomm", "-lmdns", "-lesp_local_ctrl", "-lsdmmc", "-lesp_serial_slave_link", "-lesp_websocket_client", "-lexpat", "-lwear_levelling", "-lfatfs", "-lfreemodbus", "-ljsmn", "-ljson", "-llibsodium", "-lmqtt", "-lopenssl", "-lspiffs", "-lwifi_provisioning", "-lbutton", "-lrmaker_common", "-ljson_parser", "-ljson_generator", "-lesp_schedule", "-lesp_rainmaker", "-lqrcode", "-lws2812_led", "-lesp-dsp", "-lesp_littlefs", "-lfb_gfx", "-lasio", "-lcbor", "-lcmock", "-lunity", "-lcoap", "-lesp_lcd", "-lesp_websocket_client", "-lexpat", "-lfreemodbus", "-ljsmn", "-llibsodium", "-lesp_adc_cal", "-lesp_hid", "-lfatfs", "-lwear_levelling", "-lopenssl", "-lspiffs", "-lesp_rainmaker", "-lesp_local_ctrl", "-lesp_https_server", "-lwifi_provisioning", "-lprotocomm", "-lbt", "-lbtdm_app", "-lprotobuf-c", "-lmdns", "-ljson", "-lrmaker_common", "-lmqtt", "-ljson_parser", "-ljson_generator", "-lesp_schedule", "-lqrcode", "-lcat_face_detect", "-lhuman_face_detect", "-lcolor_detect", "-lmfn", "-ldl", "-lesp_ringbuf", "-lefuse", "-lesp_ipc", "-ldriver", "-lesp_pm", "-lmbedtls", "-lapp_update", "-lbootloader_support", "-lspi_flash", "-lnvs_flash", "-lpthread", "-lesp_gdbstub", "-lespcoredump", "-lesp_phy", "-lesp_system", "-lesp_rom", "-lhal", "-lvfs", "-lesp_eth", "-ltcpip_adapter", "-lesp_netif", "-lesp_event", "-lwpa_supplicant", "-lesp_wifi", "-lconsole", "-llwip", "-llog", "-lheap", "-lsoc", "-lesp_hw_support", "-lriscv", "-lesp_common", "-lesp_timer", "-lfreertos", "-lnewlib", "-lcxx", "-lapp_trace", "-lnghttp", "-lesp-tls", "-ltcp_transport", "-lesp_http_client", "-lesp_http_server", "-lesp_https_ota", "-lsdmmc", "-lesp_serial_slave_link", "-lmbedtls", "-lmbedcrypto", "-lmbedx509", "-lcoexist", "-lcore", "-lespnow", "-lmesh", "-lnet80211", "-lpp", "-lsmartconfig", "-lwapi", "-lesp_ringbuf", "-lefuse", "-lesp_ipc", "-ldriver", "-lesp_pm", "-lmbedtls", "-lapp_update", "-lbootloader_support", "-lspi_flash", "-lnvs_flash", "-lpthread", "-lesp_gdbstub", "-lespcoredump", "-lesp_phy", "-lesp_system", "-lesp_rom", "-lhal", "-lvfs", "-lesp_eth", "-ltcpip_adapter", "-lesp_netif", "-lesp_event", "-lwpa_supplicant", "-lesp_wifi", "-lconsole", "-llwip", "-llog", "-lheap", "-lsoc", "-lesp_hw_support", "-lriscv", "-lesp_common", "-lesp_timer", "-lfreertos", "-lnewlib", "-lcxx", "-lapp_trace", "-lnghttp", "-lesp-tls", "-ltcp_transport", "-lesp_http_client", "-lesp_http_server", "-lesp_https_ota", "-lsdmmc", "-lesp_serial_slave_link", "-lmbedtls", "-lmbedcrypto", "-lmbedx509", "-lcoexist", "-lcore", "-lespnow", "-lmesh", "-lnet80211", "-lpp", "-lsmartconfig", "-lwapi", "-lesp_ringbuf", "-lefuse", "-lesp_ipc", "-ldriver", "-lesp_pm", "-lmbedtls", "-lapp_update", "-lbootloader_support", "-lspi_flash", "-lnvs_flash", "-lpthread", "-lesp_gdbstub", "-lespcoredump", "-lesp_phy", "-lesp_system", "-lesp_rom", "-lhal", "-lvfs", "-lesp_eth", "-ltcpip_adapter", "-lesp_netif", "-lesp_event", "-lwpa_supplicant", "-lesp_wifi", "-lconsole", "-llwip", "-llog", "-lheap", "-lsoc", "-lesp_hw_support", "-lriscv", "-lesp_common", "-lesp_timer", "-lfreertos", "-lnewlib", "-lcxx", "-lapp_trace", "-lnghttp", "-lesp-tls", "-ltcp_transport", "-lesp_http_client", "-lesp_http_server", "-lesp_https_ota", "-lsdmmc", "-lesp_serial_slave_link", "-lmbedtls", "-lmbedcrypto", "-lmbedx509", "-lcoexist", "-lcore", "-lespnow", "-lmesh", "-lnet80211", "-lpp", "-lsmartconfig", "-lwapi", "-lesp_ringbuf", "-lefuse", "-lesp_ipc", "-ldriver", "-lesp_pm", "-lmbedtls", "-lapp_update", "-lbootloader_support", "-lspi_flash", "-lnvs_flash", "-lpthread", "-lesp_gdbstub", "-lespcoredump", "-lesp_phy", "-lesp_system", "-lesp_rom", "-lhal", "-lvfs", "-lesp_eth", "-ltcpip_adapter", "-lesp_netif", "-lesp_event", "-lwpa_supplicant", "-lesp_wifi", "-lconsole", "-llwip", "-llog", "-lheap", "-lsoc", "-lesp_hw_support", "-lriscv", "-lesp_common", "-lesp_timer", "-lfreertos", "-lnewlib", "-lcxx", "-lapp_trace", "-lnghttp", "-lesp-tls", "-ltcp_transport", "-lesp_http_client", "-lesp_http_server", "-lesp_https_ota", "-lsdmmc", "-lesp_serial_slave_link", "-lmbedtls", "-lmbedcrypto", "-lmbedx509", "-lcoexist", "-lcore", "-lespnow", "-lmesh", "-lnet80211", "-lpp", "-lsmartconfig", "-lwapi", "-lesp_ringbuf", "-lefuse", "-lesp_ipc", "-ldriver", "-lesp_pm", "-lmbedtls", "-lapp_update", "-lbootloader_support", "-lspi_flash", "-lnvs_flash", "-lpthread", "-lesp_gdbstub", "-lespcoredump", "-lesp_phy", "-lesp_system", "-lesp_rom", "-lhal", "-lvfs", "-lesp_eth", "-ltcpip_adapter", "-lesp_netif", "-lesp_event", "-lwpa_supplicant", "-lesp_wifi", "-lconsole", "-llwip", "-llog", "-lheap", "-lsoc", "-lesp_hw_support", "-lriscv", "-lesp_common", "-lesp_timer", "-lfreertos", "-lnewlib", "-lcxx", "-lapp_trace", "-lnghttp", "-lesp-tls", "-ltcp_transport", "-lesp_http_client", "-lesp_http_server", "-lesp_https_ota", "-lsdmmc", "-lesp_serial_slave_link", "-lmbedtls", "-lmbedcrypto", "-lmbedx509", "-lcoexist", "-lcore", "-lespnow", "-lmesh", "-lnet80211", "-lpp", "-lsmartconfig", "-lwapi", "-lesp_ringbuf", "-lefuse", "-lesp_ipc", "-ldriver", "-lesp_pm", "-lmbedtls", "-lapp_update", "-lbootloader_support", "-lspi_flash", "-lnvs_flash", "-lpthread", "-lesp_gdbstub", "-lespcoredump", "-lesp_phy", "-lesp_system", "-lesp_rom", "-lhal", "-lvfs", "-lesp_eth", "-ltcpip_adapter", "-lesp_netif", "-lesp_event", "-lwpa_supplicant", "-lesp_wifi", "-lconsole", "-llwip", "-llog", "-lheap", "-lsoc", "-lesp_hw_support", "-lriscv", "-lesp_common", "-lesp_timer", "-lfreertos", "-lnewlib", "-lcxx", "-lapp_trace", "-lnghttp", "-lesp-tls", "-ltcp_transport", "-lesp_http_client", "-lesp_http_server", "-lesp_https_ota", "-lsdmmc", "-lesp_serial_slave_link", "-lmbedtls", "-lmbedcrypto", "-lmbedx509", "-lcoexist", "-lcore", "-lespnow", "-lmesh", "-lnet80211", "-lpp", "-lsmartconfig", "-lwapi", "-lphy", "-lbtbb", "-lesp_phy", "-lphy", "-lbtbb", "-lesp_phy", "-lphy", "-lbtbb", "-lm", "-lnewlib", "-lstdc++", "-lpthread", "-lgcc", "-lcxx", "-lapp_trace", "-lgcov", "-lapp_trace", "-lgcov", "-lc"
    ],

    CPPDEFINES=[
        "HAVE_CONFIG_H",
        ("MBEDTLS_CONFIG_FILE", '\\"mbedtls/esp_config.h\\"'),
        "UNITY_INCLUDE_CONFIG_H",
        "WITH_POSIX",
        "_GNU_SOURCE",
        ("IDF_VER", '\\"v4.4.1-1-gb8050b365e\\"'),
        "ESP_PLATFORM",
        "_POSIX_READER_WRITER_LOCKS",
        "ARDUINO_ARCH_ESP32",
        "ESP32",
        ("F_CPU", "$BOARD_F_CPU"),
        ("ARDUINO", 10812),
        ("ARDUINO_VARIANT", '\\"%s\\"' % env.BoardConfig().get("build.variant").replace('"', "")),
        ("ARDUINO_BOARD", '\\"%s\\"' % env.BoardConfig().get("name").replace('"', "")),
        "ARDUINO_PARTITION_%s" % basename(env.BoardConfig().get(
            "build.partitions", "default.csv")).replace(".csv", "").replace("-", "_")
    ],

    LIBSOURCE_DIRS=[
        join(FRAMEWORK_DIR, "libraries")
    ],

    FLASH_EXTRA_IMAGES=[
        ("0x0000", join(FRAMEWORK_DIR, "tools", "sdk", "esp32c3", "bin", "bootloader_${BOARD_FLASH_MODE}_${__get_board_f_flash(__env__)}.bin")),
        ("0x8000", join(env.subst("$BUILD_DIR"), "partitions.bin")),
        ("0xe000", join(FRAMEWORK_DIR, "tools", "partitions", "boot_app0.bin"))
    ]
    + [
        (offset, join(FRAMEWORK_DIR, img))
        for offset, img in env.BoardConfig().get(
            "upload.arduino.flash_extra_images", []
        )
    ],
)

#
# Target: Build Core Library
#

libs = []

variants_dir = join(FRAMEWORK_DIR, "variants")

if "build.variants_dir" in env.BoardConfig():
    variants_dir = join("$PROJECT_DIR", env.BoardConfig().get("build.variants_dir"))

if "build.variant" in env.BoardConfig():
    env.Append(
        CPPPATH=[
            join(variants_dir, env.BoardConfig().get("build.variant"))
        ]
    )
    libs.append(env.BuildLibrary(
        join("$BUILD_DIR", "FrameworkArduinoVariant"),
        join(variants_dir, env.BoardConfig().get("build.variant"))
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
