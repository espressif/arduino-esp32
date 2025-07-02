import logging


def test_nvs(dut):
    LOGGER = logging.getLogger(__name__)

    LOGGER.info("Expecting default values from Preferences")
    dut.expect_exact(
        "Values from Preferences: char: A | uchar: 0 | short: 0 | ushort: 0 | int: 0 | uint: 0 | long: 0 | ulong: 0 | "
        "long64: 0 | ulong64: 0 | float: 0.00 | double: 0.00 | bool: false | str: str0 | strLen: strLen0 | "
        "struct: {id:1,val:100}"
    )

    LOGGER.info("Expecting updated preferences for the first time")
    dut.expect_exact(
        "Values from Preferences: char: B | uchar: 1 | short: 1 | ushort: 1 | int: 1 | uint: 1 | long: 1 | ulong: 1 | "
        "long64: 1 | ulong64: 1 | float: 1.10 | double: 1.10 | bool: true | str: str1 | strLen: strLen1 | "
        "struct: {id:2,val:110}"
    )

    LOGGER.info("Expecting updated preferences for the second time")
    dut.expect_exact(
        "Values from Preferences: char: C | uchar: 2 | short: 2 | ushort: 2 | int: 2 | uint: 2 | long: 2 | ulong: 2 | "
        "long64: 2 | ulong64: 2 | float: 2.20 | double: 2.20 | bool: false | str: str2 | strLen: strLen2 | "
        "struct: {id:3,val:120}"
    )
