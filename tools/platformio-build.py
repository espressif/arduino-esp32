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

from SCons.Script import DefaultEnvironment, SConscript

env = DefaultEnvironment()
platform = env.PioPlatform()
board_config = env.BoardConfig()
build_mcu = board_config.get("build.mcu", "").lower()
partitions_name = board_config.get(
    "build.partitions", board_config.get("build.arduino.partitions", "")
)

FRAMEWORK_DIR = platform.get_package_dir("framework-arduinoespressif32")
assert isdir(FRAMEWORK_DIR)


#
# Helpers
#


def get_partition_table_csv(variants_dir):
    fwpartitions_dir = join(FRAMEWORK_DIR, "tools", "partitions")
    variant_partitions_dir = join(variants_dir, board_config.get("build.variant", ""))

    if partitions_name:
        # A custom partitions file is selected
        if isfile(join(variant_partitions_dir, partitions_name)):
            return join(variant_partitions_dir, partitions_name)

        return abspath(
            join(fwpartitions_dir, partitions_name)
            if isfile(join(fwpartitions_dir, partitions_name))
            else partitions_name
        )

    variant_partitions = join(variant_partitions_dir, "partitions.csv")
    return (
        variant_partitions
        if isfile(variant_partitions)
        else join(fwpartitions_dir, "default.csv")
    )


def get_bootloader_image(variants_dir):
    bootloader_image_file = "bootloader.bin"
    if partitions_name.endswith("tinyuf2.csv"):
        bootloader_image_file = "bootloader-tinyuf2.bin"

    variant_bootloader = join(
        variants_dir,
        board_config.get("build.variant", ""),
        board_config.get("build.arduino.custom_bootloader", bootloader_image_file),
    )

    return (
        variant_bootloader
        if isfile(variant_bootloader)
        else join(
            FRAMEWORK_DIR,
            "tools",
            "sdk",
            build_mcu,
            "bin",
            "bootloader_${__get_board_boot_mode(__env__)}_${__get_board_f_flash(__env__)}.bin",
        )
    )


def get_patched_bootloader_image(original_bootloader_image, bootloader_offset):
    patched_bootloader_image = join(env.subst("$BUILD_DIR"), "patched_bootloader.bin")
    bootloader_cmd = env.Command(
        patched_bootloader_image,
        original_bootloader_image,
        env.VerboseAction(
            " ".join(
                [
                    '"$PYTHONEXE"',
                    join(
                        platform.get_package_dir("tool-esptoolpy") or "", "esptool.py"
                    ),
                    "--chip",
                    build_mcu,
                    "merge_bin",
                    "-o",
                    "$TARGET",
                    "--flash_mode",
                    "${__get_board_flash_mode(__env__)}",
                    "--flash_freq",
                    "${__get_board_f_flash(__env__)}",
                    "--flash_size",
                    board_config.get("upload.flash_size", "4MB"),
                    "--target-offset",
                    bootloader_offset,
                    bootloader_offset,
                    "$SOURCE",
                ]
            ),
            "Updating bootloader headers",
        ),
    )
    env.Depends("$BUILD_DIR/$PROGNAME$PROGSUFFIX", bootloader_cmd)

    return patched_bootloader_image


def add_tinyuf2_extra_image():
    tinuf2_image = board_config.get(
        "upload.arduino.tinyuf2_image",
        join(variants_dir, board_config.get("build.variant", ""), "tinyuf2.bin"),
    )

    # Add the UF2 image only if it exists and it's not already added
    if not isfile(tinuf2_image):
        print("Warning! The `%s` UF2 bootloader image doesn't exist" % tinuf2_image)
        return

    if any(
        "tinyuf2.bin" == basename(extra_image[1])
        for extra_image in env.get("FLASH_EXTRA_IMAGES", [])
    ):
        print("Warning! An extra UF2 bootloader image is already added!")
        return

    env.Append(
        FLASH_EXTRA_IMAGES=[
            (
                board_config.get(
                    "upload.arduino.uf2_bootloader_offset",
                    (
                        "0x2d0000"
                        if env.subst("$BOARD").startswith("adafruit")
                        else "0x410000"
                    ),
                ),
                tinuf2_image,
            ),
        ]
    )


#
# Run target-specific script to populate the environment with proper build flags
#

SConscript(
    join(
        DefaultEnvironment()
        .PioPlatform()
        .get_package_dir("framework-arduinoespressif32"),
        "tools",
        "platformio-build-%s.py" % build_mcu,
    )
)

#
# Target: Build Core Library
#

libs = []

variants_dir = join(FRAMEWORK_DIR, "variants")

if "build.variants_dir" in board_config:
    variants_dir = join("$PROJECT_DIR", board_config.get("build.variants_dir"))

if "build.variant" in board_config:
    env.Append(CPPPATH=[join(variants_dir, board_config.get("build.variant"))])
    env.BuildSources(
        join("$BUILD_DIR", "FrameworkArduinoVariant"),
        join(variants_dir, board_config.get("build.variant")),
    )

libs.append(
    env.BuildLibrary(
        join("$BUILD_DIR", "FrameworkArduino"),
        join(FRAMEWORK_DIR, "cores", board_config.get("build.core")),
    )
)

env.Prepend(LIBS=libs)

#
# Process framework extra images
#

# Starting with v2.0.4 the Arduino core contains updated bootloader images that have
# innacurate default headers. This results in bootloops if firmware is flashed via
# OpenOCD (e.g. debugging or uploading via debug tools). For this reason, before
# uploading or debugging we need to adjust the bootloader binary according to
# the values of the --flash-size and --flash-mode arguments.
# Note: This behavior doesn't occur if uploading is done via esptoolpy, as esptoolpy
# overrides the binary image headers before flashing.

bootloader_patch_required = bool(
    env.get("PIOFRAMEWORK", []) == ["arduino"]
    and (
        "debug" in env.GetBuildType()
        or env.subst("$UPLOAD_PROTOCOL") in board_config.get("debug.tools", {})
        or env.IsIntegrationDump()
    )
)

bootloader_image_path = get_bootloader_image(variants_dir)
bootloader_offset = "0x1000" if build_mcu in ("esp32", "esp32s2") else "0x0000"
if bootloader_patch_required:
    bootloader_image_path = get_patched_bootloader_image(
        bootloader_image_path, bootloader_offset
    )

env.Append(
    LIBSOURCE_DIRS=[join(FRAMEWORK_DIR, "libraries")],
    FLASH_EXTRA_IMAGES=[
        (bootloader_offset, bootloader_image_path),
        ("0x8000", join(env.subst("$BUILD_DIR"), "partitions.bin")),
        ("0xe000", join(FRAMEWORK_DIR, "tools", "partitions", "boot_app0.bin")),
    ]
    + [
        (offset, join(FRAMEWORK_DIR, img))
        for offset, img in board_config.get("upload.arduino.flash_extra_images", [])
    ],
)

# Add an extra UF2 image if the 'TinyUF2' partition is selected
if partitions_name.endswith("tinyuf2.csv") or board_config.get(
    "upload.arduino.tinyuf2_image", ""
):
    add_tinyuf2_extra_image()

#
# Generate partition table
#

env.Replace(PARTITIONS_TABLE_CSV=get_partition_table_csv(variants_dir))

partition_table = env.Command(
    join("$BUILD_DIR", "partitions.bin"),
    "$PARTITIONS_TABLE_CSV",
    env.VerboseAction(
        '"$PYTHONEXE" "%s" -q $SOURCE $TARGET'
        % join(FRAMEWORK_DIR, "tools", "gen_esp32part.py"),
        "Generating partitions $TARGET",
    ),
)
env.Depends("$BUILD_DIR/$PROGNAME$PROGSUFFIX", partition_table)
