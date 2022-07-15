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

from SCons.Script import DefaultEnvironment, SConscript

env = DefaultEnvironment()
platform = env.PioPlatform()
board_config = env.BoardConfig()
build_mcu = board_config.get("build.mcu", "").lower()

FRAMEWORK_DIR = platform.get_package_dir("framework-arduinoespressif32")
assert isdir(FRAMEWORK_DIR)


#
# Helpers
#

def get_partition_table_csv(variants_dir):
    fwpartitions_dir = join(FRAMEWORK_DIR, "tools", "partitions")

    custom_partition = board_config.get(
        "build.partitions", board_config.get("build.arduino.partitions", "")
    )

    if custom_partition:
        partitions_csv = board_config.get("build.partitions", board_config.get(
            "build.arduino.partitions", "default.csv"))
        return abspath(
            join(fwpartitions_dir, partitions_csv)
            if isfile(join(fwpartitions_dir, partitions_csv))
            else partitions_csv
        )

    variant_partitions = join(
        variants_dir, board_config.get("build.variant", ""), "partitions.csv"
    )
    return (
        variant_partitions
        if isfile(variant_partitions)
        else join(fwpartitions_dir, "default.csv")
    )


def get_bootloader_image(variants_dir):
    variant_bootloader = join(
        variants_dir, board_config.get("build.variant", ""), "bootloader.bin"
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
# Process framework extra images
#

env.Append(
    LIBSOURCE_DIRS=[
        join(FRAMEWORK_DIR, "libraries")
    ],

    FLASH_EXTRA_IMAGES=[
        (
            "0x1000" if build_mcu in ("esp32", "esp32s2") else "0x0000",
            get_bootloader_image(board_config.get(
                "build.variants_dir", join(FRAMEWORK_DIR, "variants")))
        ),
        ("0x8000", join(env.subst("$BUILD_DIR"), "partitions.bin")),
        ("0xe000", join(FRAMEWORK_DIR, "tools", "partitions", "boot_app0.bin"))
    ]
    + [
        (offset, join(FRAMEWORK_DIR, img))
        for offset, img in board_config.get(
            "upload.arduino.flash_extra_images", []
        )
    ],
)

#
# Target: Build Core Library
#

libs = []

variants_dir = join(FRAMEWORK_DIR, "variants")

if "build.variants_dir" in board_config:
    variants_dir = join("$PROJECT_DIR", board_config.get("build.variants_dir"))

if "build.variant" in board_config:
    env.Append(
        CPPPATH=[
            join(variants_dir, board_config.get("build.variant"))
        ]
    )
    env.BuildSources(
        join("$BUILD_DIR", "FrameworkArduinoVariant"),
        join(variants_dir, board_config.get("build.variant"))
    )

libs.append(env.BuildLibrary(
    join("$BUILD_DIR", "FrameworkArduino"),
    join(FRAMEWORK_DIR, "cores", board_config.get("build.core"))
))

env.Prepend(LIBS=libs)

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
