from jinja2 import Environment, FileSystemLoader


def merge(*args):
    """Utility function to combine multiple dictionaries"""
    result = dict()
    for dictionary in args:
        assert isinstance(dictionary, dict)
        result.update(dictionary)
    return result


# Default values applicable to all chips
# (can also be overridden)
global_defaults = {
    "target": "esp",
    "core": "esp32",
    "partitions": "default",
    "flash_size": "4MB",
    "cpus": 1
}

# Chip-specific defaults
chip_esp32_defaults = {
    "f_cpu": 240000000,
    "tarch": "xtensa",
    "mcu": "esp32",
    "bootloader_addr": "0x1000",
    "boot": "dio",
    "flash_mode": "dio",
    "flash_freq": "40m",
    "cpus": 2
}


chip_esp32s2_defaults = {
    "f_cpu": 240000000,
    "tarch": "xtensa",
    "mcu": "esp32s2",
    "bootloader_addr": "0x1000",
    "boot": "qio",
    "flash_mode": "qio",
    "flash_freq": "80m"
}


chip_esp32c3_defaults = {
    "f_cpu": 160000000,
    "tarch": "riscv32",
    "mcu": "esp32c3",
    "bootloader_addr": "0x0",
    "boot": "qio",
    "flash_mode": "qio",
    "flash_freq": "80m"
}

# End of chip-specific defaults. Add new chips above this line.

# Partition schemes definitions

partition_schemes = {
    "default": {
        "name": "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)",
        "flash_size_required_mb": 4,
        "maximum_size": 1310720,
    },
    "defaultffat": {
        "name": "Default 4MB with ffat (1.2MB APP/1.5MB FATFS)",
        "flash_size_required_mb": 4,
        "maximum_size": 1310720,
    },
    "default_8MB": {
        "name": "8M Flash (3MB APP/1.5MB FAT)",
        "flash_size_required_mb": 8,
        "maximum_size": 3342336
    },
    "minimal": {
        "name": "Minimal (1.3MB APP/700KB SPIFFS)",
        "flash_size_required_mb": 2,
        "maximum_size": 1310720,
    },
    "no_ota": {
        "name": "No OTA (2MB APP/2MB SPIFFS)",
        "flash_size_required_mb": 4,
        "maximum_size": 2097152,
    }
}

# These dictionaries are used to enable different menus,
# for example flash size selection

board_has_flashsize_menu = {
    "FlashSize": True
}

board_has_flashfreq_menu = {
    "FlashFreq": True
}

board_has_flashmode_menu = {
    "FlashMode": True
}

board_has_flash_menus = merge(
    board_has_flashsize_menu,
    board_has_flashfreq_menu,
    board_has_flashmode_menu
)

# Board definitions. Each board is defined by a dictionary,
# produced by a combination of global, chip-specific, and
# board-specific dictionary. Anything defined defined at
# global or chip level can be overridden in a board-specific
# dictionary.

board_esp32 = merge(
    global_defaults,
    chip_esp32_defaults,
    board_has_flash_menus,
    {
        "name": "ESP32 Dev Module",
        "board_macro": "ESP32_DEV",
        "max_partitions_size_mb": 16
    }
)


board_esp32s2 = merge(
    global_defaults,
    chip_esp32s2_defaults,
    board_has_flash_menus,
    {
        "name": "ESP32-S2 Dev Module",
        "board_macro": "ESP32S2_DEV",
        "max_partitions_size_mb": 4
    }
)


board_esp32c3 = merge(
    global_defaults,
    chip_esp32c3_defaults,
    # just an example, let's assume this board
    # doesn't have flash menus
    # board_has_flash_menus
    {
        "name": "ESP32-C3 Dev Module",
        "board_macro": "ESP32C3_DEV",
        # just an example, let's assume this board
        # has limited flash size
        "max_partitions_size_mb": 2
    }
)

# End of board definitions. When adding a new board,
# add its dictionary above and then also add it into
# the "boards" dictionary below.

boards = {
    "esp32": board_esp32,
    "esp32s2": board_esp32s2,
    "esp32c3": board_esp32c3,
}


# This function returns the context used to expand
# the template.
def get_context():
    context = {
        "boards": boards,
        "partition_schemes": partition_schemes
    }
    return context


# The main function simply loads the template and expands
# it using the context above.
def main():
    env = Environment(loader=FileSystemLoader("."))
    template = env.get_template("boards.txt.jinja")
    context = get_context()
    with open("boards.txt", "w") as out:
        out.write(template.render(context))


if __name__ == "__main__":
    main()
