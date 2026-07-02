import logging


def test_dac(dut):
    LOGGER = logging.getLogger(__name__)

    sender = dut[0]
    reader = dut[1]

    LOGGER.info("Waiting for devices to be ready...")
    sender.expect_exact("[SENDER] Ready", timeout=120)
    reader.expect_exact("[READER] Ready", timeout=120)
    sender.expect_exact("[SENDER] DAC_READY", timeout=10)

    LOGGER.info("DAC loopback: writing 0...")
    sender.write("DAC_WRITE 0")
    sender.expect_exact("[SENDER] DAC_WRITTEN 0", timeout=10)

    reader.write("ADC_READ")
    m = reader.expect(r"\[READER\] ADC_MV=(\d+)", timeout=10)
    mv_low = int(m.group(1))
    LOGGER.info(f"DAC=0 -> reader ADC: {mv_low} mV")
    assert mv_low < 500, f"Expected < 500 mV for DAC=0, got {mv_low}"

    LOGGER.info("DAC loopback: writing 255...")
    sender.write("DAC_WRITE 255")
    sender.expect_exact("[SENDER] DAC_WRITTEN 255", timeout=10)

    reader.write("ADC_READ")
    m = reader.expect(r"\[READER\] ADC_MV=(\d+)", timeout=10)
    mv_high = int(m.group(1))
    LOGGER.info(f"DAC=255 -> reader ADC: {mv_high} mV")
    assert mv_high > 2000, f"Expected > 2000 mV for DAC=255, got {mv_high}"

    LOGGER.info("DAC loopback: disabling DAC...")
    sender.write("DAC_DISABLE")
    sender.expect_exact("[SENDER] DAC_DISABLED", timeout=10)

    sender.write("DONE")
    sender.expect_exact("[SENDER] DONE", timeout=10)
    reader.write("DONE")
    reader.expect_exact("[READER] DONE", timeout=10)

    LOGGER.info("DAC loopback test passed!")
