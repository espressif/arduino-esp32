import logging

capabilities = {
    "timer": ["esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6", "esp32h2"],
    "touchpad": ["esp32", "esp32s2", "esp32s3"],
    "rtc_io": ["esp32", "esp32s2", "esp32s3", "esp32c6"],
    "rtc_cntl": ["esp32", "esp32s2", "esp32s3", "esp32c6"],
    "gpio": ["esp32", "esp32s2", "esp32s3"],
    "uart": ["esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6", "esp32h2"]
}

def test_sleep(dut):
    LOGGER = logging.getLogger(__name__)

    # Deep Sleep
    boot_count = 1
    dut.expect_exact("Boot number: {}".format(boot_count))
    dut.expect_exact("Wakeup reason: power_up")

    for capability, devices in capabilities.items():
        if dut.app.target in devices and capability not in ["gpio", "uart"]:
            LOGGER.info("Testing {} deep sleep capability".format(capability))
            boot_count += 1
            dut.write("{}_deep".format(capability))
            dut.expect_exact("Boot number: {}".format(boot_count))
            dut.expect_exact("Wakeup reason: {}".format(capability))

    # Light Sleep
    for capability, devices in capabilities.items():
        if dut.app.target in devices:
            LOGGER.info("Testing {} light sleep capability".format(capability))
            dut.write("{}_light".format(capability))
            if capability == "uart":
                dut.write("aaa") # Send 9 positive edges
            dut.expect_exact("Woke up from light sleep")
            dut.expect_exact("Wakeup reason: {}".format(capability))
            if capability == "timer":
                LOGGER.info("Testing timer light sleep capability with low frequency")
                dut.write("timer_freq_light")
                dut.expect_exact("Woke up from light sleep")
                dut.expect_exact("Wakeup reason: timer")
