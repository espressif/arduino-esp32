def test_periman(dut):
    peripherals = [
        "GPIO",
        "SigmaDelta",
        "LEDC",
        "RMT",
        "I2S",
        "I2C",
        "SPI",
        "ADC_Oneshot",
        "ADC_Continuous",
        "DAC",
        "Touch",
    ]

    pattern = rb"(?:\b\w+\b test: This should(?: not)? be printed|Peripheral Manager test done)"

    while True:
        try:
            res = dut.expect(pattern, timeout=10)
        except Exception as e:  # noqa: F841
            assert False, "Could not detect end of test"

        console_output = res.group(0).decode("utf-8")
        peripheral = console_output.split()[0]

        if "Peripheral Manager test done" in console_output:
            break

        if peripheral in peripherals:
            if "not" in console_output:
                assert False, f"Peripheral {peripheral} printed when it should not"
            peripherals.remove(peripheral)
        else:
            assert False, f"Unknown peripheral: {peripheral}"

    assert peripherals == [], f"Missing peripherals output: {peripherals}"
