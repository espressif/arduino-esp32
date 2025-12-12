import logging


def test_periman(dut):
    LOGGER = logging.getLogger(__name__)

    # Define peripherals and whether they have a manual deinit test
    # Format: (name, has_deinit_test)
    PERIPHERALS = [
        ("GPIO", False),
        ("SigmaDelta", True),
        ("LEDC", True),
        ("RMT", True),
        ("I2S", True),
        ("I2C", True),
        ("SPI", True),
        ("ADC_Oneshot", False),
        ("ADC_Continuous", True),
        ("DAC", False),
        ("Touch", False),
    ]

    # Build expected test names
    pending_tests = set()
    for name, has_deinit in PERIPHERALS:
        pending_tests.add(name)
        if has_deinit:
            pending_tests.add(f"{name}_deinit")

    pattern = rb"(?:\b[\w_]+\b test: This should(?: not)? be printed|Peripheral Manager test done)"

    while True:
        try:
            res = dut.expect(pattern, timeout=10)
        except Exception as e:  # noqa: F841
            assert False, "Could not detect end of test"

        console_output = res.group(0).decode("utf-8")
        test_name = console_output.split()[0]

        if "Peripheral Manager test done" in console_output:
            break

        if test_name in pending_tests:
            if "not" in console_output:
                assert False, f"Output printed when it should not for test: {test_name}"
            else:
                test_type = "manual deinit" if test_name.endswith("_deinit") else "auto-detach"
                LOGGER.info(f"✓ {test_type} works for: {test_name}")
                pending_tests.remove(test_name)
        else:
            assert False, f"Unknown test: {test_name}"

    assert not pending_tests, f"Missing tests: {sorted(pending_tests)}"
